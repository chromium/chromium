# Identity Manager Internal Implementation

Freshness: 2026-09-02

## Overview
This directory contains the internal implementation classes that back the public identity management APIs in [public/identity_manager](../../public/identity_manager/README.md). Its visibility is restricted strictly to implementation files within `//components/signin/public/identity_manager` and its associated unit test targets.

The internal subsystems manage primary account tracking, OAuth2 refresh and access token lifecycles, platform-specific token storage and delegation, Gaia cookie jar synchronization, account metadata tracking and network fetching, and cryptographic token binding.

### Architectural Layering & Subsystem Flow
The internal architecture is structured around four primary coordination pillars organized in a strict conceptual and dependency hierarchy:

- **Primary Account Coordination**: Sits at the top of the stack. [`PrimaryAccountManager`](./primary_account_manager.h) manages primary identity and sync consent, persisting state to `PrefService` and notifying `IdentityManager` upon sign-in/out transitions.
- **Metadata Fetching & Caching (The ATS vs. AFS Split)**:
  - [`AccountTrackerService`](./account_tracker_service.h) sits at the foundational layer: it maintains the canonical mapping from `CoreAccountId` to `AccountInfo` and has zero dependency on token management. Platform token delegates rely on it during startup to validate and seed accounts from disk.
  - [`AccountFetcherService`](./account_fetcher_service.h) was explicitly split from `AccountTrackerService` to keep the cache decoupled from token management. It sits above the token service, observing token additions and revocations to initiate or cancel network fetches, populating newly retrieved metadata into `AccountTrackerService`.
- **Token Management & Delegation**: [`ProfileOAuth2TokenService`](./profile_oauth2_token_service.h) coordinates OAuth2 refresh and access token lifecycles. It delegates access token requests to `OAuth2AccessTokenManager` and delegates platform credential storage to concrete platform delegates (detailed in [`docs/token_service.md`](./docs/token_service.md)).
- **Cookie Jar Synchronization**: [`GaiaCookieManagerService`](./gaia_cookie_manager_service.h) operates as an independent peer to `AccountFetcherService`, serializing Multilogin, session reconciliation, and logout requests (detailed in [`docs/gaia_cookie_manager_service.md`](./docs/gaia_cookie_manager_service.md)).

### Internal Event Dispatch: Callbacks vs. ProfileOAuth2TokenServiceObserver
Most internal classes (`PrimaryAccountManager`, `AccountTrackerService`, `GaiaCookieManagerService`) communicate state mutations to `IdentityManager` through direct registration callbacks established during construction. In turn, `IdentityManager` forwards these events to external consumers through its public [`IdentityManager::Observer`](../../public/identity_manager/identity_manager.h) interface.

In contrast, [`ProfileOAuth2TokenService`](./profile_oauth2_token_service.h) maintains a multi-listener observer list ([`ProfileOAuth2TokenServiceObserver`](./profile_oauth2_token_service_observer.h)) because multiple internal peer classes must listen to refresh token events independently:
- **`PrimaryAccountManager`** observes `ProfileOAuth2TokenServiceObserver` to reconcile primary account credentials or clear state when refresh tokens are revoked (`OnRefreshTokenRevoked`, `OnRefreshTokensLoaded`).
- **`AccountFetcherService`** observes `ProfileOAuth2TokenServiceObserver` to trigger profile and capability network fetches when an account becomes available, or cancel pending requests when a token is revoked (`OnRefreshTokenAvailable`, `OnRefreshTokenRevoked`).
- **`IdentityManager`** observes `ProfileOAuth2TokenServiceObserver` to forward token availability, revocation, and authentication error changes directly to public `IdentityManager::Observer` listeners.

State mutations from public APIs are dispatched through dedicated internal mutators (`PrimaryAccountMutatorImpl`, `AccountsMutatorImpl`, `DeviceAccountsSynchronizerImpl`, `AccountsCookieMutatorImpl`) driving their respective underlying services.

## Interface

### Core Internal Services (Layered Hierarchy)
Classes are organized in top-down architectural dependency order:

