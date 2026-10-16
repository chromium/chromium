# Trusted Vault

`//components/trusted_vault` manages symmetric encryption keys (domain secrets)
for Google Security Domains, used for end-to-end encrypted data in Chrome Sync
(e.g. passwords) and Google Password Manager passkeys.

Keys are coordinated by the Security Domain Service (SDS). Each enrolled member
of a security domain (such as a physical device, macOS iCloud Keychain, or a
knowledge factor) registers an asymmetric public key with SDS, which stores the
domain secret wrapped under each member's public key.

## Platform-Independent Layer

These types are relevant for all platforms:

*   **`SecurityDomainId`** (`trusted_vault_server_constants.h`): Identifies the
    target security domain (`kChromeSync` for `chromesync`, `kPasskeys` for
    `hw_protected`).
*   **`TrustedVaultService`** (`trusted_vault_service.h`): `KeyedService`
    (one per profile) that owns one `TrustedVaultClient` per `SecurityDomainId`,
    retrieved via `GetTrustedVaultClient(SecurityDomainId)`.
*   **`TrustedVaultClient`** (`trusted_vault_client.h`): Abstract UI-thread
    interface consumed by feature code. Key operations:
    *   `FetchKeys()`: Asynchronously returns local keys or attempts silent
        recovery from SDS.
    *   `StoreKeys()`: Stores keys obtained externally (e.g., from the web
        recovery flow via Mojo).
    *   `MarkLocalKeysAsStale()`: Marks cached keys as stale so the next
        `FetchKeys()` attempts to download newer keys from SDS.
    *   `GetIsRecoverabilityDegraded()` / `AddTrustedRecoveryMethod()`: Queries
        whether user action is needed to add a recovery factor, and enrolls new
        recovery factors using locally held domain keys.
    *   `Observer`: Notifies consumers when keys or degraded recoverability
        state change (`OnTrustedVaultKeysChanged`,
        `OnTrustedVaultRecoverabilityChanged`).

### Platform Implementations of `TrustedVaultClient`

*   **Desktop (Windows, macOS, Linux, ChromeOS)**:
    `StandaloneTrustedVaultClient` (in this directory; below).
*   **Android**: `TrustedVaultClientAndroid`, which
    bridges over JNI to GMS Core.
*   **iOS**: Implemented in `//ios/chrome/browser/trusted_vault/`, delegating
    to the iOS system keychain provider.

## Standalone (Desktop) Architecture

On desktop platforms, Chromium implements the full Trusted Vault client stack.

```
UI Thread
+-----------------------------------------------------------------------+
| TrustedVaultService                                                   |
|   +-- StandaloneTrustedVaultClient (kChromeSync) --+                  |
|   +-- StandaloneTrustedVaultClient (kPasskeys)   --+                  |
|                                                    |                  |
|                                                    v                  |
|                                    StandaloneTrustedVaultFrontend     |
|                                    (shared RefCounted coordinator)    |
+----------------------------------------------------+------------------+
                                                     | PostTask
Background Sequence (MayBlock)                       v
+-----------------------------------------------------------------------+
| StandaloneTrustedVaultBackend (single shared multi-domain instance)   |
|   +-- LocalDomainsStorage (OSCryptAsync-encrypted local_domains_data) |
|   +-- LocalRecoveryFactors                                            |
|   |     +-- PhysicalDeviceRecoveryFactor (shared device key pair)     |
|   |     +-- ICloudKeychainRecoveryFactor (macOS)                      |
|   +-- TrustedVaultThrottlingConnectionImpl -> TrustedVaultConnection  |
|   +-- TrustedVaultDegradedRecoverabilityHandler (per domain)          |
+-----------------------------------------------------------------------+
```

### UI Thread Components

*   **`StandaloneTrustedVaultClient`** (`standalone_trusted_vault_client.h`):
    Lightweight per-`SecurityDomainId` facade implementing `TrustedVaultClient`.
    Manages the domain's `ObserverList` and forwards all calls (tagged with its
    `SecurityDomainId`) to the shared `StandaloneTrustedVaultFrontend`.
*   **`StandaloneTrustedVaultFrontend`**
    (`standalone_trusted_vault_frontend.h`):
    `RefCounted` coordinator shared across all `StandaloneTrustedVaultClient`
    instances for a profile:
    *   Requests an `os_crypt_async::Encryptor` from `OSCryptAsync` on startup.
        Calls arriving before the encryptor is ready are queued in
        `pending_tasks_` and replayed in FIFO order once the backend is
        initialized.
    *   Creates and owns the single `StandaloneTrustedVaultBackend` on a
        background `SequencedTaskRunner`.
    *   Observes `signin::IdentityManager` (primary account changes, refresh
        token error state, and cookie jar accounts) and forwards state updates
        to the backend.
    *   Owns `TrustedVaultAccessTokenFetcherFrontend` to fetch OAuth2 access
        tokens on the UI thread on behalf of backend network requests.
    *   Routes domain-scoped notifications from the backend to the corresponding
        `StandaloneTrustedVaultClient`.

