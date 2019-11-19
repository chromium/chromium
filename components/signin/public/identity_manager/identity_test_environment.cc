// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_test_environment.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"
#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"
#include "components/signin/internal/identity_manager/primary_account_policy_manager_impl.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"

#if defined(OS_IOS)
#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"
#endif

#if !defined(OS_ANDROID) && !defined(OS_IOS)
#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"
#endif

namespace signin {

class IdentityManagerDependenciesOwner {
 public:
  IdentityManagerDependenciesOwner(
      sync_preferences::TestingPrefServiceSyncable* pref_service,
      TestSigninClient* test_signin_client);
  ~IdentityManagerDependenciesOwner();

  sync_preferences::TestingPrefServiceSyncable* pref_service();

  TestSigninClient* signin_client();

 private:
  // Depending on whether a |pref_service| instance is passed in
  // the constructor, exactly one of these will be non-null.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      owned_pref_service_;
  sync_preferences::TestingPrefServiceSyncable* raw_pref_service_ = nullptr;

  std::unique_ptr<TestSigninClient> owned_signin_client_;
  TestSigninClient* raw_signin_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(IdentityManagerDependenciesOwner);
};

IdentityManagerDependenciesOwner::IdentityManagerDependenciesOwner(
    sync_preferences::TestingPrefServiceSyncable* pref_service_param,
    TestSigninClient* signin_client_param)
    : owned_pref_service_(
          pref_service_param
              ? nullptr
              : std::make_unique<
                    sync_preferences::TestingPrefServiceSyncable>()),
      raw_pref_service_(pref_service_param),
      owned_signin_client_(
          signin_client_param
              ? nullptr
              : std::make_unique<TestSigninClient>(pref_service())),
      raw_signin_client_(signin_client_param) {}

IdentityManagerDependenciesOwner::~IdentityManagerDependenciesOwner() = default;

sync_preferences::TestingPrefServiceSyncable*
IdentityManagerDependenciesOwner::pref_service() {
  DCHECK(raw_pref_service_ || owned_pref_service_);
  DCHECK(!(raw_pref_service_ && owned_pref_service_));

  return raw_pref_service_ ? raw_pref_service_ : owned_pref_service_.get();
}

TestSigninClient* IdentityManagerDependenciesOwner::signin_client() {
  DCHECK(raw_signin_client_ || owned_signin_client_);
  DCHECK(!(raw_signin_client_ && owned_signin_client_));

  return raw_signin_client_ ? raw_signin_client_ : owned_signin_client_.get();
}

IdentityTestEnvironment::IdentityTestEnvironment(
    network::TestURLLoaderFactory* test_url_loader_factory,
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    AccountConsistencyMethod account_consistency,
    TestSigninClient* test_signin_client)
    : IdentityTestEnvironment(
          std::make_unique<IdentityManagerDependenciesOwner>(
              pref_service,
              test_signin_client),
          test_url_loader_factory,
          account_consistency) {
  DCHECK(!test_url_loader_factory || !test_signin_client);
}

IdentityTestEnvironment::IdentityTestEnvironment(
    IdentityManager* identity_manager) {
  DCHECK(identity_manager);
  raw_identity_manager_ = identity_manager;
  Initialize();
}

void IdentityTestEnvironment::Initialize() {
  DCHECK(base::ThreadTaskRunnerHandle::Get())
      << "IdentityTestEnvironment requires a properly set up task "
         "environment. "
         "If your test has an existing one, move it to be initialized before "
         "IdentityTestEnvironment. Otherwise, use "
         "base::test::TaskEnvironment.";
  DCHECK(identity_manager()
             ->GetTokenService()
             ->IsFakeProfileOAuth2TokenServiceForTesting())
      << "IdentityTestEnvironment requires the ProfileOAuth2TokenService used "
         "to subclass FakeProfileOAuth2TokenServiceForTesting.";
  test_identity_manager_observer_ =
      std::make_unique<TestIdentityManagerObserver>(this->identity_manager());
  this->identity_manager()->AddDiagnosticsObserver(this);
}

IdentityTestEnvironment::IdentityTestEnvironment(
    std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner,
    network::TestURLLoaderFactory* test_url_loader_factory,
    AccountConsistencyMethod account_consistency) {
  dependencies_owner_ = std::move(dependencies_owner);
  TestSigninClient* test_signin_client = dependencies_owner_->signin_client();
  if (test_url_loader_factory)
    test_signin_client->OverrideTestUrlLoaderFactory(test_url_loader_factory);

  sync_preferences::TestingPrefServiceSyncable* test_pref_service =
      dependencies_owner_->pref_service();

  IdentityManager::RegisterProfilePrefs(test_pref_service->registry());
  IdentityManager::RegisterLocalStatePrefs(test_pref_service->registry());

  owned_identity_manager_ =
      BuildIdentityManagerForTests(test_signin_client, test_pref_service,
                                   base::FilePath(), account_consistency);

  Initialize();
}

// static
std::unique_ptr<IdentityManager>
IdentityTestEnvironment::BuildIdentityManagerForTests(
    SigninClient* signin_client,
    PrefService* pref_service,
    base::FilePath user_data_dir,
    AccountConsistencyMethod account_consistency) {
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(pref_service, user_data_dir);

  auto token_service =
      std::make_unique<FakeProfileOAuth2TokenService>(pref_service);

  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(
      signin_client, token_service.get(), account_tracker_service.get(),
      std::make_unique<image_fetcher::FakeImageDecoder>());

  std::unique_ptr<PrimaryAccountPolicyManager> policy_manager;
#if !defined(OS_CHROMEOS)
  policy_manager =
      std::make_unique<PrimaryAccountPolicyManagerImpl>(signin_client);
#endif
  std::unique_ptr<PrimaryAccountManager> primary_account_manager =
      std::make_unique<PrimaryAccountManager>(
          signin_client, token_service.get(), account_tracker_service.get(),
          account_consistency, std::move(policy_manager));
  primary_account_manager->Initialize(pref_service);

  std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service =
      std::make_unique<GaiaCookieManagerService>(token_service.get(),
                                                 signin_client);

  std::unique_ptr<PrimaryAccountMutator> primary_account_mutator =
      std::make_unique<PrimaryAccountMutatorImpl>(account_tracker_service.get(),
                                                  primary_account_manager.get(),
                                                  pref_service);

  std::unique_ptr<AccountsMutator> accounts_mutator;
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  accounts_mutator = std::make_unique<AccountsMutatorImpl>(
      token_service.get(), account_tracker_service.get(),
      primary_account_manager.get(), pref_service);
#endif

  auto diagnostics_provider = std::make_unique<DiagnosticsProviderImpl>(
      token_service.get(), gaia_cookie_manager_service.get());

  auto accounts_cookie_mutator = std::make_unique<AccountsCookieMutatorImpl>(
      gaia_cookie_manager_service.get(), account_tracker_service.get());

  std::unique_ptr<DeviceAccountsSynchronizer> device_accounts_synchronizer;
#if defined(OS_IOS)
  device_accounts_synchronizer =
      std::make_unique<DeviceAccountsSynchronizerImpl>(
          token_service->GetDelegate());
#endif

  return std::make_unique<IdentityManager>(
      std::move(account_tracker_service), std::move(token_service),
      std::move(gaia_cookie_manager_service),
      std::move(primary_account_manager), std::move(account_fetcher_service),
      std::move(primary_account_mutator), std::move(accounts_mutator),
      std::move(accounts_cookie_mutator), std::move(diagnostics_provider),
      std::move(device_accounts_synchronizer));
}

IdentityTestEnvironment::~IdentityTestEnvironment() {
  // Remove the Observer that IdentityTestEnvironment added during its
  // initialization.
  identity_manager()->RemoveDiagnosticsObserver(this);
}

IdentityManager* IdentityTestEnvironment::identity_manager() {
  DCHECK(raw_identity_manager_ || owned_identity_manager_);
  DCHECK(!(raw_identity_manager_ && owned_identity_manager_));

  return raw_identity_manager_ ? raw_identity_manager_
                               : owned_identity_manager_.get();
}

TestIdentityManagerObserver*
IdentityTestEnvironment::identity_manager_observer() {
  return test_identity_manager_observer_.get();
}

CoreAccountInfo IdentityTestEnvironment::SetPrimaryAccount(
    const std::string& email) {
  return signin::SetPrimaryAccount(identity_manager(), email);
}

void IdentityTestEnvironment::SetRefreshTokenForPrimaryAccount() {
  signin::SetRefreshTokenForPrimaryAccount(identity_manager());
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForPrimaryAccount() {
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());
}

void IdentityTestEnvironment::RemoveRefreshTokenForPrimaryAccount() {
  signin::RemoveRefreshTokenForPrimaryAccount(identity_manager());
}

AccountInfo IdentityTestEnvironment::MakePrimaryAccountAvailable(
    const std::string& email) {
  return signin::MakePrimaryAccountAvailable(identity_manager(), email);
}

void IdentityTestEnvironment::ClearPrimaryAccount(
    ClearPrimaryAccountPolicy policy) {
  signin::ClearPrimaryAccount(identity_manager(), policy);
}

AccountInfo IdentityTestEnvironment::MakeAccountAvailable(
    const std::string& email) {
  return signin::MakeAccountAvailable(identity_manager(), email);
}

AccountInfo IdentityTestEnvironment::MakeAccountAvailableWithCookies(
    const std::string& email,
    const std::string& gaia_id) {
  return signin::MakeAccountAvailableWithCookies(
      identity_manager(),
      dependencies_owner_->signin_client()->GetTestURLLoaderFactory(), email,
      gaia_id);
}

void IdentityTestEnvironment::SetRefreshTokenForAccount(
    const CoreAccountId& account_id) {
  return signin::SetRefreshTokenForAccount(identity_manager(), account_id);
}

void IdentityTestEnvironment::SetInvalidRefreshTokenForAccount(
    const CoreAccountId& account_id) {
  return signin::SetInvalidRefreshTokenForAccount(identity_manager(),
                                                  account_id);
}

void IdentityTestEnvironment::RemoveRefreshTokenForAccount(
    const CoreAccountId& account_id) {
  return signin::RemoveRefreshTokenForAccount(identity_manager(), account_id);
}

void IdentityTestEnvironment::UpdatePersistentErrorOfRefreshTokenForAccount(
    const CoreAccountId& account_id,
    const GoogleServiceAuthError& auth_error) {
  return signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager(), account_id, auth_error);
}

