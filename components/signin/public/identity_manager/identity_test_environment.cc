// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_test_environment.h"

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"
#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"
#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher_factory.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/signin/internal/identity_manager/test_profile_oauth2_token_service_delegate_chromeos.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate_ios.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"
#endif

using TokenResponseBuilder = OAuth2AccessTokenConsumer::TokenResponse::Builder;

namespace signin {

class IdentityManagerDependenciesOwner {
 public:
  IdentityManagerDependenciesOwner(
      sync_preferences::TestingPrefServiceSyncable* pref_service,
      TestSigninClient* test_signin_client);

  IdentityManagerDependenciesOwner(const IdentityManagerDependenciesOwner&) =
      delete;
  IdentityManagerDependenciesOwner& operator=(
      const IdentityManagerDependenciesOwner&) = delete;

  ~IdentityManagerDependenciesOwner();

  sync_preferences::TestingPrefServiceSyncable* pref_service();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AccountManagerFactory* account_manager_factory();
  account_manager::AccountManagerFacade* GetAccountManagerFacadeForEmptyPath();
#endif
  TestSigninClient* signin_client();

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::AccountManagerFactory> account_manager_factory_;
  std::unique_ptr<account_manager::AccountManagerFacadeImpl>
      account_manager_facade_for_empty_path_;
#endif
  // Depending on whether a |pref_service| instance is passed in
  // the constructor, exactly one of these will be non-null.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      owned_pref_service_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> raw_pref_service_ =
      nullptr;
  std::unique_ptr<TestSigninClient> owned_signin_client_;
  raw_ptr<TestSigninClient> raw_signin_client_ = nullptr;
};

IdentityManagerDependenciesOwner::IdentityManagerDependenciesOwner(
    sync_preferences::TestingPrefServiceSyncable* pref_service_param,
    TestSigninClient* signin_client_param)
    :
#if BUILDFLAG(IS_CHROMEOS_ASH)
      account_manager_factory_(std::make_unique<ash::AccountManagerFactory>()),
#endif
      owned_pref_service_(
          pref_service_param
              ? nullptr
              : std::make_unique<
                    sync_preferences::TestingPrefServiceSyncable>()),
      raw_pref_service_(pref_service_param),
      owned_signin_client_(
          signin_client_param
              ? nullptr
              : std::make_unique<TestSigninClient>(pref_service())),
      raw_signin_client_(signin_client_param) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  mojo::Remote<crosapi::mojom::AccountManager> remote;
  crosapi::AccountManagerMojoService* account_manager_mojo_service =
      account_manager_factory_->GetAccountManagerMojoService(std::string());
  account_manager_mojo_service->BindReceiver(
      remote.BindNewPipeAndPassReceiver());
  account_manager_facade_for_empty_path_ =
      std::make_unique<account_manager::AccountManagerFacadeImpl>(
          std::move(remote),
          /*remote_version=*/std::numeric_limits<uint32_t>::max(),
          /*account_manager_for_tests=*/
          account_manager_factory_->GetAccountManager(std::string())
              ->GetWeakPtr());
#endif
}

IdentityManagerDependenciesOwner::~IdentityManagerDependenciesOwner() {
  if (owned_signin_client_)
    owned_signin_client_->Shutdown();
}

