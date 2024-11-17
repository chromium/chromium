// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_ENVIRONMENT_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_ENVIRONMENT_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/signin/public/base/signin_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/scope_set.h"

class FakeProfileOAuth2TokenService;
class IdentityTestEnvironmentBrowserStateAdaptor;
class IdentityTestEnvironmentProfileAdaptor;
class PrefService;
class TestSigninClient;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace account_manager {
class AccountManagerFacade;
}

namespace ash {
class AccountManagerFactory;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace sync_preferences {
class TestingPrefServiceSyncable;
}

namespace network {
class TestURLLoaderFactory;
}

namespace signin {

class IdentityManagerDependenciesOwner;
class TestIdentityManagerObserver;

// Arguments for `IdentityTestEnvironment::MakeAccountAvailable()`. Keeps
// references, so do not rely on it for storage.
//
// Declared here to be usable as a default value.
struct SimpleAccountAvailabilityOptions {
  // The requested consent level for the account. If present, the account
  // will be set as primary at `primary_account_consent_level`.
  std::optional<ConsentLevel> primary_account_consent_level = std::nullopt;

  // Whether to add the account to the Gaia cookies.
  bool set_cookie = false;

  // If non-empty, the Gaia ID to use when adding the account.
  std::string_view gaia_id;
};

// Class that creates an IdentityManager for use in testing contexts and
// provides facilities for driving that IdentityManager. The IdentityManager
// instance is brought up in an environment where the primary account is
// not available; call MakePrimaryAccountAvailable() as needed.
// NOTE: IdentityTestEnvironment requires that tests have a properly set up
// task environment. If your test doesn't already have one, use a
// base::test::TaskEnvironment instance variable to fulfill this
// requirement.
class IdentityTestEnvironment : public IdentityManager::DiagnosticsObserver,
                                public IdentityManager::Observer {
 public:
  struct PendingRequest {
    PendingRequest(CoreAccountId account_id,
                   std::string client_id,
                   std::string client_secret,
                   OAuth2AccessTokenManager::ScopeSet scopes);
    PendingRequest(const PendingRequest&);
    ~PendingRequest();
    CoreAccountId account_id;
    std::string client_id;
    std::string client_secret;
    OAuth2AccessTokenManager::ScopeSet scopes;
  };

  // Preferred constructor: constructs an IdentityManager object and its
  // dependencies internally. Cannot be used if the client of this class
  // is still interacting directly with those dependencies (e.g., if
  // IdentityTestEnvironment is being introduced to incrementally convert
  // a test). In that case, use the below constructor and switch to this
  // constructor once the conversion is complete.
  //
  // This constructor takes an optional parameter |test_url_loader_factory| to
  // use for cookie-related network requests.
  // Note: the provided |test_url_loader_factory| is expected to outlive
  // IdentityTestEnvironment.
  //
  // This constructor also takes an optional PrefService instance as parameter,
  // which allows tests to move away from referencing IdentityManager's
  // dependencies directly (namely AccountTrackerService, PO2TS), but still be
  // able to tweak preferences on demand.
  //
  // A specific TestSigninClient instance can be passed optionally. If it is
  // null, the test environment will automatically build one internally.
  //
  // Note: at least one of |test_url_loader_factory| and |test_signin_client|
  // must be nulltpr. They cannot both be specified at the same time.
  explicit IdentityTestEnvironment(
      network::TestURLLoaderFactory* test_url_loader_factory = nullptr,
      sync_preferences::TestingPrefServiceSyncable* pref_service = nullptr,
      TestSigninClient* test_signin_client = nullptr);

  IdentityTestEnvironment(const IdentityTestEnvironment&) = delete;
  IdentityTestEnvironment& operator=(const IdentityTestEnvironment&) = delete;

  ~IdentityTestEnvironment() override;

  // The IdentityManager instance associated with this instance.
  IdentityManager* identity_manager();
  const IdentityManager* identity_manager() const;

  SigninClient* signin_client();

  // Returns the |TestIdentityManagerObserver| watching the IdentityManager.
  TestIdentityManagerObserver* identity_manager_observer();

  // Blocks until LoadCredentials is complete and OnRefreshTokensLoaded is
  // invoked.
  void WaitForRefreshTokensLoaded();

  // Sets the primary account for the given email address, generating a GAIA ID
  // that corresponds uniquely to that email address. On non-ChromeOS, results
  // in the firing of the IdentityManager and PrimaryAccountManager callbacks
  // for signin success. Blocks until the primary account is set. Returns the
  // CoreAccountInfo of the newly-set account.
  //
  // See `MakePrimaryAccountAvailable()` for a method that also adds a refresh
  // token for the account, or `MakeAccountAvailable()` for the more openly
  // configurable equivalent.
  CoreAccountInfo SetPrimaryAccount(const std::string& email,
                                    ConsentLevel consent_level);

  // Sets a refresh token for the primary account (which must already be set).
  // Before updating the refresh token, blocks until refresh tokens are loaded.
  // After updating the token, blocks until the update is processed by
  // |identity_manager|.
  void SetRefreshTokenForPrimaryAccount();

  // Sets a special invalid refresh token for the primary account (which must
  // already be set). Before updating the refresh token, blocks until refresh
  // tokens are loaded. After updating the token, blocks until the update is
  // processed by |identity_manager|.
  void SetInvalidRefreshTokenForPrimaryAccount();

  // Removes any refresh token for the primary account, if present. Blocks until
  // the refresh token is removed.
  // NOTE: Call EnableRemovalOfExtendedAccountInfo() before this if the test
  // expects IdentityManager::Observer::OnExtendedAccountInfoRemoved() to fire.
  void RemoveRefreshTokenForPrimaryAccount();

  // Makes the primary account available for the given email address, generating
  // a GAIA ID and refresh token that correspond uniquely to that email address.
  // On non-ChromeOS platforms, this will also result in the firing of the
  // IdentityManager and PrimaryAccountManager callbacks for signin success. On
  // all platforms, this method blocks until the primary account is available.
  // Returns the AccountInfo of the newly-available account.
  //
  // See `MakeAccountAvailable()` for a more configurable equivalent.
  AccountInfo MakePrimaryAccountAvailable(const std::string& email,
                                          ConsentLevel consent_level);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Revokes sync consent from the primary account: the primary account is left
  // at ConsentLevel::kSignin.
  void RevokeSyncConsent();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  // Clears the primary account, removes all accounts and revokes the sync
  // consent. Blocks until the primary account is cleared.
  void ClearPrimaryAccount();

  // Makes an account available for the given email address, generating a
  // refresh token that correspond uniquely to that email address. Blocks until
  // the account is available.
  //
  // Returns the AccountInfo of the newly-available account.
  //
  // 2 variants are available:
  // - The one accepting `SimpleAccountAvailabilityOptions` allows setting
  //   common flags inline using designated initializers.
  // - The one accepting `AccountAvailabilityOptions` exposes the full
  //   configuration options and requires obtaining a builder to construct the
  //   options object. See `CreateAccountAvailabilityOptionsBuilder()`.
  AccountInfo MakeAccountAvailable(
      std::string_view email,
      SimpleAccountAvailabilityOptions options = {});

  AccountInfo MakeAccountAvailable(const AccountAvailabilityOptions& options);

  AccountAvailabilityOptionsBuilder CreateAccountAvailabilityOptionsBuilder();

  // Sets a refresh token for the given account (which must already be
  // available). Before updating the refresh token, blocks until refresh tokens
  // are loaded. After updating the token, blocks until the update is processed
  // by |identity_manager|. NOTE: See disclaimer at top of file re: direct
  // usage.
  void SetRefreshTokenForAccount(const CoreAccountId& account_id);

  // Sets a special invalid refresh token for the given account (which must
  // already be available). Before updating the refresh token, blocks until
  // refresh tokens are loaded. After updating the token, blocks until the
  // update is processed by |identity_manager|. NOTE: See disclaimer at top of
  // file re: direct usage.
  void SetInvalidRefreshTokenForAccount(const CoreAccountId& account_id);

  // Removes any refresh token that is present for the given account. Blocks
  // until the refresh token is removed.
  // NOTE: See disclaimer at top of file re: direct usage.
  // NOTE: Call EnableRemovalOfExtendedAccountInfo() before this if the test
  // expects IdentityManager::Observer::OnExtendedAccountInfoRemoved() to fire.
  void RemoveRefreshTokenForAccount(const CoreAccountId& account_id);

  // Updates the persistent auth error set on |account_id| which must be a known
  // account, i.e., an account with a refresh token.
  void UpdatePersistentErrorOfRefreshTokenForAccount(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& auth_error);

  // Puts the given accounts into the Gaia cookie, replacing any previous
  // accounts. Blocks until the accounts have been set.
  void SetCookieAccounts(
      const std::vector<CookieParamsForTest>& cookie_accounts);

  // Triggers a fake /ListAccount call with the current accounts in the cookie
  // jar. It will notify all observers.
  void TriggerListAccount();

  // When this is set, access token requests will be automatically granted with
  // an access token value of "access_token".
  void SetAutomaticIssueOfAccessTokens(bool grant);

  // Issues |token| in response to any access token request that either has (a)
  // already occurred and has not been matched by a previous call to this or
  // other WaitFor... method, or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // |id_token| is an uncommonly-needed parameter that contains extra
  // information regarding the user's currently-registered services; if this
  // means nothing to you, you don't need to concern yourself with it.
  // NOTE: This method behaves this way to allow IdentityTestEnvironment to be
  // agnostic with respect to whether access token requests are handled
  // synchronously or asynchronously in the production code.
  // NOTE: This version is suitable for use in the common context where access
  // token requests are only being made for one account. If you need to
  // disambiguate requests coming for different accounts, see the version below.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      const std::string& token,
      const base::Time& expiration,
      const std::string& id_token = std::string());