void IdentityTestEnvironment::SetCookieAccounts(
    const std::vector<CookieParamsForTest>& cookie_accounts) {
  signin::SetCookieAccounts(
      identity_manager(),
      dependencies_owner_->signin_client()->GetTestURLLoaderFactory(),
      cookie_accounts);
}

void IdentityTestEnvironment::SetAutomaticIssueOfAccessTokens(bool grant) {
  fake_token_service()->set_auto_post_fetch_response_on_message_loop(grant);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  fake_token_service()->IssueTokenForAllPendingRequests(
      OAuth2AccessTokenConsumer::TokenResponse(token, expiration, id_token));
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const CoreAccountId& account_id,
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token) {
  WaitForAccessTokenRequestIfNecessary(account_id);
  fake_token_service()->IssueAllTokensForAccount(
      account_id,
      OAuth2AccessTokenConsumer::TokenResponse(token, expiration, id_token));
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token,
        const identity::ScopeSet& scopes) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  fake_token_service()->IssueTokenForScope(
      scopes,
      OAuth2AccessTokenConsumer::TokenResponse(token, expiration, id_token));
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary(base::nullopt);
  fake_token_service()->IssueErrorForAllPendingRequests(error);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const CoreAccountId& account_id,
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary(account_id);
  fake_token_service()->IssueErrorForAllPendingRequestsForAccount(account_id,
                                                                  error);
}