sync_preferences::TestingPrefServiceSyncable*
IdentityManagerDependenciesOwner::pref_service() {
  DCHECK(raw_pref_service_ || owned_pref_service_);
  DCHECK(!(raw_pref_service_ && owned_pref_service_));

  return raw_pref_service_ ? raw_pref_service_.get()
                           : owned_pref_service_.get();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
ash::AccountManagerFactory*
IdentityManagerDependenciesOwner::account_manager_factory() {
  DCHECK(account_manager_factory_);
  return account_manager_factory_.get();
}

account_manager::AccountManagerFacade*
IdentityManagerDependenciesOwner::GetAccountManagerFacadeForEmptyPath() {
  return account_manager_facade_for_empty_path_.get();
}
#endif

TestSigninClient* IdentityManagerDependenciesOwner::signin_client() {
  DCHECK(raw_signin_client_ || owned_signin_client_);
  DCHECK(!(raw_signin_client_ && owned_signin_client_));

  return raw_signin_client_ ? raw_signin_client_.get()
                            : owned_signin_client_.get();
}

IdentityTestEnvironment::IdentityTestEnvironment(
    network::TestURLLoaderFactory* test_url_loader_factory,
    sync_preferences::TestingPrefServiceSyncable* pref_service,
    TestSigninClient* test_signin_client)
    : IdentityTestEnvironment(
          std::make_unique<IdentityManagerDependenciesOwner>(
              pref_service,
              test_signin_client),
          test_url_loader_factory) {
  DCHECK(!test_url_loader_factory || !test_signin_client);
}

IdentityTestEnvironment::IdentityTestEnvironment(
    IdentityManager* identity_manager,
    SigninClient* signin_client) {
  DCHECK(identity_manager);
  raw_identity_manager_ = identity_manager;
  raw_signin_client_ = signin_client;
  Initialize();
}

void IdentityTestEnvironment::Initialize() {
  DCHECK(base::SingleThreadTaskRunner::GetCurrentDefault())
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
      std::make_unique<TestIdentityManagerObserver>(identity_manager());
  diagnostics_observation_.Observe(identity_manager());
  identity_manager_observation_.Observe(identity_manager());
}

IdentityTestEnvironment::IdentityTestEnvironment(
    std::unique_ptr<IdentityManagerDependenciesOwner> dependencies_owner,
    network::TestURLLoaderFactory* test_url_loader_factory) {
  dependencies_owner_ = std::move(dependencies_owner);
  TestSigninClient* test_signin_client = dependencies_owner_->signin_client();
  if (test_url_loader_factory)
    test_signin_client->OverrideTestUrlLoaderFactory(test_url_loader_factory);
  test_url_loader_factory_ = test_signin_client->GetTestURLLoaderFactory();

  sync_preferences::TestingPrefServiceSyncable* test_pref_service =
      dependencies_owner_->pref_service();

  IdentityManager::RegisterProfilePrefs(test_pref_service->registry());
  SigninPrefs::RegisterProfilePrefs(test_pref_service->registry());
  IdentityManager::RegisterLocalStatePrefs(test_pref_service->registry());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  account_manager::AccountManager::RegisterPrefs(test_pref_service->registry());

  owned_identity_manager_ = BuildIdentityManagerForTests(
      test_signin_client, test_pref_service, base::FilePath(),
      dependencies_owner_->account_manager_factory(),
      dependencies_owner_->GetAccountManagerFacadeForEmptyPath());
#else
  owned_identity_manager_ = BuildIdentityManagerForTests(
      test_signin_client, test_pref_service, base::FilePath());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  Initialize();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
std::unique_ptr<IdentityManager>
IdentityTestEnvironment::BuildIdentityManagerForTests(
    SigninClient* signin_client,
    PrefService* pref_service,
    base::FilePath user_data_dir,
    ash::AccountManagerFactory* account_manager_factory,
    account_manager::AccountManagerFacade* account_manager_facade) {
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(pref_service, user_data_dir);

  account_manager::AccountManager* account_manager =
      account_manager_factory->GetAccountManager(user_data_dir.value());

  if (user_data_dir.empty()) {
    account_manager->InitializeInEphemeralMode(
        signin_client->GetURLLoaderFactory());
  } else {
    account_manager::AccountManager::DelayNetworkCallRunner
        immediate_callback_runner =
            base::BindRepeating([](base::OnceClosure closure) -> void {
              std::move(closure).Run();
            });
    account_manager->Initialize(user_data_dir,
                                signin_client->GetURLLoaderFactory(),
                                immediate_callback_runner, base::DoNothing());
  }
  account_manager->SetPrefService(pref_service);
  account_manager->SetUrlLoaderFactoryForTests(
      signin_client->GetURLLoaderFactory());

  auto token_service = std::make_unique<FakeProfileOAuth2TokenService>(
      pref_service,
      std::make_unique<TestProfileOAuth2TokenServiceDelegateChromeOS>(
          signin_client, account_tracker_service.get(),
          account_manager_factory->GetAccountManagerMojoService(
              user_data_dir.value()),
          /*is_regular_profile=*/true));

  return FinishBuildIdentityManagerForTests(
      std::move(account_tracker_service), std::move(token_service),
      signin_client, pref_service, user_data_dir, account_manager_facade);
}
#else
// static
std::unique_ptr<IdentityManager>
IdentityTestEnvironment::BuildIdentityManagerForTests(
    SigninClient* signin_client,
    PrefService* pref_service,
    base::FilePath user_data_dir) {
#if BUILDFLAG(IS_ANDROID)
  // Required to create AccountTrackerService on Android. Uses FakeImpl instead
  // of Mockito because it is incompatible for tests that are run on VM.
  SetUpMockAccountManagerFacade(/*useFakeImpl =*/true);
#endif
  auto account_tracker_service = std::make_unique<AccountTrackerService>();
  account_tracker_service->Initialize(pref_service, user_data_dir);
  auto token_service =
      std::make_unique<FakeProfileOAuth2TokenService>(pref_service);
  return FinishBuildIdentityManagerForTests(
      std::move(account_tracker_service), std::move(token_service),
      signin_client, pref_service, user_data_dir);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

IdentityTestEnvironment::PendingRequest::PendingRequest(
    CoreAccountId account_id,
    std::string client_id,
    std::string client_secret,
    OAuth2AccessTokenManager::ScopeSet scopes)
    : account_id(account_id),
      client_id(client_id),
      client_secret(client_secret),
      scopes(scopes) {}

IdentityTestEnvironment::PendingRequest::PendingRequest(const PendingRequest&) =
    default;

IdentityTestEnvironment::PendingRequest::~PendingRequest() = default;

// static
std::unique_ptr<IdentityManager>
IdentityTestEnvironment::FinishBuildIdentityManagerForTests(
    std::unique_ptr<AccountTrackerService> account_tracker_service,
    std::unique_ptr<ProfileOAuth2TokenService> token_service,
    SigninClient* signin_client,
    PrefService* pref_service,
    base::FilePath user_data_dir
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ,
    account_manager::AccountManagerFacade* account_manager_facade
#endif
) {
  auto account_fetcher_service = std::make_unique<AccountFetcherService>();
  account_fetcher_service->Initialize(
      signin_client, token_service.get(), account_tracker_service.get(),
      std::make_unique<image_fetcher::FakeImageDecoder>(),
      std::make_unique<FakeAccountCapabilitiesFetcherFactory>());

  std::unique_ptr<PrimaryAccountManager> primary_account_manager =
      std::make_unique<PrimaryAccountManager>(
          signin_client, token_service.get(), account_tracker_service.get());

  std::unique_ptr<GaiaCookieManagerService> gaia_cookie_manager_service =
      std::make_unique<GaiaCookieManagerService>(
          account_tracker_service.get(), token_service.get(), signin_client);
  IdentityManager::InitParameters init_params;
  init_params.primary_account_mutator =
      std::make_unique<PrimaryAccountMutatorImpl>(account_tracker_service.get(),
                                                  primary_account_manager.get(),
                                                  pref_service, signin_client);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  init_params.accounts_mutator = std::make_unique<AccountsMutatorImpl>(
      token_service.get(), account_tracker_service.get(),
      primary_account_manager.get(), pref_service);
#endif

  init_params.diagnostics_provider = std::make_unique<DiagnosticsProviderImpl>(
      token_service.get(), gaia_cookie_manager_service.get());

  init_params.accounts_cookie_mutator =
      std::make_unique<AccountsCookieMutatorImpl>(
          signin_client, token_service.get(), gaia_cookie_manager_service.get(),
          account_tracker_service.get());

#if BUILDFLAG(IS_IOS)
  init_params.device_accounts_synchronizer =
      std::make_unique<DeviceAccountsSynchronizerImpl>(
          token_service->GetDelegate());
#endif

  init_params.account_fetcher_service = std::move(account_fetcher_service);
  init_params.account_tracker_service = std::move(account_tracker_service);
  init_params.gaia_cookie_manager_service =
      std::move(gaia_cookie_manager_service);
  init_params.primary_account_manager = std::move(primary_account_manager);
  init_params.token_service = std::move(token_service);
  // TODO: Set the account_manager_facade on Lacros once Mirror is enabled by
  // default.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  init_params.account_manager_facade = account_manager_facade;
#endif
  init_params.signin_client = signin_client;

  return std::make_unique<IdentityManager>(std::move(init_params));
}

IdentityTestEnvironment::~IdentityTestEnvironment() {
  if (owned_identity_manager_)
    owned_identity_manager_->Shutdown();
}

IdentityManager* IdentityTestEnvironment::identity_manager() {
  return const_cast<IdentityManager*>(std::as_const(*this).identity_manager());
}

const IdentityManager* IdentityTestEnvironment::identity_manager() const {
  DCHECK(raw_identity_manager_ || owned_identity_manager_);
  DCHECK(!(raw_identity_manager_ && owned_identity_manager_));

  return raw_identity_manager_ ? raw_identity_manager_.get()
                               : owned_identity_manager_.get();
}

SigninClient* IdentityTestEnvironment::signin_client() {
  if (dependencies_owner_) {
    return dependencies_owner_->signin_client();
  } else {
    DCHECK(raw_signin_client_);
    return raw_signin_client_;
  }
}

TestIdentityManagerObserver*
IdentityTestEnvironment::identity_manager_observer() {
  return test_identity_manager_observer_.get();
}

void IdentityTestEnvironment::WaitForRefreshTokensLoaded() {
  signin::WaitForRefreshTokensLoaded(identity_manager());
}

CoreAccountInfo IdentityTestEnvironment::SetPrimaryAccount(
    const std::string& email,
    ConsentLevel consent_level) {
  return signin::SetPrimaryAccount(identity_manager(), email, consent_level);
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
    const std::string& email,
    ConsentLevel consent_level) {
  return signin::MakePrimaryAccountAvailable(identity_manager(), email,
                                             consent_level);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void IdentityTestEnvironment::RevokeSyncConsent() {
  signin::RevokeSyncConsent(identity_manager());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

void IdentityTestEnvironment::ClearPrimaryAccount() {
  signin::ClearPrimaryAccount(identity_manager());
}

AccountInfo IdentityTestEnvironment::MakeAccountAvailable(
    std::string_view email,
    SimpleAccountAvailabilityOptions options) {
  auto builder = CreateAccountAvailabilityOptionsBuilder();

  builder.WithCookie(options.set_cookie);

  if (!options.gaia_id.empty()) {
    builder.WithGaiaId(options.gaia_id);
  }
  if (options.primary_account_consent_level.has_value()) {
    builder.AsPrimary(options.primary_account_consent_level.value());
  }

  return MakeAccountAvailable(builder.Build(email));
}

AccountInfo IdentityTestEnvironment::MakeAccountAvailable(
    const AccountAvailabilityOptions& options) {
  return signin::MakeAccountAvailable(identity_manager(), options);
}

AccountAvailabilityOptionsBuilder
IdentityTestEnvironment::CreateAccountAvailabilityOptionsBuilder() {
  // NOTE: `test_url_loader_factory_` is passed directly here, but will be
  // CHECKed if we attempt to use a null value.
  return AccountAvailabilityOptionsBuilder(test_url_loader_factory_);
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
  signin::SetCookieAccounts(identity_manager(), test_url_loader_factory(),
                            cookie_accounts);
}

void IdentityTestEnvironment::TriggerListAccount() {
  signin::TriggerListAccount(identity_manager(), test_url_loader_factory());
}

void IdentityTestEnvironment::SetAutomaticIssueOfAccessTokens(bool grant) {
  fake_token_service()->set_auto_post_fetch_response_on_message_loop(grant);
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token) {
  WaitForAccessTokenRequestIfNecessary(std::nullopt);
  fake_token_service()->IssueTokenForAllPendingRequests(
      TokenResponseBuilder()
          .WithAccessToken(token)
          .WithExpirationTime(expiration)
          .WithIdToken(id_token)
          .build());
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        const CoreAccountId& account_id,
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token) {
  WaitForAccessTokenRequestIfNecessary(account_id);
  fake_token_service()->IssueAllTokensForAccount(
      account_id, TokenResponseBuilder()
                      .WithAccessToken(token)
                      .WithExpirationTime(expiration)
                      .WithIdToken(id_token)
                      .build());
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForScopes(
        const std::string& token,
        const base::Time& expiration,
        const std::string& id_token,
        const ScopeSet& scopes) {
  WaitForAccessTokenRequestIfNecessary(std::nullopt);
  fake_token_service()->IssueTokenForScope(scopes,
                                           TokenResponseBuilder()
                                               .WithAccessToken(token)
                                               .WithExpirationTime(expiration)
                                               .WithIdToken(id_token)
                                               .build());
}

void IdentityTestEnvironment::
    WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
        const GoogleServiceAuthError& error) {
  WaitForAccessTokenRequestIfNecessary(std::nullopt);
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
    const ScopeSet& scopes) {
  // Post a task to handle this access token request in order to support the
  // case where the access token request is handled synchronously in the
  // production code, in which case this callback could be coming in ahead
  // of an invocation of WaitForAccessTokenRequestIfNecessary() that will be
  // made in this same iteration of the run loop.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdentityTestEnvironment::HandleOnAccessTokenRequested,
                     weak_ptr_factory_.GetWeakPtr(), account_id));
}

void IdentityTestEnvironment::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  // Remove the Observers that IdentityTestEnvironment added during its
  // initialization.
  test_identity_manager_observer_.reset();
  diagnostics_observation_.Reset();
  identity_manager_observation_.Reset();
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
    std::optional<CoreAccountId> account_id) {
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

network::TestURLLoaderFactory*
IdentityTestEnvironment::test_url_loader_factory() {
  CHECK(test_url_loader_factory_)
      << "IdentityTestEnvironment cannot perform cookie-related operations if "
         "TestURLLoaderFactory isn't set. This may happen if you built your "
         "IdentityTestEnvironment from an existing profile. Please provide a "
         "TestURLLoaderFactory by calling SetTestURLLoaderFactory()";
  return test_url_loader_factory_;
}

void IdentityTestEnvironment::UpdateAccountInfoForAccount(
    AccountInfo account_info) {
  signin::UpdateAccountInfoForAccount(identity_manager(), account_info);
}

void IdentityTestEnvironment::ResetToAccountsNotYetLoadedFromDiskState() {
  fake_token_service()->set_all_credentials_loaded_for_testing(false);
}

void IdentityTestEnvironment::ReloadAccountsFromDisk() {
  fake_token_service()->LoadCredentials(CoreAccountId(), /*is_syncing=*/false);
}

bool IdentityTestEnvironment::IsAccessTokenRequestPending() {
  return fake_token_service()->GetPendingRequests().size();
}

std::vector<IdentityTestEnvironment::PendingRequest>
IdentityTestEnvironment::GetPendingAccessTokenRequests() {
  std::vector<PendingRequest> result;

  for (const auto& request : fake_token_service()->GetPendingRequests()) {
    result.emplace_back(request.account_id, request.client_id,
                        request.client_secret, request.scopes);
  }

  return result;
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

void IdentityTestEnvironment::SimulateGaiaLogOutFailure(
    const GoogleServiceAuthError& auth_error) {
  // GaiaCookieManagerService changes the visibility of inherited method
  // `OnLogOutFailure` from public to private. Cast to a base class
  // pointer to call the method.
  static_cast<GaiaAuthConsumer*>(
      identity_manager()->GetGaiaCookieManagerService())
      ->OnLogOutFailure(auth_error);
}

void IdentityTestEnvironment::SetTestURLLoaderFactory(
    network::TestURLLoaderFactory* test_url_loader_factory) {
  if (dependencies_owner_) {
    dependencies_owner_->signin_client()->OverrideTestUrlLoaderFactory(
        test_url_loader_factory);
  } else {
    DCHECK(raw_signin_client_);
    DCHECK_EQ(test_url_loader_factory->GetSafeWeakWrapper(),
              raw_signin_client_->GetURLLoaderFactory())
        << "SigninClient uses a factory different from the one passed in "
           "SetTestURLLoaderFactory(). See "
           "BuildChromeSigninClientWithURLLoader() method for setting up the "
           "SigninClient properly.";
  }
  test_url_loader_factory_ = test_url_loader_factory;
}

int IdentityTestEnvironment::
    GetNumCallsToPrepareForFetchingAccountCapabilities() {
  return static_cast<FakeAccountCapabilitiesFetcherFactory*>(
             identity_manager()
                 ->GetAccountFetcherService()
                 ->GetAccountCapabilitiesFetcherFactoryForTest())
      ->GetNumCallsToPrepareForFetchingAccountCapabilities();
}

}  // namespace signin
