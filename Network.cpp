#include "Network.h"
#include <QDataStream>
#include <QNetworkInterface>
#include <QStringList>

Network::Network(QObject *parent) : QObject(parent)
{
	init();
}

Network::~Network()
{
    
}

void Network::init()
{
	m_isHost = false;
	m_tcpPort = 50010;
	m_udpPort = 50050;

	QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
	for (int i = 0; i < addresses.size(); i++)
	{
		QHostAddress addr = addresses.at(i);
		if (addr.toString().startsWith("192.168.18."))
		{
			m_myAddress = addr;
			break;
		}
	}

    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, SIGNAL(newConnection()), this, SLOT(onClientArrives()));
	m_tcpServer->listen(QHostAddress::Any, m_tcpPort);
    
    m_mySocket = new QTcpSocket(this);
    connect(m_mySocket, SIGNAL(readyRead()), this, SLOT(onTcpRead()));
    
    m_udpSocket = new QUdpSocket(this);
	m_udpSocket->bind(QHostAddress::Any, m_udpPort, QUdpSocket::ShareAddress);
    connect(m_udpSocket, SIGNAL(readyRead()), this, SLOT(onUdpRead()));

	m_clientSocket = NULL;
}

void Network::onClientArrives()
{
	m_isHost = true;

    m_clientSocket = m_tcpServer->nextPendingConnection();
    connect(m_clientSocket, SIGNAL(readyRead()), this, SLOT(onTcpRead()));

	emit otherConnected();
}

void Network::onTcpRead()
{
	QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
	QByteArray data = socket->readAll();

	QDataStream ds(data);
	QByteArray content;
	ds >> content;
	if (content == "start")
	{
		Qizi qizi;
		bool firstDrop;
		ds >> (int&)qizi >> firstDrop;
		emit joinGame(qizi, firstDrop);
	}
	else if (content == "drop")
	{
		Step step;
		ds >> step.pos.column >> step.pos.row >> (int&)step.qizi;
		emit otherDrops(step);
	}
	else if (content == "regret")
	{
		emit otherAsksRegret();
	}
	else if (content == "agree regret")
	{
		emit otherRespondsRegret(true);
	}
	else if (content == "disagree regret")
	{
		emit otherRespondsRegret(false);
	}
}

void Network::onUdpRead()
{
	if (m_udpSocket->hasPendingDatagrams())
	{
		QByteArray data;
		data.resize(m_udpSocket->pendingDatagramSize());
		QHostAddress senderAddr;
		quint16 senderPort;
		m_udpSocket->readDatagram(data.data(), data.size(), &senderAddr, &senderPort);

		if (senderAddr == m_myAddress)
			return;

		QDataStream ds(data);
		QByteArray content;
		ds >> content;
		if (content == "host")
			emit receiveHost(senderAddr);
	}
}

void Network::broadcastToHost()
{
	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << QByteArray("host");

	QStringList ipSection = m_myAddress.toString().split(".");
	QHostAddress broadcastAddr = QHostAddress(
		QString("%1.%2.%3.255")
		.arg(ipSection.at(0))
		.arg(ipSection.at(1))
		.arg(ipSection.at(2)) );

	m_udpSocket->writeDatagram(data, broadcastAddr, m_udpPort);
}

void Network::join(QHostAddress addr)
{
	m_mySocket->connectToHost(addr, m_tcpPort);
	m_mySocket->waitForConnected(1000);
}

void Network::askRegret()
{
	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << QByteArray("regret");

	if (m_isHost)
		m_clientSocket->write(data);
	else
		m_mySocket->write(data);
}

void Network::respondRegret(bool agree)
{
	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);

	if (agree)
		ds << QByteArray("agree regret");
	else
		ds << QByteArray("disagree regret");

	if (m_isHost)
		m_clientSocket->write(data);
	else
		m_mySocket->write(data);
}

void Network::notifyStart(Qizi otherQizi, bool firstDrop)
{
	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << QByteArray("start") << (int)otherQizi << firstDrop;
	m_clientSocket->write(data);
}

void Network::notifyDrop(Step step)
{
	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << QByteArray("drop") << step.pos.column << step.pos.row << (int)step.qizi;

	if (m_isHost)
		m_clientSocket->write(data);
	else
		m_mySocket->write(data);
}

void Network::close()
{
	m_mySocket->close();

	if (m_clientSocket)
	{
		m_clientSocket->close();
		m_clientSocket->deleteLater();
		m_clientSocket = NULL;
	}
	
	
}