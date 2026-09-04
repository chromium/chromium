# GaiaCookieManagerService Architecture

Freshness: 2026-09-02

## Overview
[`GaiaCookieManagerService`](../gaia_cookie_manager_service.h) manages Google authentication cookies (the Gaia cookie jar) and keeps the list of signed-in Gaia accounts synchronized with Chrome's identity state.

## Operations & Underlying Server APIs

### 1. Set Accounts (`SetAccountsInCookie`)
- Updates the signed-in accounts in the cookie jar using the `oauth/multilogin` endpoint (via [`signin::OAuthMultiloginHelper`](../oauth_multilogin_helper.h)).
- Fetches access tokens with the login scope for the specified accounts required for the OAuthMultiLogin call.
- Writes cookies in the OAuthMultiLogin response into the network service's `network::mojom::CookieManager` (or device-bound session manager).
- Prior to the first multilogin call, an external connection check (`GaiaCookieManagerService::ExternalCcResultFetcher`) is performed against the `GetCheckConnectionInfo` endpoint to test connectivity to external Google services (e.g., YouTube).

### 2. List Accounts (`ListAccounts`, `TriggerListAccounts`)
- Fetches the list of accounts currently present in the cookie jar via the `ListAccounts?json=standard&laf=b64bin` endpoint.
- Parsed account info is cached in memory, persisted in prefs, and dispatched to observers via `GaiaAccountsInCookieUpdatedCallback`.
- `ListAccounts()` returns cached data if fresh or triggers a fetch if stale.
- `TriggerListAccounts()` always enqueues a fetch.

### 3. Log Out All Accounts (`LogOutAllAccounts`)
- Signs out all accounts by sending a request to the `Logout` endpoint.
- Marks the cached list of accounts as stale and clears Gaia cookies.

## Request Queue Management & ListAccounts Optimization
- All operations are serialized in `requests_` (`base::circular_deque`) so that at most one network request executes at a time.
- Redundant `LOG_OUT` requests are canceled immediately if another logout is already queued.
- `OptimizeListAccounts()` optimizes pending `LIST_ACCOUNTS` requests when they reach the front of the queue:
  1. **Deduplication**: Any subsequent duplicate `LIST_ACCOUNTS` requests in the queue are removed.
  2. **Deprioritization / Reordering**: If any mutating operations (`SET_ACCOUNTS` or `LOG_OUT`) are queued behind `LIST_ACCOUNTS`, the `LIST_ACCOUNTS` request is moved to the end of the queue. This avoids fetching account state that will be immediately invalidated by pending mutations.
- Failed requests are retried with exponential backoff (`fetcher_backoff_`).

## Cookie Change Detection
- `InitCookieListener()` registers this service as a `network::mojom::CookieChangeListener` on the network service's `CookieManager` for `https://google.com`. Connection errors automatically trigger re-registration to recover from network service crashes.
- `OnCookieChange()` filters incoming cookie events on `google.com`:
  1. Changes (additions, modifications, deletions) to the `SAPISID` cookie (`GaiaConstants::kGaiaSigninCookieName`), which indicate session changes.
  2. Explicit deletions (`net::CookieChangeCause::EXPLICIT`) of any `google.com` cookie.
- When a relevant change is detected:
  - The cached list of accounts is marked as stale (`list_accounts_stale_`).
  - If the `SAPISID` cookie was explicitly deleted by user action (e.g., via settings or extensions), `GaiaCookieDeletedByUserActionCallback` is fired.
  - A `/ListAccounts` fetch is triggered (`TriggerListAccounts()`) to re-synchronize the account list and notify observers.
- `ForceOnCookieChangeProcessing()` allows manual triggering of this flow (e.g., in tests or on platforms without Mojo cookie change notifications).