### Background Sequence Components

*   **`StandaloneTrustedVaultBackend`** (`standalone_trusted_vault_backend.h`):
    Single shared state machine handling all security domains on the background
    sequence. Coordinates key retrieval, factor registration, pending trusted
    recovery methods, and per-domain
    `TrustedVaultDegradedRecoverabilityHandler` instances.
*   **`TrustedVaultDegradedRecoverabilityHandler`**
    (`trusted_vault_degraded_recoverability_handler.h`):
    Periodically polls SDS to check whether the user's security domain
    recoverability is degraded and persists the state via
    `DegradedRecoverabilityStorage`.

## Storage (`LocalDomainsStorage`)

*   **`LocalDomainsStorage`** (`local_domains_storage.h`):
    Persists multi-domain state (`trusted_vault_pb::LocalDomainsData`, defined
    in `proto/local_domains_data.proto`) in
    `<profile_dir>/local_domains_data.pb`.
    *   The serialized protobuf is encrypted at rest using
        `os_crypt_async::Encryptor` and written atomically via
        `base::ImportantFileWriter`.
    *   Implements five role-specific interfaces consumed by backend
        collaborators:
        *   `PhysicalDeviceStorage` (per-user, domain-independent): Accesses the
            physical device `SecureBox` private key shared across all domains.
        *   `RecoveryFactorRegistrationStorage` (per-user, per-domain): Tracks
            whether each `LocalRecoveryFactorType` is registered in a domain.
        *   `KeyStorage` (per-user, per-domain): Stores domain keys, last key
            version, and staleness flags.
        *   `ConnectionThrottlingStorage` (per-user, per-domain): Stores the
            timestamp of the last failed request for client-side throttling.
        *   `DegradedRecoverabilityStorage` (per-user, per-domain): Stores
            cached degraded recoverability state and last refresh timestamp.
*   **`LegacyStandaloneTrustedVaultStorage`**
    (`legacy_standalone_trusted_vault_storage.h`):
    Static migration helper used only when `local_domains_data.pb` does not yet
    exist on disk. Reads and upgrades legacy unencrypted `trusted_vault.pb`
    files (`proto/local_trusted_vault.proto`), after which `LocalDomainsStorage`
    persists the migrated data and overwrites / deletes the legacy files.

## Local Recovery Factors

*   **`LocalRecoveryFactor`** (`local_recovery_factor.h`):
    Interface for local recovery mechanisms managed by
    `StandaloneTrustedVaultBackend`. Each factor can register itself with SDS
    (`MaybeRegister`) and recover keys from SDS (`AttemptRecovery`) for a given
    `SecurityDomainId`.
*   **`PhysicalDeviceRecoveryFactor`** (`physical_device_recovery_factor.h`):
    Uses a single per-user key pair (stored in `PhysicalDeviceStorage`) across
    all security domains. Supports silent pre-enrollment with the constant key
    for domains that allow it (`kChromeSync`).
*   **`ICloudKeychainRecoveryFactor`** (`icloud_keychain_recovery_factor.h`,
    `icloud_recovery_key_mac.h`, macOS only):
    Stores and retrieves synced key pairs in macOS iCloud Keychain
    (`kSecAttrSynchronizable`). Attempted after `PhysicalDeviceRecoveryFactor`
    during key recovery.

## Network and Cryptography

*   **`TrustedVaultConnection`** / **`TrustedVaultConnectionImpl`**
    (`trusted_vault_connection.h`):
    Stateless HTTP client (`TrustedVaultRequest`) for SDS endpoints (member
    registration, key downloading, member listing).
*   **`TrustedVaultThrottlingConnectionImpl`**
    (`trusted_vault_throttling_connection_impl.h`):
    Decorator around `TrustedVaultConnection` that blocks repeated requests
    after transient failures per user and per `SecurityDomainId`.
*   **`RecoveryKeyStoreConnection`**
    (`recovery_key_store_connection.h`, `recovery_key_store_certificate.h`):
    Client for uploading recovery keys to the Recovery Key Store service.
*   **`SecureBox`** (`securebox.h`) and `trusted_vault_crypto.h`:
    Cryptographic primitives implementing `SecureBox` v2 (NIST P-256 ECDH +
    HKDF-SHA256 + AES-128-GCM) and HMAC-SHA256 key rotation proofs.