  // Issues |token| in response to an access token request for |account_id| that
  // either already occurred and has not been matched by a previous call to this
  // or other WaitFor... method , or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // |id_token| is an uncommonly-needed parameter that contains extra
  // information regarding the user's currently-registered services; if this
  // means nothing to you, you don't need to concern yourself with it.
  // NOTE: This method behaves this way to allow
  // IdentityTestEnvironment to be agnostic with respect to whether access token
  // requests are handled synchronously or asynchronously in the production
  // code.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      const CoreAccountId& account_id,
      const std::string& token,
      const base::Time& expiration,
      const std::string& id_token = std::string());

  // Similar to WaitForAccessTokenRequestIfNecessaryAndRespondWithToken above
  // apart from the fact that it issues tokens for a given set of scopes only,
  // instead of issueing all tokens for all requests (the method variant above).
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
      const std::string& token,
      const base::Time& expiration,
      const std::string& id_token,
      const ScopeSet& scopes);

  // Issues |error| in response to any access token request that either has (a)
  // already occurred and has not been matched by a previous call to this or
  // other WaitFor... method, or (b) will occur in the future. In the latter
  // case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow IdentityTestEnvironment to be
  // agnostic with respect to whether access token requests are handled
  // synchronously or asynchronously in the production code.
  // NOTE: This version is suitable for use in the common context where access
  // token requests are only being made for one account. If you need to
  // disambiguate requests coming for different accounts, see the version below.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      const GoogleServiceAuthError& error);

  // Issues |error| in response to an access token request for |account_id| that
  // either has (a) already occurred and has not been matched by a previous call
  // to this or other WaitFor... method, or (b) will occur in the future. In the
  // latter case, waits until the access token request occurs.
  // NOTE: This method behaves this way to allow
  // IdentityTestEnvironment to be agnostic with respect to whether access token
  // requests are handled synchronously or asynchronously in the production
  // code.
  void WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      const CoreAccountId& account_id,
      const GoogleServiceAuthError& error);

  // Sets a callback that will be invoked on the next incoming access token
  // request. Note that this can not be combined with the
  // WaitForAccessTokenRequestIfNecessaryAndRespondWith* methods - you must
  // either wait for the callback to get called, or explicitly reset it by
  // passing in a null callback, before the Wait* methods can be used again.
  void SetCallbackForNextAccessTokenRequest(base::OnceClosure callback);

  // Updates the info for |account_info.account_id|, which must be a known
  // account.
  void UpdateAccountInfoForAccount(AccountInfo account_info);

  // Resets to the state where accounts have not yet been loaded from disk.
  void ResetToAccountsNotYetLoadedFromDiskState();

  // Simulates the reloading of the accounts from disk.
  void ReloadAccountsFromDisk();

  // Returns whether there is a access token request pending.
  bool IsAccessTokenRequestPending();

  // Returns the pending access token requests.
  std::vector<PendingRequest> GetPendingAccessTokenRequests();

  // Sets whether the list of accounts in Gaia cookie jar is fresh and does not
  // need to be updated.
  void SetFreshnessOfAccountsInGaiaCookie(bool accounts_are_fresh);

  // By default, extended account info removal is disabled in testing
  // contexts. This call enables it for tests that require
  // IdentityManager::Observer::OnExtendedAccountInfoRemoved() to fire as
  // expected. TODO(crbug.com/40612138): Enable this unconditionally.
  void EnableRemovalOfExtendedAccountInfo();

  // Simulate account fetching using AccountTrackerService without sending
  // network requests.
  void SimulateSuccessfulFetchOfAccountInfo(const CoreAccountId& account_id,
                                            const std::string& email,
                                            const std::string& gaia,
                                            const std::string& hosted_domain,
                                            const std::string& full_name,
                                            const std::string& given_name,
                                            const std::string& locale,
                                            const std::string& picture_url);

  // Simulates a log out failure with |auth_error| as the error.
  void SimulateGaiaLogOutFailure(const GoogleServiceAuthError& auth_error);

  // Sets the TestURLLoaderFactory used for cookie-related requests. This
  // factory is expected to be the same factory as the one used by SigninClient.
  void SetTestURLLoaderFactory(
      network::TestURLLoaderFactory* test_url_loader_factory);

  // Gets the number of calls to PrepareForFetchingAccountCapabilities() in the
  // account capabilities fetcher factory.
  int GetNumCallsToPrepareForFetchingAccountCapabilities();

 private:
  friend class ::IdentityTestEnvironmentProfileAdaptor;
  friend class ::IdentityTestEnvironmentBrowserStateAdaptor;

  struct AccessTokenRequestState {
    AccessTokenRequestState();
    ~AccessTokenRequestState();
    AccessTokenRequestState(AccessTokenRequestState&& other);
    AccessTokenRequestState& operator=(AccessTokenRequestState&& other);

    enum {
      kPending,
      kAvailable,
    } state;
    std::optional<CoreAccountId> account_id;
    base::OnceClosure on_available;
  };

  // Constructs an IdentityTestEnvironment that builds and owns an
  // IdentityManager. This private constructor is meant to only be called
  // internally by IdentityTestEnvironment from other constructors.
  IdentityTestEnvironment(
      std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner,
      network::TestURLLoaderFactory* test_url_loader_factory);

  // Constructs an IdentityTestEnvironment that uses the supplied
  // |identity_manager| and |signin_client|.
  // For use only in contexts where IdentityManager and its dependencies are all
  // unavoidably created by the embedder (e.g., //chrome-level unittests that
  // use the ProfileKeyedServiceFactory infrastructure).
  // NOTE: This constructor is for usage only in the special case of embedder
  // unittests that must use the IdentityManager instance associated with the
  // Profile. If you think you have another use case for it, contact
  // blundell@chromium.org.
  IdentityTestEnvironment(IdentityManager* identity_manager,
                          SigninClient* signin_client);

  // Shared constructor initialization logic.
  void Initialize();

  // Create an IdentityManager instance for tests.
  static std::unique_ptr<IdentityManager> BuildIdentityManagerForTests(
      SigninClient* signin_client,
      PrefService* pref_service,
      base::FilePath user_data_dir
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      ash::AccountManagerFactory* account_manager_factory,
      account_manager::AccountManagerFacade* account_manager_facade
#endif
  );

  static std::unique_ptr<IdentityManager> FinishBuildIdentityManagerForTests(
      std::unique_ptr<AccountTrackerService> account_tracker_service,
      std::unique_ptr<ProfileOAuth2TokenService> token_service,
      SigninClient* signin_client,
      PrefService* pref_service,
      base::FilePath user_data_dir
#if BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      account_manager::AccountManagerFacade* account_manager_facade
#endif
  );

  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const ScopeSet& scopes) override;

  // IdentityManager::Observer:
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // Handles the notification that an access token request was received for
  // |account_id|. Invokes |on_access_token_request_callback_| if the latter
  // is non-null *and* either |*pending_access_token_requester_| equals
  // |account_id| or |pending_access_token_requester_| is empty.
  void HandleOnAccessTokenRequested(CoreAccountId account_id);

  // If a token request for |account_id| (or any account if nullopt) has already
  // been made and not matched by a different call, returns immediately.
  // Otherwise and runs a nested runloop until a matching access token request
  // is observed.
  void WaitForAccessTokenRequestIfNecessary(
      std::optional<CoreAccountId> account_id);

  // Returns the FakeProfileOAuth2TokenService owned by IdentityManager.
  FakeProfileOAuth2TokenService* fake_token_service();

  // Returns the TestURLLoaderFactory that is either extracted from
  // IdentityManagerDependenciesOwner or set manually via
  // SetTestURLLoaderFactory(). Crashes if the factory wasn't set.
  network::TestURLLoaderFactory* test_url_loader_factory();

  // Owner of all dependencies that don't belong to IdentityManager.
  std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner_;

  // Non-owning pointer to the TestURLLoaderFactory.
  raw_ptr<network::TestURLLoaderFactory> test_url_loader_factory_ = nullptr;

  // If IdentityTestEnvironment doesn't use TestSigninClient, stores a
  // non-owning pointer to the SigninClient.
  raw_ptr<SigninClient, AcrossTasksDanglingUntriaged> raw_signin_client_ =
      nullptr;

  // Depending on which constructor is used, exactly one of these will be
  // non-null. See the documentation on the constructor wherein IdentityManager
  // is passed in for required lifetime invariants in that case.
  std::unique_ptr<IdentityManager> owned_identity_manager_;
  raw_ptr<IdentityManager, AcrossTasksDanglingUntriaged> raw_identity_manager_ =
      nullptr;

  std::unique_ptr<TestIdentityManagerObserver> test_identity_manager_observer_;

  base::ScopedObservation<IdentityManager, IdentityManager::DiagnosticsObserver>
      diagnostics_observation_{this};
  base::ScopedObservation<IdentityManager, IdentityManager::Observer>
      identity_manager_observation_{this};

  base::OnceClosure on_access_token_requested_callback_;
  std::vector<AccessTokenRequestState> requesters_;

  base::WeakPtrFactory<IdentityTestEnvironment> weak_ptr_factory_{this};
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_TEST_ENVIRONMENT_H_
