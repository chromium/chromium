# Identity Manager Public API

Freshness: 2026-09-02

## Overview
This directory defines the core public C++ API surfaces and data models for querying and mutating Google identities across Chromium. `IdentityManager` is the primary entry point for managing Gaia accounts, tracking the primary authenticated account, requesting OAuth2 access tokens, synchronizing the Gaia cookie jar, and observing account state lifecycle changes.

Accounts can be found across two distinct stores:
- **Cookies**: Mostly used by web pages and web navigation flows. Accounts in the Gaia cookie jar are represented via [`signin::AccountsInCookieJarInfo`](./accounts_in_cookie_jar_info.h).
- **OAuth Tokens**: Mostly used by native Chrome browser code and services. Accounts backed by OAuth2 refresh tokens are represented via [`AccountInfo`](./account_info.h) (or [`CoreAccountInfo`](./account_info.h)).

Because these representations and mechanisms serve different consumers, the two stores do not always contain the same set of accounts (for example, an account signed into the web may not have an OAuth token in Chrome, and a browser-managed account may be temporarily absent or invalid in the Gaia cookie).

### Layering, Subdirectories & Boundaries
`components/signin` is designed as an embedder-agnostic component. Header files in this directory must depend strictly on:
- `//components/signin/public/base`
- `//components/signin/public/webdata`
- Standard foundational libraries (`//base`, `//google_apis`, `//ui/gfx`, `//components/keyed_service/core`).

Implementation files (`.cc` / `.mm`) are additionally permitted to depend on internal implementation headers in `//components/signin/internal/identity_manager`. Platform-specific embedders supply their own concrete platform delegates and builders.

Subdirectories isolate platform-specific interfaces:
- **[ios/](./ios/README.md)**: Defines `DeviceAccountsProvider`, the dependency-inversion abstraction for querying iOS platform Single Sign-On (SSO) accounts.
- **[objc/](./objc/README.md)**: Objective-C bridge (`IdentityManagerObserverBridge`) exposing `IdentityManager::Observer` events to Objective-C and Objective-C++ consumers via protocols.

### Conceptual Architecture & Core Models
This guide details the core identity concepts in `IdentityManager` and maps each concept to its corresponding data structures, query methods, and observer events.

#### 1. Accounts & Account Information
In `IdentityManager`, an "account" always refers to a Google/Gaia account. Identity metadata is partitioned across two layered data structures:

- **Core Account Identifiers ([`CoreAccountInfo`](./account_info.h))**:
  Every account known to Chrome has three foundational identifiers, bundled together in `CoreAccountInfo` and guaranteed to remain available as long as the account is visible:
  1. **Email Address** (`email`): The user-facing email address string (e.g., `user@example.com`).
  2. **Gaia ID** (`gaia`): The persistent, globally unique Google user ID assigned by Google identity services.
  3. **Account ID** (`account_id` / [`CoreAccountId`](../../../../google_apis/gaia/core_account_id.h)): An opaque, Chrome-internal identifier for the account (canonically equal to the Gaia ID or normalized email depending on platform migration state).
- **Extended Account Metadata ([`AccountInfo`](./account_info.h))**:
  Extends `CoreAccountInfo` with rich profile attributes fetched asynchronously from Gaia (via internal `AccountFetcherService`):
  - User's display name, given name, and profile picture/avatar URL.
  - Hosted domain / enterprise status (identifying Google Workspace accounts).
  - Account capabilities (e.g. permission to sync, parental controls, advanced protection).
- **API & Observer Conventions**:
  - To query core or extended account information, use `IdentityManager` methods containing `"Account"` (e.g., `FindExtendedAccountInfoByAccountId()`, `GetAccountsWithRefreshTokens()`).
  - To observe changes as extended profile metadata arrives asynchronously over the network, implement `IdentityManager::Observer` methods containing `"ExtendedAccountInfo"` (e.g., `OnExtendedAccountInfoUpdated()`).

#### 2. The Primary Account & Consent Levels
The "primary account" represents the central authenticated Google user identity in a given Profile.

- **Consent Levels & Deprecation ([`signin::ConsentLevel`](../base/consent_level.h))**:
  Historically, Chromium distinguished between two consent tiers for the primary account:
  - **`ConsentLevel::kSignin`**: Represents the user's signed-in browsing identity (historically termed the "unconsented primary account" or web sign-in identity).
  - **`ConsentLevel::kSync` (Deprecated)**: Historically represented an account explicitly granted consent for browser-wide Chrome Sync.
  > [!IMPORTANT]
  > **`ConsentLevel` is a deprecated concept being phased out of Chromium.** For all new code, **only `ConsentLevel::kSignin` should be used**. The `ConsentLevel` parameter will eventually be removed entirely, and the primary account will simply represent the signed-in account.