1. [`PrimaryAccountManager`](./primary_account_manager.h): Tracks primary account identity, sync consent level transitions, and dispatches callbacks to `IdentityManager`.
2. [`AccountFetcherService`](./account_fetcher_service.h): Sits above `ProfileOAuth2TokenService`, observing it for token additions/revocations to fetch user profiles, avatars, and capabilities from Gaia into `AccountTrackerService`.
3. [`GaiaCookieManagerService`](./gaia_cookie_manager_service.h): Independent peer to `AccountFetcherService`. Manages web cookie sessions, Multilogin requests, and cookie change monitoring (detailed architecture in [`docs/gaia_cookie_manager_service.md`](./docs/gaia_cookie_manager_service.md)).
4. [`ProfileOAuth2TokenService`](./profile_oauth2_token_service.h): Coordinates OAuth2 refresh and access token lifecycles, maintaining `ProfileOAuth2TokenServiceObserver` for internal peers and delegating access tokens to `OAuth2AccessTokenManager` (detailed architecture in [`docs/token_service.md`](./docs/token_service.md)).
5. [`TokenBindingHelper`](./token_binding_helper.h): Coordinates hardware-backed cryptographic keys and generates assertions for device-bound refresh tokens (DICE).
6. [`AccountTrackerService`](./account_tracker_service.h): Foundational repository mapping `CoreAccountId` to `AccountInfo`. Sits at the bottom of the stack because platform delegates require it during initialization and credential loading without depending on token management.

### Public Mutator Implementations
- [`signin::PrimaryAccountMutatorImpl`](./primary_account_mutator_impl.h): Implements public `PrimaryAccountMutator` by driving `PrimaryAccountManager`.
- [`signin::AccountsMutatorImpl`](./accounts_mutator_impl.h): Implements public `AccountsMutator` by driving `ProfileOAuth2TokenService`.
- [`signin::AccountsCookieMutatorImpl`](./accounts_cookie_mutator_impl.h): Implements public `AccountsCookieMutator` by coordinating `GaiaCookieManagerService` and `OAuthMultiloginHelper`.
- [`signin::DeviceAccountsSynchronizerImpl`](./device_accounts_synchronizer_impl.h): Implements public `DeviceAccountsSynchronizer` for platform SSO and device account sync.
- [`signin::DiagnosticsProviderImpl`](./diagnostics_provider_impl.h): Implements public `DiagnosticsProvider` to query token backoff and load status.

### Platform Delegation & Builders
- [`BuildProfileOAuth2TokenService`](./profile_oauth2_token_service_builder.h): Factory function constructing the platform-appropriate `ProfileOAuth2TokenService` and delegate instance for a profile.
- [`ProfileOAuth2TokenServiceDelegate`](./profile_oauth2_token_service_delegate.h): Abstract base class defining the contract for platform-specific refresh token storage and synchronization.
- [`MutableProfileOAuth2TokenServiceDelegate`](./mutable_profile_oauth2_token_service_delegate.h): Desktop (Linux/Win/Mac) delegate storing encrypted tokens in SQLite `TokenWebData`.
- [`ProfileOAuth2TokenServiceDelegateAndroid`](./profile_oauth2_token_service_delegate_android.h): Android delegate interacting with native Android account manager APIs via JNI.
- [`signin::ProfileOAuth2TokenServiceDelegateChromeOS`](./profile_oauth2_token_service_delegate_chromeos.h): ChromeOS delegate synchronizing with `AccountManagerFacade`.
- [`ProfileOAuth2TokenServiceIOSDelegate`](./profile_oauth2_token_service_delegate_ios.h): iOS delegate bridging to `DeviceAccountsProvider` for iOS SSO identities.

### Test Utilities & Fakes
- [`FakeProfileOAuth2TokenService`](./fake_profile_oauth2_token_service.h): Test subclass supporting mock access token fetching and manual token provisioning.
- [`FakeProfileOAuth2TokenServiceDelegate`](./fake_profile_oauth2_token_service_delegate.h): In-memory test delegate simulating token storage and error states.
- [`FakeAccountCapabilitiesFetcher`](./fake_account_capabilities_fetcher.h): Test fake for immediate or deferred capability fetch responses.
- [`FakeAccountFetcherFactory`](./fake_account_fetcher_factory.h): Test factory producing fake info and capability fetchers.
- [`TestProfileOAuth2TokenServiceDelegateChromeOS`](./test_profile_oauth2_token_service_delegate_chromeos.h): ChromeOS delegate test double for browser tests and mock accounts.

