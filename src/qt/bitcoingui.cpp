// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The PIVX developers
// Copyright (c) 2018-2019 The AXEL Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoingui.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "miner.h"
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "rpcconsole.h"
#include "utilitydialog.h"
#include "toolspage.h"

#ifdef ENABLE_WALLET
#include "blockexplorer.h"
#include "walletframe.h"
#include "walletmodel.h"
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "init.h"
#include "masternodelist.h"
#include "ui_interface.h"
#include "util.h"
#include "qssloader.h"
#include "urlrequest.h"

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QIcon>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QPalette>
#include <QPixmap>
#include <QFontDatabase>
#include <QFileInfo>
#include <QDebug>

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>

#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const QString BitcoinGUI::DEFAULT_WALLET = "~Default";
static int statusIconSize = STATUSBARTOP_ICONSIZE;

BitcoinGUI::BitcoinGUI(const NetworkStyle* networkStyle, QWidget* parent) : QMainWindow(parent),
                                                                            clientModel(0),
                                                                            walletFrame(0),
                                                                            labelStakingIcon(0),
                                                                            labelEncryptionIcon(0),
                                                                            labelConnectionsIcon(0),
                                                                            labelBlocksIcon(0),
                                                                            progressBarLabel(0),
                                                                            progressBar(0),
                                                                            progressDialog(0),
                                                                            appMenuBar(0),
                                                                            overviewAction(0),
                                                                            historyAction(0),
                                                                            masternodeAction(0),
                                                                            quitAction(0),
                                                                            sendCoinsAction(0),
                                                                            usedSendingAddressesAction(0),
                                                                            usedReceivingAddressesAction(0),
                                                                            signMessageAction(0),
                                                                            verifyMessageAction(0),
                                                                            bip38ToolAction(0),
                                                                            multisigCreateAction(0),
                                                                            multisigSpendAction(0),
                                                                            multisigSignAction(0),
                                                                            aboutAction(0),
                                                                            receiveCoinsAction(0),
                                                                            optionsAction(0),
                                                                            toggleHideAction(0),
                                                                            encryptWalletAction(0),
                                                                            backupWalletAction(0),
                                                                            changePassphraseAction(0),
                                                                            aboutQtAction(0),
                                                                            openRPCConsoleAction(0),
                                                                            openAction(0),
                                                                            showHelpMessageAction(0),
                                                                            multiSendAction(0),
                                                                            trayIcon(0),
                                                                            trayIconMenu(0),
                                                                            notificator(0),
                                                                            rpcConsole(0),
                                                                            explorerWindow(0),
                                                                            prevBlocks(0),
                                                                            spinnerFrame(0)
{
/* Open CSS when configured */
this->setStyleSheet(GUIUtil::loadStyleSheet());

GUIUtil::restoreWindowGeometry("nWindow", QSize(1065, 670), this);
//this->setFixedSize(1065,670);
QFontDatabase::addApplicationFont(":/fonts/axel_font");

QString windowTitle = tr("AXEL") + " - ";
#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !GetBoolArg("-disablewallet", false);
#else
    enableWallet = false;
#endif // ENABLE_WALLET
    if (enableWallet) {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    QString userWindowTitle = QString::fromStdString(GetArg("-windowtitle", ""));
    if (!userWindowTitle.isEmpty()) windowTitle += " - " + userWindowTitle;
    windowTitle += " " + networkStyle->getTitleAddText();
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getAppIcon());
    setWindowIcon(networkStyle->getAppIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

     rpcConsole = new RPCConsole(enableWallet ? this : 0);

#ifdef ENABLE_WALLET
    if (enableWallet) {
        /** Create wallet frame*/
        walletFrame = new WalletFrame(this);
        explorerWindow = new BlockExplorer();
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    // Accept D&D of URIs
    setAcceptDrops(true);

    QSettings settings;

    // Status bar notification icons
    QFrame* frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0, 0, 0, 0);
    frameBlocks->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    QHBoxLayout* frameBlocksLayout = new QHBoxLayout(frameBlocks);
    if (settings.value("toolbarPosition", "Top").toString() == "Top") {
        frameBlocksLayout->setSpacing(9);
        frameBlocksLayout->setContentsMargins(3, 0, 3, 0);
    } else {
        frameBlocksLayout->setSpacing(4);
        frameBlocksLayout->setContentsMargins(3, 0, 13, 0);
    }

    labelStakingIcon = new QLabel();

    if (settings.value("toolbarPosition", "Top").toString() == "Left")
        statusIconSize = STATUSBAR_ICONSIZE;

    labelEncryptionIcon = new QPushButton();
    labelEncryptionIcon->setFlat(true); // Make the button look like a label, but clickable
    labelEncryptionIcon->setObjectName(QStringLiteral("EncryptionIcon"));
    labelEncryptionIcon->setFixedSize(statusIconSize,statusIconSize);
    labelEncryptionIcon->setIconSize(QSize(statusIconSize,statusIconSize));
    labelEncryptionIcon->setCursor(Qt::PointingHandCursor);

    labelConnectionsIcon = new QPushButton();
    labelConnectionsIcon->setFlat(true); // Make the button look like a label, but clickable
    labelConnectionsIcon->setObjectName(QStringLiteral("ConnectionsIcon"));
    labelConnectionsIcon->setFixedSize(statusIconSize,statusIconSize);
    labelConnectionsIcon->setIconSize(QSize(statusIconSize,statusIconSize));
    labelConnectionsIcon->setCursor(Qt::PointingHandCursor);

    labelBlocksIcon = new QLabel();
    labelBlocksIcon->setFixedSize(statusIconSize,statusIconSize);

    if (enableWallet) {
        frameBlocksLayout->addWidget(labelEncryptionIcon);
    }

    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->setAlignment(Qt::AlignRight);

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setObjectName(QStringLiteral("progressBarLabel"));
    progressBarLabel->setVisible(true);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setVisible(true);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html

    QString curStyle = QApplication::style()->metaObject()->className();
    if (curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle") {
        progressBar->setStyleSheet("QProgressBar { background-color: #F8F8F8; border: 1px solid black; border-radius: 0px; padding: 1px; text-align: center; }  QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #00CCFF, stop: 1 #33CCFF); border-radius: 0px; margin: 0px; }");
    }

    //Put elements on bottom status bar
    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);

    // Status bar notification icons if left toolbar
    if (settings.value("toolbarPosition", "Top").toString() == "Left") {
        statusBar()->addPermanentWidget(frameBlocks);
    }

    statusBar()->setSizeGripEnabled(false);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initializeda
    createActions(networkStyle);

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars(frameBlocks);

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    //create sync anymation
    synctimer  = new QTimer(this);
    connect(synctimer, SIGNAL(timeout()), this, SLOT(updateSyncAnimation()));
    synctimer->start(50);

    // Jump directly to tabs in Toolpage
    connect(openInfoAction, SIGNAL(triggered()), this, SLOT(showInfo()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showConsole()));
    connect(openNetworkAction, SIGNAL(triggered()), this, SLOT(showGraph()));
    connect(openPeersAction, SIGNAL(triggered()), this, SLOT(showPeers()));
    connect(openRepairAction, SIGNAL(triggered()), this, SLOT(showRepair()));
/*
    connect(openInfoAction, SIGNAL(triggered()), rpcConsole, SLOT(showInfo()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(showConsole()));
    connect(openNetworkAction, SIGNAL(triggered()), rpcConsole, SLOT(showNetwork()));
    connect(openPeersAction, SIGNAL(triggered()), rpcConsole, SLOT(showPeers()));
    connect(openRepairAction, SIGNAL(triggered()), rpcConsole, SLOT(showRepair()));
*/
    connect(openConfEditorAction, SIGNAL(triggered()), rpcConsole, SLOT(showConfEditor()));
    connect(openMNConfEditorAction, SIGNAL(triggered()), rpcConsole, SLOT(showMNConfEditor()));
    connect(showBackupsAction, SIGNAL(triggered()), rpcConsole, SLOT(showBackups()));
    connect(labelConnectionsIcon, SIGNAL(clicked()), this, SLOT(showPeers()));


    // Get restart command-line parameters and handle restart
    connect(rpcConsole, SIGNAL(handleRestart(QStringList)), this, SLOT(handleRestart(QStringList)));

    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

    connect(openBlockExplorerAction, SIGNAL(triggered()), explorerWindow, SLOT(show()));
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), explorerWindow, SLOT(hide()));

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

   //will be activate when pow ends

   labelStakingIcon->hide();
   QTimer* timerStakingIcon = new QTimer(labelStakingIcon);
   connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(setStakingStatus()));
   timerStakingIcon->start(10000);
   setStakingStatus();

}