- **API & Observer Conventions**:
  - Query primary account state using methods containing `"PrimaryAccount"` (e.g., `GetPrimaryAccountId(signin::ConsentLevel::kSignin)`, `GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)`, `HasPrimaryAccount(signin::ConsentLevel::kSignin)`).
  - To track sign-in, sign-out, or account transitions, observe `IdentityManager::Observer::OnPrimaryAccountChanged(PrimaryAccountChangeEvent)`, which provides detailed before/after diffs and change reasons.

#### 3. OAuth2 Tokens: Access vs. Refresh Tokens
`IdentityManager` handles two distinct classes of OAuth2 client-server authorization tokens:

- **OAuth2 Refresh Tokens (Long-Lived Credentials)**:
  - Long-lived credentials obtained when a user explicitly signs into the browser or adds a Google account at the OS level.
  - **Encapsulation Invariant**: Consumers of `IdentityManager` **never directly see, receive, or store raw refresh token strings**. Chrome manages refresh token persistence and cryptography entirely internally (e.g. in encrypted SQLite `TokenWebData` or via system SSO brokers).
  - **Accounts with Refresh Tokens**: Refers to the set of accounts that currently possess valid, usable refresh credentials in the browser store. Query using `IdentityManager` methods containing `"RefreshToken"` (e.g. `HasAccountWithRefreshToken(account_id)`, `GetAccountsWithRefreshTokens()`), and observe state changes via `OnRefreshTokenAvailable` and `OnRefreshTokenRevoked`.
- **OAuth2 Access Tokens (Short-Lived Authorization)**:
  - Short-lived, scope-limited bearer tokens minted on demand using an account's refresh token to authorize HTTPS API requests to specific Google/Gaia backend endpoints.
- **Client-Side Fetching Interfaces**:
  - [`AccessTokenFetcher`](./access_token_fetcher.h): Primary client-side interface for obtaining access tokens for arbitrary accounts. Handles token caching, scopes, and network fetch dispatch.
  - [`PrimaryAccountAccessTokenFetcher`](./primary_account_access_token_fetcher.h): Specialized helper for fetching tokens for the primary account. Transparently handles the common asynchronous pattern of *"wait until the primary account and its refresh token are available, then fetch an access token"*.

#### 4. The Gaia Cookie Jar vs. Native OAuth Tokens
Identity state exists in two decoupled, distinct stores across Chromium:

- **The Gaia Cookie Jar**:
  - The set of Google authentication cookies (the `SAPISID` cookie jar) present in the network service cookie manager, representing the web-visible accounts currently signed into Google web properties (e.g., mail.google.com, accounts.google.com).
  - Represented by [`signin::AccountsInCookieJarInfo`](./accounts_in_cookie_jar_info.h).
  - **Structural Distinctness**: The data Chrome receives from web cookies is fundamentally distinct from native OAuth token state (tracking cookie freshness, signed-out web states, and lacking OAuth token scopes). Because of this, `AccountsInCookieJarInfo` is a distinct data model rather than reusing `AccountInfo`.
  - Interact with cookie state via `IdentityManager` methods containing `"Cookie"` (e.g., `GetAccountsInCookieJar()`), and observe updates via `IdentityManager::Observer::OnAccountsInCookieUpdated()`.
- **Store Asynchrony**:
  Because native browser tokens and web cookies serve different consumers and protocols, the two stores do not always contain identical accounts. An account signed into the web may lack a native OAuth refresh token in Chrome, and a browser-managed account may be temporarily absent or invalid in the web cookie jar until reconciled by `GaiaCookieManagerService`.

#### 5. Platform-Specific Refresh Token Mutation
While querying token availability is cross-platform, the underlying mechanisms for adding, removing, and synchronizing accounts with refresh tokens vary according to OS identity capabilities:
- **Windows / Mac / Linux (Desktop)**: Chrome manages OAuth2 refresh tokens internally in SQLite `TokenWebData`; mutations are driven directly via [`AccountsMutator`](./accounts_mutator.h).
- **ChromeOS**: Backed by ChromeOS system `AccountManager`; Chrome synchronizes its internal view by observing `AccountManagerFacade`.
- **Android**: Backed by Android OS `AccountManager`; synchronized via Java-side `IdentityMutator.java` through JNI.
- **iOS**: Backed by Google iOS Single Sign-On (SSO) libraries; synchronized via [`DeviceAccountsSynchronizer`](./device_accounts_synchronizer.h) and [`DeviceAccountsProvider`](./ios/device_accounts_provider.h).