void IdentityTestEnvironment::SetCallbackForNextAccessTokenRequest(
    base::OnceClosure callback) {
  on_access_token_requested_callback_ = std::move(callback);
}

IdentityTestEnvironment::AccessTokenRequestState::AccessTokenRequestState() =
    default;
IdentityTestEnvironment::AccessTokenRequestState::~AccessTokenRequestState() =
    default;
IdentityTestEnvironment::AccessTokenRequestState::AccessTokenRequestState(
    AccessTokenRequestState&& other) = default;
IdentityTestEnvironment::AccessTokenRequestState&
IdentityTestEnvironment::AccessTokenRequestState::operator=(
    AccessTokenRequestState&& other) = default;

void IdentityTestEnvironment::OnAccessTokenRequested(
    const CoreAccountId& account_id,
    const std::string& consumer_id,
    const identity::ScopeSet& scopes) {
  // Post a task to handle this access token request in order to support the
  // case where the access token request is handled synchronously in the
  // production code, in which case this callback could be coming in ahead
  // of an invocation of WaitForAccessTokenRequestIfNecessary() that will be
  // made in this same iteration of the run loop.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdentityTestEnvironment::HandleOnAccessTokenRequested,
                     weak_ptr_factory_.GetWeakPtr(), account_id));
}