BitcoinGUI::~BitcoinGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if (trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
//    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif
}

void BitcoinGUI::createActions(const NetworkStyle* networkStyle)
{
    QActionGroup* tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/overview")), "", this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
#ifdef Q_OS_MAC
    overviewAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_1));
#else
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
#endif
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/send")), "", this);
    sendCoinsAction->setStatusTip(tr("Send coins to a AXEL address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
#ifdef Q_OS_MAC
    sendCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_2));
#else
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
#endif
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/receiving_addresses")), "", this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and AXEL: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
#ifdef Q_OS_MAC
    receiveCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_3));
#else
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
#endif
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/history")), "", this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
#ifdef Q_OS_MAC
    historyAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_4));
#else
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
#endif
    tabGroup->addAction(historyAction);

    toolsAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/tools")), "", this);
    toolsAction->setStatusTip(tr("Tools"));
    toolsAction->setToolTip(toolsAction->statusTip());
    toolsAction->setCheckable(true);
#ifdef Q_OS_MAC
    toolsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_7));
#else
    toolsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
#endif
    tabGroup->addAction(toolsAction);

#ifdef ENABLE_WALLET

    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/masternodes")), "", this);
        masternodeAction->setStatusTip(tr("Browse masternodes"));
        masternodeAction->setToolTip(masternodeAction->statusTip());
        masternodeAction->setCheckable(true);
