The core public API surfaces for interacting with Google identity. Header files
in this directory are allowed to depend only on the following other parts of the
signin component:

//components/signin/public/base
//components/signin/public/webdata

Implementation files in this directory, however, are additionally allowed to
depend on //components/signin/internal/identity_manager.

Here we take a quick guide through the core concepts (note: Documentation on
specific IdentityManager and IdentityManager::Observer methods can be found as
method-level comments in identity_manager.h; this guide defines the core
concepts themselves and gives a high-level mapping between these core concepts
and the relevant IdentityManager(::Observer) API surfaces).

# Accounts
- "Account" always refers to a Gaia account.
- An account has three core pieces of information, which are collected together
  in the CoreAccountInfo struct and are available for the duration of the
  account being visible to IdentityManager:
  - The email address.
  - The Gaia ID.
  - The account ID. This is an opaque Chrome-specific identifier for the
    account.
- The AccountInfo struct contains extra "extended" information about the account
  that may become available only asynchronously after the account is first added
  to Chrome. To interact with the extended account info, use the IdentityManager
  methods with "ExtendedAccountInfo" in their names. To observe changes in the
  state of the extended account info for one or more accounts, observe
  IdentityManager and override one or more of the IdentityManager::Observer
  methods with "ExtendedAccountInfo" in their names.

# The Primary Account
- "Primary account" in IdentityManager refers to the account that has been
  blessed for sync by the user (what in Chromium historically was often referred
  to as the "authenticated account").
- "Unconsented primary account" is intuitively the browsing identity of the user
  that we display to the user; the user may or may not have blessed this account
  for sync. In particular, whenever a primary account exists, the unconsented
  primary account equals to the primary account. On desktop platforms (excl.
  ChromeOS), if no primary account exists and there exist any content-area
  accounts, the unconsented primary account equals to the first signed-in content-
  area account. In all other cases there is no unconsented primary account.
  NOTE: This name is still subject to finalization. The problem with
  "unconsented" in this context is that it means "did not consent"; really, this
  account is the "possibly unconsented, possibly primary, default account", which
  is a mouthful :).
- To interact with the primary account and/or unconsented primary account, use
  the IdentityManager methods with "PrimaryAccount" in their names. To observe
  changes in the presence of either of these accounts, observe IdentityManager
  and override one or more of the methods with "PrimaryAccount" in their names
  as desired.
- PrimaryAccountTokenFetcher is the primary client-side interface for obtaining
  OAuth2 access tokens for the primary account (see the next section for the
  discussion of OAuth2 tokens). In particular, it can handle the common use case
  of "wait until the primary account is available and then fetch an access token
  for it" transparently on behalf of the client. See
  primary_account_access_token_fetcher.h for usage explanation and examples.

# OAuth2 Access and Refresh Tokens
- "OAuth2 tokens" are tokens related to the OAuth2 client-server authorization
  protocol. "OAuth2 refresh tokens" or just "refresh tokens" are long-lived
  tokens that the browser obtains via the user explicitly adding an account.
  Clients of IdentityManager do not explicitly see refresh tokens, but rather use
  IdentityManager to obtain "OAuth2 access tokens" or just "access tokens".
  Access tokens are short-lived tokens with given scopes that can be used to make
  authorized requests to Gaia endpoints.
- "The accounts with refresh tokens" refer to the accounts that are visible to
  IdentityManager with OAuth2 refresh tokens present (e.g., because the user has
    signed in to the browser and embedder-level code then added the account to
    IdentityManager, or because the user has added an account at the
    system level that embedder-level code then made visible to IdentityManager).
  To interact with these accounts, use the IdentityManager methods with
  "RefreshToken" in their name. To observe changes in the state of one or more
  of these accounts, observe IdentityManager and override one or more of the
  IdentityManager::Observer methods with "RefreshToken" in their name.
- AccessTokenFetcher is the client-side interface for obtaining access tokens
  for arbitrary accounts; see access_token_fetcher.h for usage explanation and
  examples.

