The internal implementation of //components/signin/public/identity_manager;
visible only to implementation files in
//components/signin/public/identity_manager. This directory contains
implementations of pure interfaces in
//components/signin/public/identity_manager as well as internal classes.

Here is how the various core internal classes support the public functionality
of IdentityManager (see
//components/signin/public/identity_manager/README.md for definitions of core
terms):

- PrimaryAccountManager: Maintains the information of the primary account. When
  it is set/cleared, calls out to IdentityManager via callbacks, resulting in the
  firing of IdentityManager::Observer notifications. Setting/clearing occurs via
  PrimaryAccountMutatorImpl.
  Historically was exposed as SigninManager(Base).
- PrimaryAccountPolicyManager: Helper class for PrimaryAccountManager that
  manages interactions between policy and the primary account, e.g. clearing the
  primary account if signin becomes disallowed by policy. Historically was
  contained within SigninManager.
- ProfileOAuth2TokenService: Manages the OAuth2 tokens for the user's Gaia
  accounts. Delegates much of the functionality for refresh tokens to its
  various platform-specific delegates, and delegates much of the functionality
  for access tokens to the lower-level OAuth2AccessTokenManager class. Supports
  the IdentityManager APIs for querying the state of the accounts with refresh
  tokens and for fetching access tokens via (PrimaryAccount)AccessTokenFetcher.
  Mutation of refresh tokens occurs via platform-specific flows:
  - AcccountsMutatorImpl on desktop
  - DeviceAccountsSynchronizerImpl on iOS
  - Within the token service delegate by observing AccountManager on ChromeOS
  - Via a legacy flow on Android (https://crbug.com/963391)
  Has an observer API (ProfileOAuth2TokenServiceObserver); IdentityManager
  observes token-related events via this API in order to fire
  IdentityManager::Observer events. Other internal classes also observe
  token-related events, which is why PO2TS still has observers rather than
  directly calling out to IdentityManager via callbacks as other internal
  classes do.
- ProfileOAuth2TokenServiceDelegate: Mostly-abstract class via which
  ProfileOAuth2TokenService interacts with platform-specific functionality for
  OAuth2 tokens. Has various platform-specific implementations. The
  platform-specific implementations glue the cross-platform
  ProfileOAuth2TokenService interface to the underlying platform support, and
  dictate the firing of ProfileOAuth2TokenServiceObserver events.
- AccountTrackerService: Maintains the mapping from account ID to account
  information for the user's Gaia accounts with refresh tokens. Supports the
  various IdentityManager::FindAccountInfoXXX() methods. When account info is
  updated/removed for a given account, calls out to IdentityManager via
  callbacks, resulting in the firing of IdentityManager::Observer notifications.
  Conceptually below ProfileOAuth2TokenService, as it is needed by various
  platform-specific implementations of ProfileOAuth2TokenServiceDelegate, which
  themselves are needed by ProfileOAuth2TokenService.
- AccountFetcherService: Fetches account information for the user's Gaia
  accounts with refresh tokens in order to populate this information in
  AccountTrackerService. Observes ProfileOAuth2TokenService in order to observe
  addition and removal of accounts with refresh tokens to initiate
  fetching/removal of account information. Split from AccountTrackerService in
  order to support knowledge of ProfileOAuth2TokenService.
- GaiaCookieManagerService: Maintains information about the accounts in the Gaia
  cookie. Supports IdentityManager::GetAccountsInCookieJar(). When the accounts
  in the cookie are updated or the cookie itself is deleted, calls out to
  IdentityManager via callbacks, resulting in the firing of
  IdentityManager::Observer notifications.

The layering of the core internal classes is as follows, with higher-level classes
listed first:

- PrimaryAccountManager
- AccountFetcherService
- GaiaCookieManagerService (note: GCMS and AFS are independent peers)
- ProfileOAuth2TokenService 
- various ProfileOAuth2TokenServiceDelegate implementations
- AccountTrackerService