#ifdef Q_OS_MAC
        masternodeAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_6));
#else
        masternodeAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
#endif
        tabGroup->addAction(masternodeAction);
        connect(masternodeAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
        connect(masternodeAction, SIGNAL(triggered()), this, SLOT(gotoMasternodePage()));
    }

// --- explorer menu tab ---
/*
    openBlockExplorerAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/blockexplorer")), "", this);
    openBlockExplorerAction->setStatusTip(tr("Blockchain explorer"));
    openBlockExplorerAction->setToolTip(openBlockExplorerAction->statusTip());
    openBlockExplorerAction->setCheckable(true);
#ifdef Q_OS_MAC
    openBlockExplorerAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_8));
#else
    openBlockExplorerAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_8));
#endif
    tabGroup->addAction(openBlockExplorerAction);
    connect(openBlockExplorerAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(openBlockExplorerAction, SIGNAL(triggered()), this, SLOT(gotoBlockExplorerPage()));
*/
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(toolsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(toolsAction, SIGNAL(triggered()), this, SLOT(gotoToolsPage()));

#endif // ENABLE_WALLET

    quitAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/quit")), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(networkStyle->getAppIcon(), tr("&About AXEL wallet"), this);
    aboutAction->setStatusTip(tr("Show information about AXEL wallet"));
    aboutAction->setMenuRole(QAction::AboutRole);
#if QT_VERSION < 0x050000
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#else
    aboutQtAction = new QAction(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#endif
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/options")), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for AXEL wallet"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(networkStyle->getAppIcon(), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/lock_closed")), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/filesave")), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/key")), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(tr("&Unlock Wallet..."), this);
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    lockWalletAction = new QAction(tr("&Lock Wallet"), this);
    signMessageAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/edit")), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your AXEL addresses to prove you own them"));
    verifyMessageAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/transaction_0")), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified AXEL addresses"));
    bip38ToolAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/key")), tr("&BIP38 tool"), this);
    bip38ToolAction->setToolTip(tr("Encrypt and decrypt private keys using a passphrase"));
    multiSendAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/edit")), tr("&MultiSend"), this);
    multiSendAction->setToolTip(tr("MultiSend Settings"));
    multiSendAction->setCheckable(true);

    openRPCConsoleAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/debugwindow")), tr("&Debug console"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging console"));

    openBlockExplorerAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/blockexplorer")), tr("Blockchain explorer"), this);
    openBlockExplorerAction->setStatusTip(tr("Open inWallet Blockchain explorer"));

    openInfoAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Information"), this);
    openInfoAction->setStatusTip(tr("Show diagnostic information"));
    openRPCConsoleAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/debugwindow")), tr("&Debug console"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging console"));
    openNetworkAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/connect_4")), tr("&Network Monitor"), this);
    openNetworkAction->setStatusTip(tr("Show network monitor"));
    openPeersAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/connect_4")), tr("&Peers list"), this);
    openPeersAction->setStatusTip(tr("Show peers info"));
    openRepairAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/options")), tr("Wallet &Repair"), this);
    openRepairAction->setStatusTip(tr("Show wallet repair options"));
    openConfEditorAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/edit")), tr("Open Wallet &Configuration File"), this);
    openConfEditorAction->setStatusTip(tr("Open configuration file"));
    openMNConfEditorAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/edit")), tr("Open &Masternode Configuration File"), this);
    openMNConfEditorAction->setStatusTip(tr("Open Masternode configuration file"));
    showBackupsAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/browse")), tr("Show Automatic &Backups"), this);
    showBackupsAction->setStatusTip(tr("Show automatically created wallet backups"));

    usedSendingAddressesAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/address-book")), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/address-book")), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    multisigCreateAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/address-book")), tr("&Multisignature creation..."), this);
    multisigCreateAction->setStatusTip(tr("Create a new multisignature address and add it to this wallet"));
    multisigSpendAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/send")), tr("&Multisignature spending..."), this);
    multisigSpendAction->setStatusTip(tr("Spend from a multisignature address"));
    multisigSignAction = new QAction(QIcon(GUIUtil::getThemeImage(":/icons/editpaste")), tr("&Multisignature signing..."), this);
    multisigSignAction->setStatusTip(tr("Sign with a multisignature address"));

    openAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileIcon), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a AXEL: URI or payment request"));

    showHelpMessageAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the AXEL help message to get a list with possible AXEL command-line options"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