# The Gaia Cookie
- "The Gaia cookie" refers to the cookie that contains the information of the
  user's Gaia accounts that are available on the web.
- "The accounts in the Gaia cookie" and "the accounts in the cookie jar" refer to
  the set of accounts in this cookie. To interact with these accounts, use the
  IdentityManager methods with "Cookie" in their name. To observe changes in the
  state of one or more of these accounts, observe IdentityManager and override
  one or more of the IdentityManager::Observer methods with "Cookie" in their
  name. Note that as the information that Chrome has about these accounts is
  fundamentally different than that which Chrome has about the user's accounts
  with OAuth2 refresh tokens, the struct encoding this information is also
  distinct: see accounts_in_cookie_jar_info.h.

# Interacting with IdentityManager and Friends in Tests
- IdentityTestEnvironment is the preferred test infrastructure for unittests
  of production code that interacts with IdentityManager. It is suitable for
  use in cases where neither the production code nor the unittest is interacting
  with Profile.
- identity_test_utils.h provides lower-level test facilities for interacting
  explicitly with IdentityManager. These facilities are the way to interact with
  IdentityManager in testing contexts where the production code and/or the
  unittest are interacting with Profile (in particular, where the
  IdentityManager instance with which the test is interacting must be
  IdentityManagerFactory::GetForProfile(profile)). Examples include integration
  tests and Profile-based unittests (in the latter case, consider migrating the
  test and production code away from using Profile directly and using
  IdentityTestEnvironment).

# Mutation of Account State
- Various mutators of account state are available through IdentityManager (e.g.,
  PrimaryAccountMutator). These should in general be used only as part of larger
  embedder-specific flows for mutating the user's account state in ways that are
  in line with product specifications. If you are a consumer of
  //components/identity/public/identity_manager and you believe that you have a
  new use case for one of these API surfaces, you should first contact the
  OWNERS of //components/signin to discuss this use case and how best to realize
  it. With these caveats, here are the details of how various operations are
  supported on the various platforms on which Chrome runs:
  * Mutating the primary account: Setting the primary account is done via
    PrimaryAccountMutator::SetPrimaryAccount(); see the comments on the
    declaration of that method for the conditions required for the setting of
    the primary account to succeed. On all platforms other than ChromeOS, the
    primary account can be cleared via
    PrimaryAccountMutator::ClearPrimaryAccount() (on ChromeOS, the primary
    account cannot be cleared as there is no user-level flow to sign out of
    the browser).
  * Updating the state of the accounts in the Gaia cookie: This is supported via
    AccountsCookieMutator.
  * Mutating the set of accounts with refresh tokens: The ways in which this
    occurs are platform-specific due to the differences in underlying platform
    account management (or lack thereof). We detail these below:
    - Windows/Mac/Linux: Chrome manages the user's OAuth2 refresh tokens
      internally. Adding and removing accounts with refresh tokens is done via
      AccountsMutator.
    - ChromeOS: Chrome is backed by ChromeOS' platform-level AccountManager.
      Chrome's view of the accounts with refresh tokens is synchronized with
      the platform-level state by observing that platform-level AccountManager
      internally.
    - Android: Chrome is backed by Android's platform-level AccountManager (in
      Java). Chrome's view of the accounts with refresh tokens is synchronized
      with the platform-level state via IdentityMutator.java.
    - iOS: Chrome is backed by Google's iOS SSO library for supporting shared
      identities between Google's various iOS applications. Chrome's view of the
      accounts with refresh tokens is synchronized with the platform-level state
      via DeviceAccountsSynchronizer.

# Mental Mapping from Chromium's Historical API Surfaces for Signin
Documentation on the mapping between usage of legacy signin
classes (notably PrimaryAccountManager and ProfileOAuth2TokenService) and usage
of IdentityManager is available here:

https://docs.google.com/document/d/14f3qqkDM9IE4Ff_l6wuXvCMeHfSC9TxKezXTCyeaPUY/edit#
