# Sync Nigori (encryption)

Nigori is Chrome Sync's encryption layer. It owns the keys that encrypt
sensitive sync data before it is sent to the Sync server: the types in
`AlwaysEncryptedUserTypes()` (such as passwords) are always encrypted, and
setting a custom passphrase encrypts every type in `EncryptableUserTypes()`
(both in [`data_type.h`](../base/data_type.h)). The keys are shared between the
user's devices through a dedicated `NIGORI` entity
([`nigori_specifics.proto`](../protocol/nigori_specifics.proto)), whose keybag
is protected according to the [`PassphraseType`](../base/passphrase_enums.h):
keystore keys handed out by the server (the default), a custom passphrase the
user enters, or trusted vault keys. See the [architecture
overview](../README.md) for where Nigori fits in Sync and
[`SECURITY.md`](../SECURITY.md) for the threat model.

Everything here runs on the sync sequence, shared with the engine. Unlike other
data types, whose bridges run on their own model sequence, the Nigori bridge and
processor live on the sync sequence too.

## How to use it

*   [`SyncEncryptionHandler`](../engine/sync_encryption_handler.h) is the
    interface the rest of Sync uses (implemented by
    [`NigoriSyncBridgeImpl`](nigori_sync_bridge_impl.h), created by
    [`SyncEngineBackend`](../service/glue/sync_engine_backend.h) and reached
    through `SyncManager::GetEncryptionHandler()`). It exposes the encrypted
    types, the passphrase type and the cryptographer, and accepts passphrases,
    bootstrap tokens, trusted vault keys and keystore keys.
*   `SyncEncryptionHandler::Observer`: implement it and call `AddObserver()` to
    learn when a passphrase, trusted vault keys or keystore keys are required or
    accepted, and when the encrypted types, passphrase type or cryptographer
    change. On the UI thread,
    [`SyncServiceCrypto`](../service/sync_service_crypto.h) turns these
    notifications into the encryption state exposed by `SyncUserSettings`.
*   [`Cryptographer`](../engine/cryptographer.h), returned by
    `GetCryptographer()`: encrypts and decrypts data with the current keys. The
    engine's [`DataTypeWorker`](../engine/data_type_worker.h) (one per data
    type) uses it for every encrypted data type, and its
    `AuthEncryptForCrossUserSharing()` / `AuthDecryptForCrossUserSharing()`
    encrypt and decrypt data exchanged with other users, such as password
    sharing invitations.
*   Tests: integration tests live in `chrome/browser/sync/test/integration/`
    (`single_client_nigori_sync_test.cc` and the
    `*_custom_passphrase_sync_test.cc` files), and
    [`components/sync/test/`](../test/) has `FakeServer` and keybag helpers
    (`fake_server_nigori_helper.h`, `nigori_test_utils.h`).

## Lay of the land

[`NigoriSyncBridgeImpl`](nigori_sync_bridge_impl.h) is the hub; the rest of the
directory serves it.

*   State: [`NigoriState`](nigori_state.h) holds everything the bridge knows
    (cryptographer, pending keys, passphrase type, keystore keys, cross-user
    sharing public key) and converts it to and from the local proto
    (`NigoriModel`) and the server proto (`NigoriSpecifics`).
*   Keys: [`CryptographerImpl`](cryptographer_impl.h) implements `Cryptographer`
    on top of a [`NigoriKeyBag`](nigori_key_bag.h); individual keys and key
    derivation (`Nigori`, `KeyDerivationParams`) live in
    [`model/crypto/`](../model/crypto/).
    [`KeystoreKeysCryptographer`](keystore_keys_cryptographer.h) wraps the
    keystore keys received from the server. The X25519 key pairs used for
    cross-user sharing are [`CrossUserSharingKeys`](cross_user_sharing_keys.h),
    [`CrossUserSharingPublicPrivateKeyPair`](cross_user_sharing_public_private_key_pair.h)
    and [`CrossUserSharingPublicKey`](cross_user_sharing_public_key.h).
*   Local changes: every change the client initiates (setting a custom
    passphrase, initializing keystore Nigori, generating cross-user sharing
    keys, migrating from the implicit passphrase) is a
    [`PendingLocalNigoriCommit`](pending_local_nigori_commit.h), which the
    bridge applies to `NigoriState` and commits in order.
*   Syncing the `NIGORI` entity: Nigori has its own processor,
    [`NigoriDataTypeProcessor`](nigori_data_type_processor.h), which talks to
    the bridge through [`NigoriSyncBridge`](nigori_sync_bridge.h) and
    [`NigoriLocalChangeProcessor`](nigori_local_change_processor.h), the Nigori
    counterparts of `DataTypeSyncBridge` and `DataTypeLocalChangeProcessor`.
    Unlike other data types, its controller is not exposed to the UI thread or
    `DataTypeManager`; the engine starts it directly.
*   Persistence: [`NigoriStorage`](nigori_storage.h) persists the bridge state
    and sync metadata locally; [`NigoriStorageImpl`](nigori_storage_impl.h)
    writes them to a file in the sync data folder, encrypted with OSCrypt.
*   Notifications:
    [`SyncEncryptionHandlerObserverList`](sync_encryption_handler_observer_list.h)
    broadcasts `SyncEncryptionHandler::Observer` calls, and
    [`RequiredPassphraseVerifierImpl`](required_passphrase_verifier_impl.h) is
    the verifier passed to `OnPassphraseRequired()` so a caller can check a
    passphrase against the pending keys.

## Design docs

No public design doc covers Nigori specifically. The [Sync design
documents](https://www.chromium.org/developers/design-documents/sync/) index
lists the public Sync design docs.
