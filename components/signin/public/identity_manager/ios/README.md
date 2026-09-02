# iOS Device Accounts Provider

Freshness: 2026-09-02

## Overview
This package defines the abstract interface and testing utilities for querying system-level Google accounts and generating OAuth2 access tokens on iOS.

Unlike desktop platforms where Chrome manages refresh tokens directly, iOS delegates account management to platform Single Sign-On (SSO) services. `DeviceAccountsProvider` serves as the dependency-inversion abstraction layer between the cross-platform [IdentityManager](../README.md) component (`ProfileOAuth2TokenServiceDelegateIOS` in `//components/signin/internal/identity_manager`) and the platform-specific identity backend, whose concrete implementation must be supplied by the embedder of the signin component.

## Interface
- [`DeviceAccountsProvider`](./device_accounts_provider.h): Abstract interface for querying device-level and profile-level accounts (`GetAccountsOnDevice`, `GetAccountsForProfile`) and initiating asynchronous access token requests (`GetAccessToken`).
- [`DeviceAccountsProvider::DeviceAccountInfo`](./device_accounts_provider.h): Value class containing account credentials and metadata (`GaiaId`, email address, hosted domain, and persistent auth error state).
- [`DeviceAccountsProvider::AccessTokenInfo`](./device_accounts_provider.h): Struct representing an issued OAuth2 access token and its expiration timestamp.
- [`DeviceAccountsProvider::Observer`](./device_accounts_provider.h): `base::CheckedObserver` interface for receiving notifications when device accounts change or account details are updated.
- [`FakeDeviceAccountsProvider`](./fake_device_accounts_provider.h): In-memory test fake (provided by `:test_support`) supporting deterministic account management (`AddAccount`, `UpdateAccount`, `ClearAccounts`) and batch access token resolution (`IssueAccessTokenForAllRequests`, `IssueAccessTokenErrorForAllRequests`).

## Invariants
- **Account Identifier Non-Emptiness**: `DeviceAccountInfo` strictly requires non-empty `GaiaId` and email string parameters upon construction, enforcing valid identities via `CHECK`.
- **Observer Lifetime**: Classes implementing `DeviceAccountsProvider::Observer` must be registered with `AddObserver` and unregistered via `RemoveObserver` prior to observer or provider destruction.
- **Single Callback Invocation**: `AccessTokenCallback` is a `base::OnceCallback` and is guaranteed to be invoked exactly once per `GetAccessToken` request with either an `AccessTokenInfo` or a `GoogleServiceAuthError`.

## Side Effects
- **Observer Notifications**: Concrete implementations dispatch `OnAccountsOnDeviceChanged` and `OnAccountOnDeviceUpdated` to registered observers when underlying system identities change on the iOS device.
- **Asynchronous Token Requests**: Initiates token fetch requests against iOS platform SSO services (or synthetic token dispatch in test environments).
- **No Direct Storage State**: This interface does not manage persistent disk storage or write preferences directly; persistence and sync are delegated to the underlying SSO provider and the consumer token service.

## Verification
- **Build**: `autoninja -C out/ios_sim components/signin/public/identity_manager/ios` _(Requires `target_os = "ios"` in args.gn)_
- **Test**: `out/ios_sim/components_unittests --gtest_filter="ProfileOAuth2TokenServiceDelegateIOSTest.*"` _(Tests interface consumers in `//components/signin/internal/identity_manager` on iOS)_