## Invariants
- **Strict Internal Visibility**: Implementation headers in this directory must not be included outside `//components/signin/internal/identity_manager`, `//components/signin/public/identity_manager`, and unit tests.
- **Embedder Agnosticism**: Internal classes must remain embedder-agnostic. Concrete embedders (`//chrome`, `//ios/chrome`) are not referenced directly; platform dependencies are supplied through builders ([`BuildProfileOAuth2TokenService`](./profile_oauth2_token_service_builder.h)) or abstract delegate interfaces.
- **Initialization Order**: [`AccountTrackerService`](./account_tracker_service.h) must be initialized prior to [`ProfileOAuth2TokenService`](./profile_oauth2_token_service.h) credential loading so that `CoreAccountId` mappings and account metadata are available when token delegates load tokens from persistent storage.
- **Serialized Cookie Requests**: [`GaiaCookieManagerService`](./gaia_cookie_manager_service.h) processes cookie mutations (`SET_ACCOUNTS`, `LOG_OUT`, `LIST_ACCOUNTS`) serially through an internal FIFO queue (`requests_`). A new request is not dispatched until the preceding request completes or fails.
- **Primary Account Persistence & Immutability**: Once established, the primary account is persisted in profile preferences (`kGoogleServicesAccountId`, `kGoogleServicesConsentedToSync`) and cannot be switched to a different account without an explicit sign-out or account clearance operation (which is forbidden on ChromeOS).
- **Token Binding Key Consistency**: When DICE token binding is enabled, wrapped binding keys stored in `TokenWebData` must match keys loaded by `UnexportableKeyService`. Refresh tokens bound to hardware keys require valid assertions for multilogin and token refresh operations.

## Side Effects
- **Persistent Storage**:
  - Persists account metadata (emails, Gaia IDs, hosted domains, child account flags, advanced protection flags) to profile preferences in `PrefService`.
  - Persists primary account identity and consent level to `PrefService`.
  - Persists encrypted refresh tokens and wrapped binding keys to SQLite `TokenWebData` (on desktop platforms via [`MutableProfileOAuth2TokenServiceDelegate`](./mutable_profile_oauth2_token_service_delegate.h)).
  - Asynchronously saves downloaded account avatar images to disk under `user_data_dir` via `image_storage_task_runner_`.
- **Network & Gaia Endpoints**:
  - Issues HTTPS requests via `network::SharedURLLoaderFactory` and `GaiaAuthFetcher` to Gaia endpoints (`/ListAccounts`, `/OAuthMultilogin`, `/MergeSession`, `/Logout`).
  - Fetches account user information and account capabilities over the network.
  - Fetches avatar image assets via `ImageFetcherImpl`.
  - Requests OAuth2 access tokens via `OAuth2AccessTokenManager` / `OAuth2AccessTokenFetcher`.
- **Cookie Jar Mutations**:
  - Modifies HTTP cookies in `network::mojom::CookieManager` across default and partitioned contexts upon sign-in, account reconciliation, or logout.
- **Observer Notifications**:
  - Dispatches `PrimaryAccountManager::Observer::OnPrimaryAccountChanged` when sign-in state or consent transitions occur.
  - Dispatches `ProfileOAuth2TokenServiceObserver` events (`OnRefreshTokenAvailable`, `OnRefreshTokenRevoked`, `OnRefreshTokensLoaded`, `OnAuthErrorChanged`).
  - Invokes internal callbacks to notify `IdentityManager` of extended account updates and cookie jar changes.

## Verification
- **Build**: `autoninja -C out/Default components_unittests`
- **Test**: `out/Default/components_unittests --gtest_filter="AccountCapabilitiesFetcherTest.*:AccountInfoUtilTest.*:AccountTrackerServiceTest.*:GaiaCookieManagerServiceTest.*:OAuthMultiloginHelperTest.*:OAuthMultiloginTokenFetcherTest.*:PrimaryAccountManagerTest.*:ProfileOAuth2TokenServiceDelegateTest.*:ProfileOAuth2TokenServiceTest.*:MutableProfileOAuth2TokenServiceDelegateTest.*:OAuth2UpgradeTokenFlowTest.*:TokenBindingHelperTest.*:TokenBindingOAuth2AccessTokenFetcherTest.*"`