void IdentityTestEnvironment::HandleOnAccessTokenRequested(
    CoreAccountId account_id) {
  if (on_access_token_requested_callback_) {
    std::move(on_access_token_requested_callback_).Run();
    return;
  }

  for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
    if (!it->account_id || (it->account_id.value() == account_id)) {
      if (it->state == AccessTokenRequestState::kAvailable)
        return;
      if (it->on_available)
        std::move(it->on_available).Run();
      requesters_.erase(it);
      return;
    }
  }

  // A requests came in for a request for which we are not waiting. Record
  // that it's available.
  requesters_.emplace_back();
  requesters_.back().state = AccessTokenRequestState::kAvailable;
  requesters_.back().account_id = account_id;
}

void IdentityTestEnvironment::WaitForAccessTokenRequestIfNecessary(
    base::Optional<CoreAccountId> account_id) {
  // Handle HandleOnAccessTokenRequested getting called before
  // WaitForAccessTokenRequestIfNecessary.
  if (account_id) {
    for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
      if (it->account_id && it->account_id.value() == account_id.value()) {
        // Can't wait twice for same thing.
        DCHECK_EQ(AccessTokenRequestState::kAvailable, it->state);
        requesters_.erase(it);
        return;
      }
    }
  } else {
    for (auto it = requesters_.begin(); it != requesters_.end(); ++it) {
      if (it->state == AccessTokenRequestState::kAvailable) {
        requesters_.erase(it);
        return;
      }
    }
  }

  base::RunLoop run_loop;
  requesters_.emplace_back();
  requesters_.back().state = AccessTokenRequestState::kPending;
  requesters_.back().account_id = std::move(account_id);
  requesters_.back().on_available = run_loop.QuitClosure();
  run_loop.Run();
}

FakeProfileOAuth2TokenService* IdentityTestEnvironment::fake_token_service() {
  // We can't absolutely guarantee that IdentityTestEnvironment was not given an
  // IdentityManager that uses a non-fake FakeProfileOAuth2TokenService. If that
  // ever happens, this will blow up. There doesn't seem to be a better option.
  return static_cast<FakeProfileOAuth2TokenService*>(
      identity_manager()->GetTokenService());
}

void IdentityTestEnvironment::UpdateAccountInfoForAccount(
    AccountInfo account_info) {
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);
}

void IdentityTestEnvironment::ResetToAccountsNotYetLoadedFromDiskState() {
  fake_token_service()->set_all_credentials_loaded_for_testing(false);
}

void IdentityTestEnvironment::ReloadAccountsFromDisk() {
  fake_token_service()->LoadCredentials(CoreAccountId());
}

bool IdentityTestEnvironment::IsAccessTokenRequestPending() {
  return fake_token_service()->GetPendingRequests().size();
}

void IdentityTestEnvironment::SetFreshnessOfAccountsInGaiaCookie(
    bool accounts_are_fresh) {
  signin::SetFreshnessOfAccountsInGaiaCookie(identity_manager(),
                                             accounts_are_fresh);
}

void IdentityTestEnvironment::EnableRemovalOfExtendedAccountInfo() {
  identity_manager()->GetAccountFetcherService()->EnableAccountRemovalForTest();
}

void IdentityTestEnvironment::SimulateSuccessfulFetchOfAccountInfo(
    const CoreAccountId& account_id,
    const std::string& email,
    const std::string& gaia,
    const std::string& hosted_domain,
    const std::string& full_name,
    const std::string& given_name,
    const std::string& locale,
    const std::string& picture_url) {
  signin::SimulateSuccessfulFetchOfAccountInfo(
      identity_manager(), account_id, email, gaia, hosted_domain, full_name,
      given_name, locale, picture_url);
}

void IdentityTestEnvironment::SimulateMergeSessionFailure(
    const GoogleServiceAuthError& auth_error) {
  // GaiaCookieManagerService changes the visibility of inherited method
  // OnMergeSessionFailure from public to private. Cast to a base class
  // pointer to call the method.
  static_cast<GaiaAuthConsumer*>(
      identity_manager()->GetGaiaCookieManagerService())
      ->OnMergeSessionFailure(auth_error);
}

}  // namespace signin