### Testing Strategy
- **`IdentityTestEnvironment`**: Preferred test harness for unittests where neither production code nor test fixtures interact directly with `Profile`. Provides synchronous state mutation and token response mocking.
- **`identity_test_utils.h`**: Lower-level test utilities for integration tests and legacy unittests where code explicitly interacts with `Profile` (specifically requiring `IdentityManagerFactory::GetForProfile(profile)`). New tests should avoid direct `Profile` coupling and prefer `IdentityTestEnvironment`.

### Mutation & API Governance
Account mutators ([`PrimaryAccountMutator`](./primary_account_mutator.h), [`AccountsMutator`](./accounts_mutator.h), [`AccountsCookieMutator`](./accounts_cookie_mutator.h), [`DeviceAccountsSynchronizer`](./device_accounts_synchronizer.h)) should only be invoked as part of larger embedder-specific flows adhering to product specifications. Consumers considering a new mutation use case should **contact `//components/signin` OWNERS first** before introducing new call sites. See declaration comments on `PrimaryAccountMutator::SetPrimaryAccount()` for prerequisites required for primary account changes to succeed.

## Interface

### Core Identity Service & Observers
- [`signin::IdentityManager`](./identity_manager.h): Central `KeyedService` providing read access to accounts, primary account state, refresh token status, cookie jar status, and factory methods for token fetchers.
- [`signin::IdentityManager::Observer`](./identity_manager.h): Observer interface for lifecycle events including primary account changes, token updates/removals, cookie jar modifications, and extended account metadata updates.
- [`signin::IdentityManager::DiagnosticsObserver`](./identity_manager.h): Diagnostic observer for monitoring access token requests, completions, and token source operations.
- [`signin::IdentityManagerBuildParams`](./identity_manager_builder.h): Parameter bag for constructing an `IdentityManager` instance from embedder-level dependencies.
- [`signin::DiagnosticsProvider`](./diagnostics_provider.h): Interface for inspecting token loading states and rate-limiting delays.

### Mutators
- [`signin::IdentityMutator`](./identity_mutator.h): Aggregator container holding platform-specific mutators for primary accounts, refresh tokens, cookie jars, and device synchronizers.
- [`signin::PrimaryAccountMutator`](./primary_account_mutator.h): Interface for setting, revoking sync consent for, or clearing the primary account.
- [`signin::AccountsMutator`](./accounts_mutator.h): Interface for adding, updating, removing, and invalidating OAuth2 refresh tokens in local storage.
- [`signin::AccountsCookieMutator`](./accounts_cookie_mutator.h): Interface for setting Gaia cookie accounts via multilogin, triggering cookie updates, and logging out sessions.
- [`signin::DeviceAccountsSynchronizer`](./device_accounts_synchronizer.h): Interface for reloading and seeding device-level system accounts into `IdentityManager`.

### Token Fetching & Scopes
- [`signin::AccessTokenFetcher`](./access_token_fetcher.h): One-shot helper class to request OAuth2 access tokens for a specific `CoreAccountId` and `OAuthConsumerId`.
- [`signin::PrimaryAccountAccessTokenFetcher`](./primary_account_access_token_fetcher.h): High-level helper class to request OAuth2 access tokens for the primary account, supporting deferred waiting until the primary account is available.
- [`signin::AccessTokenInfo`](./access_token_info.h): Struct containing the access token string, expiration timestamp, and optional ID token.
- [`signin::TokenBindingInfo`](./token_binding_info.h): Struct holding wrapped cryptographic binding keys and mTLS flags for bound refresh tokens.
- [`signin::OAuth2ScopeRestriction`](./access_token_restriction.h): Defines consent requirements and privilege levels for OAuth2 API scopes (`GetOAuth2ScopeRestriction`, `IsPrivilegedOAuth2Consumer`).

