Wifi Configuration Sync

sync_wifi is a component which provides the necessary APIs to sync Wi-Fi
credentials across devices.  This component will receive changes from the
Chrome sync server as well as monitor local changes to the network list
and keep the two network lists in sync with each other.  Local changes will
be monitored using chromeos::NetworkStateHandler and updated using
chromeos::NetworkConfigurationHandler.  Changes from the server will be
received through the syncer::ModelTypeSyncBridge interface.

Only password protected networks which were added by the specific user will be
synced to their account.  Public networks, enterprise networks, and networks
which have static ip configurations will not be synced.

The network configurations with credentials will be stored in the users
cryptohome using a syncer::ModelTypeStore and held in memory during the
user session.  All network details will be encrypted before getting sent
to the Chrome sync server.

This feature is tracked at http://crbug.com/954648