#ifdef ENABLE_WALLET
    if (walletFrame) {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
        connect(unlockWalletAction, SIGNAL(triggered()), walletFrame, SLOT(unlockWallet()));
        connect(lockWalletAction, SIGNAL(triggered()), walletFrame, SLOT(lockWallet()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(bip38ToolAction, SIGNAL(triggered()), this, SLOT(gotoBip38Tool()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
        connect(multiSendAction, SIGNAL(triggered()), this, SLOT(gotoMultiSendDialog()));
        connect(multisigCreateAction, SIGNAL(triggered()), this, SLOT(gotoMultisigCreate()));
        connect(multisigSpendAction, SIGNAL(triggered()), this, SLOT(gotoMultisigSpend()));
        connect(multisigSignAction, SIGNAL(triggered()), this, SLOT(gotoMultisigSign()));
    }
#endif // ENABLE_WALLET
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
//    appMenuBar = new QMenuBar();
    appMenuBar = menuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu* file = appMenuBar->addMenu(tr("&File"));
    if (walletFrame) {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
        file->addAction(multisigCreateAction);
        file->addAction(multisigSpendAction);
        file->addAction(multisigSignAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu* settings = appMenuBar->addMenu(tr("&Settings"));
    if (walletFrame) {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addAction(unlockWalletAction);
        settings->addAction(lockWalletAction);
        settings->addAction(bip38ToolAction);
        settings->addAction(multiSendAction);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);

    if (walletFrame) {
        QMenu* tools = appMenuBar->addMenu(tr("&Tools"));

        tools->addAction(openInfoAction);
        tools->addAction(openRPCConsoleAction);
        tools->addAction(openNetworkAction);
        tools->addAction(openPeersAction);
        tools->addAction(openRepairAction);
        tools->addSeparator();
        tools->addAction(openBlockExplorerAction);
        tools->addSeparator();

        tools->addAction(openConfEditorAction);
        tools->addAction(openMNConfEditorAction);
        tools->addAction(showBackupsAction);
      }

    QMenu* help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void BitcoinGUI::createToolBars(QWidget *frameBlocks)
{
    if (walletFrame) {
        QSettings settings;
        QToolBar* toolbar = new QToolBar(tr("Tabs toolbar"));
        toolbar->setIconSize(QSize(35,35));
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

        QWidget* hspacer = new QWidget;
        hspacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(historyAction);

        if (settings.value("fShowMasternodesTab").toBool()) {
            toolbar->addAction(masternodeAction);
        }
        toolbar->addAction(toolsAction);
//        toolbar->addAction(openBlockExplorerAction);

        if (settings.value("toolbarPosition", "Top").toString() == "Top") {
            toolbar->addWidget(hspacer);
            toolbar->addWidget(frameBlocks);
            toolbar->setObjectName("top_ToolBar");
//            toolbar->setOrientation(Qt::Horizontal);
        } else {
            toolbar->setObjectName("left_ToolBar");
            toolbar->setOrientation(Qt::Vertical);
        }
        toolbar->setMovable(false); // remove unused icon in upper left corner
        overviewAction->setChecked(true);

        /** Create additional container for toolbar and walletFrame and make it the central widget.
            This is a workaround mostly for toolbar styling on Mac OS but should work fine for every other OSes too.
        */
        QWidget* containerWidget = new QWidget();
        if (settings.value("toolbarPosition", "Top").toString() == "Top") {
            QVBoxLayout* layout = new QVBoxLayout;
            layout->setSpacing(0);
            layout->setContentsMargins(QMargins());
            layout->addWidget(toolbar);
            layout->setAlignment(toolbar, Qt::AlignBottom);
            layout->addWidget(walletFrame);
            containerWidget->setLayout(layout);
        } else {
            QHBoxLayout* layout = new QHBoxLayout;
            layout->setSpacing(0);
            layout->setContentsMargins(QMargins());
            layout->addWidget(toolbar);
            layout->addWidget(walletFrame);
            containerWidget->setLayout(layout);
        }
        setCentralWidget(containerWidget);

    }
}

void BitcoinGUI::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;
    if (clientModel) {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));

        // Receive and report messages from client model
        connect(clientModel, SIGNAL(message(QString, QString, unsigned int)), this, SLOT(message(QString, QString, unsigned int)));

        // Show progress dialog
        connect(clientModel, SIGNAL(showProgress(QString, int)), this, SLOT(showProgress(QString, int)));

        rpcConsole->setClientModel(clientModel);
#ifdef ENABLE_WALLET
        if (walletFrame) {
            walletFrame->setClientModel(clientModel);
        }
#endif // ENABLE_WALLET

    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if (trayIconMenu) {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
    }
}

#ifdef ENABLE_WALLET
bool BitcoinGUI::addWallet(const QString& name, WalletModel* walletModel)
{
    if (!walletFrame)
        return false;
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool BitcoinGUI::setCurrentWallet(const QString& name)
{
    if (!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void BitcoinGUI::removeAllWallets()
{
    if (!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void BitcoinGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeAction->setEnabled(enabled);
    }

    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    multisigCreateAction->setEnabled(enabled);
    multisigSpendAction->setEnabled(enabled);
    multisigSignAction->setEnabled(enabled);
    bip38ToolAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void BitcoinGUI::createTrayIcon(const NetworkStyle* networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("AXEL client") + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getAppIcon());
    trayIcon->show();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void BitcoinGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
        this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow*)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu

    trayIconMenu->addAction(toggleHideAction);
/*
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addAction(bip38ToolAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(openInfoAction);
    trayIconMenu->addAction(openRPCConsoleAction);
    trayIconMenu->addAction(openNetworkAction);
    trayIconMenu->addAction(openPeersAction);
    trayIconMenu->addAction(openRepairAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(openConfEditorAction);
    trayIconMenu->addAction(openMNConfEditorAction);
    trayIconMenu->addAction(showBackupsAction);
    trayIconMenu->addAction(openBlockExplorerAction);
*/
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if (!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    if (!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void BitcoinGUI::showHelpMessageClicked()
{
    HelpMessageDialog* help = new HelpMessageDialog(this, false);
    help->setAttribute(Qt::WA_DeleteOnClose);
    help->show();
}

#ifdef ENABLE_WALLET
void BitcoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if (dlg.exec()) {
        emit receivedURI(dlg.getURI());
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void BitcoinGUI::gotoMasternodePage()
{
    QSettings settings;
    if (settings.value("fShowMasternodesTab").toBool()) {
        masternodeAction->setChecked(true);
        if (walletFrame) walletFrame->gotoMasternodePage();
    }
}



void BitcoinGUI::gotoToolsPage()
{
    toolsAction->setChecked(true);
    if(toolsAction) walletFrame->gotoToolsPage();
}

void BitcoinGUI::showInfo()
{
    toolsAction->setChecked(true);
    walletFrame->gotoToolsPageTab(ToolsPage::TAB_INFO);
}

void BitcoinGUI::showConsole()
{
    toolsAction->setChecked(true);
    walletFrame->gotoToolsPageTab(ToolsPage::TAB_CONSOLE);
}

void BitcoinGUI::showGraph()
{
    toolsAction->setChecked(true);
    walletFrame->gotoToolsPageTab(ToolsPage::TAB_GRAPH);
}

void BitcoinGUI::showPeers()
{
    toolsAction->setChecked(true);
    walletFrame->gotoToolsPageTab(ToolsPage::TAB_PEERS);
}

void BitcoinGUI::showRepair()
{
    toolsAction->setChecked(true);
    walletFrame->gotoToolsPageTab(ToolsPage::TAB_REPAIR);
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void BitcoinGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}

void BitcoinGUI::gotoMultisigCreate()
{
    if(walletFrame) walletFrame->gotoMultisigDialog(0);
}

void BitcoinGUI::gotoMultisigSpend()
{
    if(walletFrame) walletFrame->gotoMultisigDialog(1);
}

void BitcoinGUI::gotoMultisigSign()
{
    if(walletFrame) walletFrame->gotoMultisigDialog(2);
}

void BitcoinGUI::gotoBip38Tool()
{
    if (walletFrame) walletFrame->gotoBip38Tool();
}

void BitcoinGUI::gotoMultiSendDialog()
{
    multiSendAction->setChecked(true);
    if (walletFrame)
        walletFrame->gotoMultiSendDialog();
}

#endif // ENABLE_WALLET

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch (count) {
    case 0:
        icon = GUIUtil::getThemeImage(":/icons/connect_0");
        break;
    case 1:
    case 2:
    case 3:
        icon = GUIUtil::getThemeImage(":/icons/connect_1");
        break;
    case 4:
    case 5:
    case 6:
        icon = GUIUtil::getThemeImage(":/icons/connect_2");
        break;
    case 7:
    case 8:
    case 9:
        icon = GUIUtil::getThemeImage(":/icons/connect_3");
        break;
    default:
        icon = GUIUtil::getThemeImage(":/icons/connect_4");
        break;
    }
    QIcon connectionItem = QIcon(icon).pixmap(32, 32);
    labelConnectionsIcon->setIcon(connectionItem);
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to AXEL network", "", count));
}

void BitcoinGUI::updateSyncAnimation()
{
if (!masternodeSync.IsBlockchainSynced() || !masternodeSync.IsSynced())
{
 labelBlocksIcon->setPixmap(QIcon( GUIUtil::getThemeImage(QString(":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0'))) )
                                           .pixmap(statusIconSize, statusIconSize));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
}
else   disconnect(synctimer, SIGNAL(timeout()), 0, 0);
}

void BitcoinGUI::setNumBlocks(int count)
{
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbelled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
    case BLOCK_SOURCE_NETWORK:
        progressBarLabel->setText(tr("Synchronizing with network..."));
        break;
    case BLOCK_SOURCE_DISK:
        progressBarLabel->setText(tr("Importing blocks from disk..."));
        break;
    case BLOCK_SOURCE_REINDEX:
        progressBarLabel->setText(tr("Reindexing blocks on disk..."));
        break;
    case BLOCK_SOURCE_NONE:
        // Case: not Importing, not Reindexing and no network connection
        progressBarLabel->setText(tr("No block source available..."));
        break;
    }

    QString tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = lastBlockDate.secsTo(currentDate);

    tooltip = tr("Processed %n blocks of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    //    if(secs < 25*60) // 90*60 for bitcoin but we are 4x times faster
    if (masternodeSync.IsBlockchainSynced()) {
        QString strSyncStatus;
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;

        if (masternodeSync.IsSynced()) {
            progressBarLabel->setVisible(false);
            progressBar->setVisible(false);
            labelBlocksIcon->setPixmap(QIcon(GUIUtil::getThemeImage(":/icons/synced")).pixmap(statusIconSize, statusIconSize));
        } else {
            int nAttempt;
            int progress = 0;

#ifdef ENABLE_WALLET
            if (walletFrame)
                walletFrame->showOutOfSyncWarning(false);
#endif // ENABLE_WALLET

            nAttempt = masternodeSync.RequestedMasternodeAttempt < MASTERNODE_SYNC_THRESHOLD ?
                           masternodeSync.RequestedMasternodeAttempt + 1 :
                           MASTERNODE_SYNC_THRESHOLD;
            progress = nAttempt + (masternodeSync.RequestedMasternodeAssets - 1) * MASTERNODE_SYNC_THRESHOLD;
            progressBar->setMaximum(4 * MASTERNODE_SYNC_THRESHOLD);
            progressBar->setFormat(tr("Synchronizing additional data: %p%"));
            progressBar->setValue(progress);
        }

        strSyncStatus = QString(masternodeSync.GetSyncStatus().c_str());
        progressBarLabel->setText(strSyncStatus);
        tooltip = strSyncStatus + QString("<br>") + tooltip;
    } else {
        // Represent time from last generated block in human readable text
        QString timeBehindText;
        const int HOUR_IN_SECONDS = 60 * 60;
        const int DAY_IN_SECONDS = 24 * 60 * 60;
        const int WEEK_IN_SECONDS = 7 * 24 * 60 * 60;
        const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
        if (secs < 2 * DAY_IN_SECONDS) {
            timeBehindText = tr("%n hour(s)", "", secs / HOUR_IN_SECONDS);
        } else if (secs < 2 * WEEK_IN_SECONDS) {
            timeBehindText = tr("%n day(s)", "", secs / DAY_IN_SECONDS);
        } else if (secs < YEAR_IN_SECONDS) {
            timeBehindText = tr("%n week(s)", "", secs / WEEK_IN_SECONDS);
        } else {
            int years = secs / YEAR_IN_SECONDS;
            int remainder = secs % YEAR_IN_SECONDS;
            timeBehindText = tr("%1 and %2").arg(tr("%n year(s)", "", years)).arg(tr("%n week(s)", "", remainder / WEEK_IN_SECONDS));
        }

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind. Scanning block %2").arg(timeBehindText).arg(count));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(clientModel->getVerificationProgress() * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;

        prevBlocks = count;

#ifdef ENABLE_WALLET
        if (walletFrame)
            walletFrame->showOutOfSyncWarning(true);
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret)
{
    QString strTitle = tr("AXEL"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    } else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "AXEL - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons, this);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    } else
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
}

void BitcoinGUI::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent* event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if (clientModel && clientModel->getOptionsModel()) {
        if (!clientModel->getOptionsModel()->getMinimizeOnClose()) {
            QApplication::quit();
        }
    }
#endif
    QMainWindow::closeEvent(event);
}

#ifdef ENABLE_WALLET

void BitcoinGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address)
{
    // On new transaction, make an info balloon
    message((amount) < 0 ? (pwalletMain->fMultiSendNotify == true ? tr("Sent MultiSend transaction") : tr("Sent transaction")) : tr("Incoming transaction"),
        tr("Date: %1\n"
           "Amount: %2\n"
           "Type: %3\n"
           "Address: %4\n")
            .arg(date)
            .arg(BitcoinUnits::formatWithUnit(unit, amount, true))
            .arg(type)
            .arg(address),
        CClientUIInterface::MSG_INFORMATION);

    pwalletMain->fMultiSendNotify = false;
}
#endif // ENABLE_WALLET

void BitcoinGUI::dragEnterEvent(QDragEnterEvent* event)
{
    // Accept only URIs
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        foreach (const QUrl& uri, event->mimeData()->urls()) {
            emit receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool BitcoinGUI::eventFilter(QObject* object, QEvent* event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip) {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

void BitcoinGUI::setStakingStatus()
{
    if (pwalletMain)
        fMultiSend = pwalletMain->isMultiSendEnabled();

    if (nLastCoinStakeSearchInterval) {
        labelStakingIcon->show();
        labelStakingIcon->setPixmap(QIcon(GUIUtil::getThemeImage(":/icons/staking_active")).pixmap(statusIconSize, statusIconSize));
        labelStakingIcon->setToolTip(tr("Staking is active\n MultiSend: %1").arg(fMultiSend ? tr("Active") : tr("Not Active")));
    } else {
        labelStakingIcon->show();
        labelStakingIcon->setPixmap(QIcon(GUIUtil::getThemeImage(":/icons/staking_inactive")).pixmap(statusIconSize, statusIconSize));
        labelStakingIcon->setToolTip(tr("Staking is not active\n MultiSend: %1").arg(fMultiSend ? tr("Active") : tr("Not Active")));
    }
}

#ifdef ENABLE_WALLET
bool BitcoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient)) {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch (status) {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setIcon(QIcon(GUIUtil::getThemeImage(":/icons/lock_open")).pixmap(statusIconSize, statusIconSize));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        disconnect(labelEncryptionIcon, SIGNAL(clicked()), walletFrame, SLOT(unlockWallet()));
        connect(labelEncryptionIcon, SIGNAL(clicked()), walletFrame, SLOT(lockWallet()));
        break;
    case WalletModel::UnlockedForAnonymizationOnly:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setIcon(QIcon(GUIUtil::getThemeImage(":/icons/lock_open")).pixmap(statusIconSize, statusIconSize));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b> for anonimization and staking only"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        disconnect(labelEncryptionIcon, SIGNAL(clicked()), walletFrame, SLOT(lockWallet()));
        connect(labelEncryptionIcon, SIGNAL(clicked()), walletFrame, SLOT(unlockWallet()));
        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setIcon(QIcon(GUIUtil::getThemeImage(":/icons/lock_closed")).pixmap(statusIconSize, statusIconSize));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        disconnect(labelEncryptionIcon, SIGNAL(clicked()), walletFrame, SLOT(lockWallet()));
        connect(labelEncryptionIcon, SIGNAL(clicked()), walletFrame, SLOT(unlockWallet()));
        break;
    }
}
#endif // ENABLE_WALLET

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if (!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden()) {
        show();
        activateWindow();
    } else if (isMinimized()) {
        showNormal();
        activateWindow();
    } else if (GUIUtil::isObscured(this)) {
        raise();
        activateWindow();
    } else if (fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::detectShutdown()
{
    if (ShutdownRequested()) {
        if (rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void BitcoinGUI::showProgress(const QString& title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog)
        progressDialog->setValue(nProgress);
}


bool GetDocumentObject(const QString &jsonStr, QJsonDocument &document, QJsonObject &Object)
{
    QJsonParseError ParseError;
    document = QJsonDocument::fromJson(jsonStr.toUtf8(), &ParseError);
    if(!document.isNull() && ParseError.error == QJsonParseError::NoError)
    {
        if(document.isObject())
        {
            Object = document.object();
            if(!Object.isEmpty())
            {
                return true;
            }
        }
    }
    return false;
}

bool GetQtJsonObject(const QJsonObject &Object, const char *name, QJsonObject &dataObj)
{
    if(Object.contains(name))
    {
        QJsonValue value = Object.value(name);
        if(value.isObject())
        {
            dataObj = value.toObject();
            qDebug() << "getObject:" << name << "exist";
            return true;
        }
    }
    return false;
}

bool GetQtJsonString(const QJsonObject &Object, const char *name, QString &str)
{
    if(Object.contains(name))
    {
        QJsonValue dataObj = Object.value(name);
        if(dataObj.isString())
        {
            str = dataObj.toString();
            return true;
        }
    }
    return false;
}

static bool ThreadSafeMessageBox(BitcoinGUI* gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
        modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(caption)),
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(unsigned int, style),
        Q_ARG(bool*, &ret));
    return ret;
}

/*
retrun:
    true: have new version
    false: no new version
*/
static bool GetAndCompareVersion(BitcoinGUI* gui,  QString &version, QString &downloadUrl) {
    static bool isRunning = false;
    bool fHaveNew = false;
    bool fRet = false;
    QString url("https://binaries.axel.network/downloads/axelwallet/index.json");
    QString body;

    QJsonDocument document;
    QJsonObject object;
    QJsonObject walletObj;
    QJsonObject loaderObj;
    QString baseUrl;
    QString loaderStr;
    QString platformStr;
    QString suffixStr;

    if(isRunning) return false;
    isRunning = true;

    // 1. Get the latest version from server.
    body = gui->httpReq.get(url);

    // 2. Parse the version from response.
    fRet = GetDocumentObject(body, document, object);
    if(fRet) fRet = GetQtJsonObject(object, "axelwallet", walletObj);
    if(fRet) {

        if(fRet) fRet = GetQtJsonObject(walletObj, "loader", loaderObj);
        if(fRet) GetQtJsonString(walletObj, "version", version);
        if(fRet) GetQtJsonString(walletObj, "base_url", baseUrl);


        #ifdef WIN32
        platformStr = "-win";
        suffixStr = ".exe";
        fRet = GetQtJsonString(loaderObj, "win", loaderStr);
        #else
        #ifdef MAC_OSX
        platformStr = "-osx";
        suffixStr = ".dmg";
        fRet = GetQtJsonString(loaderObj, "osx", loaderStr);
        #else
        platformStr = "-linux";
        suffixStr = "";
        fRet = GetQtJsonString(loaderObj, "linux", loaderStr);
        #endif
        #endif
        if(baseUrl.right(1) == "/"){
            downloadUrl = baseUrl + loaderStr + "?filename=axel-" + version + platformStr + suffixStr;
        }
        else{
            downloadUrl = baseUrl +  "/" + loaderStr + "?filename=axel-" + version + platformStr + suffixStr;
        }
    }

    // 3.  Compare with current version.
    if(fRet) {
        std::string sVersion = version.toStdString();
        const char* s = sVersion.c_str();
        int first = -1, sec = -1, thir = -1, four = -1;
        int iCurrentVersion = -1, iNewVersion = -1;
        if(s) sscanf(s, "%d.%d.%d.%d", &first, &sec, &thir, &four);
        if((first != -1) &&  (sec != -1) &&  (thir != -1) &&  (four != -1)) {
            iNewVersion = first * 1000000 + sec * 10000 + thir * 100 + four;
        }
        if((iNewVersion != -1) && (iNewVersion > CLIENT_VERSION)) fHaveNew = true;
    }

    isRunning = false;
    return fHaveNew;
}


void BitcoinGUI::checkVersion()
{
    QSettings settings;

    if (!settings.contains("fCheckVersion"))
        settings.setValue("fCheckVersion", true);

    bool fCheck = settings.value("fCheckVersion").toBool();
    if (!fCheck) return;

    QString version;
    QString url;
    bool fHaveNew = false;
    fHaveNew = GetAndCompareVersion(this, version, url);
    if (!fHaveNew) return;

    QString message;
    message = QString(
                        "<div style=\'font-size: 14px;"
                        "line-height: 21px;"
                        "max-width: 384px;"
                        "padding-left: 10px;\'>"
                        "A new version of Axel coin Desktop Wallet is available.<br/>"
                        "New version: %1 <a style=\'padding-left: 10px;\'"
                        "href=\"%2\">Download </a> <br/>"
                        "<span style=\'font-size: 12px; line-height: 17px;"
                        "color: #757575;\'>( If you don't want to receive the updates any more, you can go to Settings - Options - Main to cancel the update check. )</span>"
                        "</div>").arg(version, url);
    ThreadSafeMessageBox(this, message.toStdString(), "Update Available", CClientUIInterface::MSG_WARNING);
}

void BitcoinGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void BitcoinGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

/** Get restart command-line parameters and request restart */
void BitcoinGUI::handleRestart(QStringList args)
{
    if (!ShutdownRequested())
        emit requestedRestart(args);
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl() : optionsModel(0),
                                                             menu(0)
{
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent* event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu();
    foreach (BitcoinUnits::Unit u, BitcoinUnits::availableUnits()) {
        QAction* menuAction = new QAction(QString(BitcoinUnits::name(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(onMenuSelection(QAction*)));
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel* optionsModel)
{
    if (optionsModel) {
        this->optionsModel = optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(optionsModel, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit(int)));

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        setPixmap(QIcon(GUIUtil::getThemeImage(":/icons/unit_" + BitcoinUnits::id(newUnits))).pixmap(39, STATUSBAR_ICONSIZE));
    } else {
        setPixmap(QIcon(GUIUtil::getThemeImage(":/icons/unit_t" + BitcoinUnits::id(newUnits))).pixmap(39, STATUSBAR_ICONSIZE));
    }
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action) {
        optionsModel->setDisplayUnit(action->data());
    }
}