### Account Models & Capabilities
- [`CoreAccountInfo`](./account_info.h): Basic immutable struct holding `account_id`, `gaia` ID, `email`, and advanced protection status.
- [`AccountInfo`](./account_info.h): Extended account model containing full name, given name, hosted domain, avatar images/URLs, locale, and capabilities.
- [`AccountInfo::Builder`](./account_info.h): Builder pattern class for constructing validated `AccountInfo` instances.
- [`AccountCapabilities`](./account_capabilities.h): Value class storing boolean account capabilities (e.g. enterprise policies, parental controls, AI feature eligibility) represented as `Tribool` states.
- [`signin::AccountManagedStatusFinder`](./account_managed_status_finder.h): Asynchronous helper to evaluate whether an account belongs to an enterprise/managed domain.
- [`AccountStateFetcher`](./account_state_fetcher.h): One-shot helper that monitors `IdentityManager` until a specific account state or capability condition is satisfied.
- [`signin::AccountsInCookieJarInfo`](./accounts_in_cookie_jar_info.h): Snapshot container holding accounts listed in the Gaia cookie jar and freshness status.
- [`signin::PrimaryAccountChangeEvent`](./primary_account_change_event.h): Event details describing previous and current `PrimaryAccountChangeEvent::State` along with mutation access points.
- [`signin::LoadCredentialsState`](./load_credentials_state.h): Enum indicating the status of loading credentials from persistent storage.
- [`signin::Tribool`](./tribool.h): Three-state boolean enum (`kUnknown`, `kFalse`, `kTrue`) with conversion utilities.
- [`signin::IdentityUtils`](./identity_utils.h): Utility functions for checking username patterns (`IsUsernameAllowedByPattern`) and ranking accounts for UI display (`GetOrderedAccountsForDisplay`).

### Test Support Utilities
- [`signin::IdentityTestEnvironment`](./identity_test_environment.h): High-level test fixture and harness for managing `IdentityManager` in unit tests.
- [`signin::IdentityTestUtils`](./identity_test_utils.h): Utility functions for driving `IdentityManager` states, setting tokens, and simulating Gaia responses.
- [`AccountCapabilitiesTestMutator`](./account_capabilities_test_mutator.h): Test helper to mutate capabilities on `AccountInfo` and `AccountCapabilities`.
- [`signin::TestAccounts`](./test_accounts.h): Pre-configured test account fixtures.
- [`signin::TestIdentityManagerObserver`](./test_identity_manager_observer.h): Observer test fake for recording and asserting identity events.

## Invariants
- **Embedder Agnosticism**: Public interfaces in this component must not depend on or reference concrete higher-level embedders (e.g. `//chrome`, `//ios/chrome`). Embedders inject implementations via factories and builders.
- **KeyedService Lifecycle**: `IdentityManager` is tied to the lifecycle of its associated `Profile` / `BrowserState`. Observers must detach prior to destruction or during `OnIdentityManagerShutdown()`.
- **Observer Notification Safety**: Observers receiving `OnPrimaryAccountChanged()` must not mutate the primary account synchronously within the callback to avoid corrupting event state for subsequent observers.
- **Platform Sign-Out Restrictions**: Modifying or clearing the primary account via `PrimaryAccountMutator` is strictly constrained by platform policies (e.g. clearing primary accounts is disallowed on ChromeOS).
- **Asynchronous Token Completion**: `AccessTokenFetcher` and `PrimaryAccountAccessTokenFetcher` invoke `TokenCallback` at most once per request; callers may safely delete the fetcher object directly inside the completion callback.
- **Undefined Sign-In/Out Ordering**: The relative order between `OnPrimaryAccountChanged` and refresh token observer callbacks (`OnRefreshTokenUpdatedForAccount` / `OnRefreshTokenRemovedForAccount`) is undefined. Consumers requiring coordinated state should use `PrimaryAccountAccessTokenFetcher`.

## Side Effects
- **Credential & Preference Storage**: Writes refresh tokens, device IDs, and binding keys to `TokenWebData` or system credential backends. Persists account preferences to `PrefService` (local state and profile prefs).
- **Network & Gaia IPC**: Dispatches HTTPS requests via `network::SharedURLLoaderFactory` and `GaiaAuthFetcher` to Gaia endpoints for token minting, token revocation, multilogin cookie reconciliation, and user profile/capabilities fetching.
- **Cookie Jar Updates**: Modifies browser cookies in `network::mojom::CookieManager` across default and partitioned contexts upon sign-in, account reconciliation, or logout.
- **Observer Event Broadcasts**: Fires observer notifications on `IdentityManager::Observer` and `IdentityManager::DiagnosticsObserver` when account, token, capability, or cookie jar states change.

## Verification
- **Build**: `autoninja -C out/Default components_unittests`
- **Test**: `out/Default/components_unittests --gtest_filter="IdentityManagerTest.*:AccessTokenFetcherTest.*:PrimaryAccountMutatorTest.*:AccountsMutatorTest.*:AccountsCookieMutatorTest.*:AccountCapabilitiesTest.*:IdentityTestEnvironmentTest.*"`
