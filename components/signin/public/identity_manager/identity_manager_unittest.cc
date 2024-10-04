// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/identity_manager.h"

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/image_fetcher/core/fake_image_decoder.h"
#include "components/signin/internal/identity_manager/account_fetcher_service.h"
#include "components/signin/internal/identity_manager/account_tracker_service.h"
#include "components/signin/internal/identity_manager/accounts_cookie_mutator_impl.h"
#include "components/signin/internal/identity_manager/accounts_mutator_impl.h"
#include "components/signin/internal/identity_manager/device_accounts_synchronizer_impl.h"
#include "components/signin/internal/identity_manager/diagnostics_provider_impl.h"
#include "components/signin/internal/identity_manager/fake_account_capabilities_fetcher_factory.h"
#include "components/signin/internal/identity_manager/fake_profile_oauth2_token_service.h"
#include "components/signin/internal/identity_manager/gaia_cookie_manager_service.h"
#include "components/signin/internal/identity_manager/primary_account_manager.h"
#include "components/signin/internal/identity_manager/primary_account_mutator_impl.h"
#include "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/accounts_cookie_mutator.h"
#include "components/signin/public/identity_manager/device_accounts_synchronizer.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/signin/public/identity_manager/set_accounts_in_cookie_result.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/cookie_change_dispatcher.h"
#include "net/cookies/cookie_constants.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_cookie_manager.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/signin/internal/identity_manager/child_account_info_fetcher_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_manager_facade_impl.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/account_manager_core/chromeos/account_manager_mojo_service.h"
#include "components/signin/internal/identity_manager/test_profile_oauth2_token_service_delegate_chromeos.h"
#endif

namespace signin {
namespace {

const char kTestConsumerId[] = "dummy_consumer";
const char kTestConsumerId2[] = "dummy_consumer 2";
const char kTestGaiaId[] = "dummyId";
const char kTestGaiaId2[] = "dummyId2";
const char kTestGaiaId3[] = "dummyId3";
const char kTestEmail[] = "me@gmail.com";
const char kTestEmail2[] = "me2@gmail.com";
const char kTestEmail3[] = "me3@gmail.com";

const char kTestHostedDomain[] = "example.com";
const char kTestFullName[] = "full_name";
const char kTestGivenName[] = "given_name";
const char kTestLocale[] = "locale";
const char kTestPictureUrl[] = "http://picture.example.com/picture.jpg";

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kTestEmailWithPeriod[] = "m.e@gmail.com";
#endif

// Subclass of FakeOAuth2AccessTokenManager with bespoke behavior.
class CustomFakeOAuth2AccessTokenManager : public FakeOAuth2AccessTokenManager {
 public:
  CustomFakeOAuth2AccessTokenManager(
      OAuth2AccessTokenManager::Delegate* delegate)
      : FakeOAuth2AccessTokenManager(delegate) {}

  void set_on_access_token_invalidated_info(
      CoreAccountId expected_account_id_to_invalidate,
      std::set<std::string> expected_scopes_to_invalidate,
      std::string expected_access_token_to_invalidate,
      base::OnceClosure callback) {
    expected_account_id_to_invalidate_ = expected_account_id_to_invalidate;
    expected_scopes_to_invalidate_ = expected_scopes_to_invalidate;
    expected_access_token_to_invalidate_ = expected_access_token_to_invalidate;
    on_access_token_invalidated_callback_ = std::move(callback);
  }

 private:
  friend class CustomFakeProfileOAuth2TokenService;
  // OAuth2AccessTokenManager:
  void InvalidateAccessTokenImpl(const CoreAccountId& account_id,
                                 const std::string& client_id,
                                 const ScopeSet& scopes,
                                 const std::string& access_token) override {
    if (on_access_token_invalidated_callback_) {
      EXPECT_EQ(expected_account_id_to_invalidate_, account_id);
      EXPECT_EQ(expected_scopes_to_invalidate_, scopes);
      EXPECT_EQ(expected_access_token_to_invalidate_, access_token);

      // It should trigger OnAccessTokenRemovedFromCache from
      // IdentityManager::DiagnosticsObserver.
      for (auto& observer : GetDiagnosticsObserversForTesting())
        observer.OnAccessTokenRemoved(account_id, scopes);

      std::move(on_access_token_invalidated_callback_).Run();
    }
  }

  CoreAccountId expected_account_id_to_invalidate_;
  std::set<std::string> expected_scopes_to_invalidate_;
  std::string expected_access_token_to_invalidate_;
  base::OnceClosure on_access_token_invalidated_callback_;
};

// Subclass of FakeProfileOAuth2TokenService with bespoke behavior.
class CustomFakeProfileOAuth2TokenService
    : public FakeProfileOAuth2TokenService {
 public:
  explicit CustomFakeProfileOAuth2TokenService(PrefService* user_prefs)
      : FakeProfileOAuth2TokenService(user_prefs) {
    OverrideAccessTokenManagerForTesting(
        std::make_unique<CustomFakeOAuth2AccessTokenManager>(
            this /* OAuth2AccessTokenManager::Delegate* */));
  }

  CustomFakeProfileOAuth2TokenService(
      PrefService* user_prefs,
      std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate)
      : FakeProfileOAuth2TokenService(user_prefs, std::move(delegate)) {
    OverrideAccessTokenManagerForTesting(
        std::make_unique<CustomFakeOAuth2AccessTokenManager>(
            this /* OAuth2AccessTokenManager::Delegate* */));
  }

  void set_on_access_token_invalidated_info(
      CoreAccountId expected_account_id_to_invalidate,
      std::set<std::string> expected_scopes_to_invalidate,
      std::string expected_access_token_to_invalidate,
      base::OnceClosure callback) {
    GetCustomAccessTokenManager()->set_on_access_token_invalidated_info(
        expected_account_id_to_invalidate, expected_scopes_to_invalidate,
        expected_access_token_to_invalidate, std::move(callback));
  }

 private:
  CustomFakeOAuth2AccessTokenManager* GetCustomAccessTokenManager() {
    return static_cast<CustomFakeOAuth2AccessTokenManager*>(
        GetAccessTokenManager());
  }
};

class TestIdentityManagerDiagnosticsObserver
    : IdentityManager::DiagnosticsObserver {
 public:
  explicit TestIdentityManagerDiagnosticsObserver(
      IdentityManager* identity_manager)
      : identity_manager_(identity_manager) {
    identity_manager_->AddDiagnosticsObserver(this);
  }
  ~TestIdentityManagerDiagnosticsObserver() override {
    identity_manager_->RemoveDiagnosticsObserver(this);
  }

  void set_on_access_token_requested_callback(base::OnceClosure callback) {
    on_access_token_requested_callback_ = std::move(callback);
  }

  void set_on_access_token_request_completed_callback(
      base::OnceClosure callback) {
    on_access_token_request_completed_callback_ = std::move(callback);
  }

  const CoreAccountId& token_requestor_account_id() {
    return token_requestor_account_id_;
  }
  const std::string& token_requestor_consumer_id() {
    return token_requestor_consumer_id_;
  }
  const ScopeSet& token_requestor_scopes() { return token_requestor_scopes_; }
  const CoreAccountId& token_remover_account_id() {
    return token_remover_account_id_;
  }
  const ScopeSet& token_remover_scopes() { return token_remover_scopes_; }
  const CoreAccountId& on_access_token_request_completed_account_id() {
    return access_token_request_completed_account_id_;
  }
  const std::string& on_access_token_request_completed_consumer_id() {
    return access_token_request_completed_consumer_id_;
  }
  const ScopeSet& on_access_token_request_completed_scopes() {
    return access_token_request_completed_scopes_;
  }
  const GoogleServiceAuthError& on_access_token_request_completed_error() {
    return access_token_request_completed_error_;
  }

 private:
  // IdentityManager::DiagnosticsObserver:
  void OnAccessTokenRequested(const CoreAccountId& account_id,
                              const std::string& consumer_id,
                              const ScopeSet& scopes) override {
    token_requestor_account_id_ = account_id;
    token_requestor_consumer_id_ = consumer_id;
    token_requestor_scopes_ = scopes;

    if (on_access_token_requested_callback_)
      std::move(on_access_token_requested_callback_).Run();
  }

  void OnAccessTokenRemovedFromCache(const CoreAccountId& account_id,
                                     const ScopeSet& scopes) override {
    token_remover_account_id_ = account_id;
    token_remover_scopes_ = scopes;
  }

  void OnAccessTokenRequestCompleted(const CoreAccountId& account_id,
                                     const std::string& consumer_id,
                                     const ScopeSet& scopes,
                                     const GoogleServiceAuthError& error,
                                     base::Time expiration_time) override {
    access_token_request_completed_account_id_ = account_id;
    access_token_request_completed_consumer_id_ = consumer_id;
    access_token_request_completed_scopes_ = scopes;
    access_token_request_completed_error_ = error;

    if (on_access_token_request_completed_callback_)
      std::move(on_access_token_request_completed_callback_).Run();
  }

  raw_ptr<IdentityManager> identity_manager_;
  base::OnceClosure on_access_token_requested_callback_;
  base::OnceClosure on_access_token_request_completed_callback_;
  CoreAccountId token_requestor_account_id_;
  std::string token_requestor_consumer_id_;
  CoreAccountId token_remover_account_id_;
  ScopeSet token_requestor_scopes_;
  ScopeSet token_remover_scopes_;
  CoreAccountId access_token_request_completed_account_id_;
  std::string access_token_request_completed_consumer_id_;
  ScopeSet access_token_request_completed_scopes_;
  GoogleServiceAuthError access_token_request_completed_error_;
};

}  // namespace

class IdentityManagerTest : public testing::Test {
 public:
  IdentityManagerTest(const IdentityManagerTest&) = delete;
  IdentityManagerTest& operator=(const IdentityManagerTest&) = delete;

 protected:
  IdentityManagerTest()
      : signin_client_(&pref_service_, &test_url_loader_factory_) {
    IdentityManager::RegisterProfilePrefs(pref_service_.registry());
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());

    IdentityManager::RegisterLocalStatePrefs(pref_service_.registry());

    RecreateIdentityManager(
        AccountConsistencyMethod::kDisabled,
        PrimaryAccountManagerSetup::kWithAuthenticatedAccout);
    primary_account_id_ =
        identity_manager_->PickAccountIdForAccount(kTestGaiaId, kTestEmail);
  }

  ~IdentityManagerTest() override {
    identity_manager_->Shutdown();
    signin_client_.Shutdown();
  }

  IdentityManager* identity_manager() { return identity_manager_.get(); }

  TestIdentityManagerObserver* identity_manager_observer() {
    return identity_manager_observer_.get();
  }

  TestIdentityManagerDiagnosticsObserver*
  identity_manager_diagnostics_observer() {
    return identity_manager_diagnostics_observer_.get();
  }

  AccountTrackerService* account_tracker() {
    return identity_manager()->GetAccountTrackerService();
  }

  CustomFakeProfileOAuth2TokenService* token_service() {
    return static_cast<CustomFakeProfileOAuth2TokenService*>(
        identity_manager()->GetTokenService());
  }

  // See RecreateIdentityManager.
  enum class PrimaryAccountManagerSetup {
    kWithAuthenticatedAccout,
    kNoAuthenticatedAccount
  };

  // Used by some tests that need to re-instantiate IdentityManager after
  // performing some other setup.
  void RecreateIdentityManager() {
    RecreateIdentityManager(
        AccountConsistencyMethod::kDisabled,
        PrimaryAccountManagerSetup::kNoAuthenticatedAccount);
  }

  // Recreates IdentityManager with given |account_consistency| and optionally
  // seeds with an authenticated account depending on
  // |primary_account_manager_setup|. This process destroys any existing
  // IdentityManager and its dependencies, then remakes them. Dependencies that
  // outlive PrimaryAccountManager (e.g. SigninClient) will be reused.
  void RecreateIdentityManager(
      AccountConsistencyMethod account_consistency,
      PrimaryAccountManagerSetup primary_account_manager_setup) {
    // Remove observers first, otherwise IdentityManager destruction might
    // trigger a DCHECK because there are still living observers.
    identity_manager_observer_.reset();
    identity_manager_diagnostics_observer_.reset();
    if (identity_manager_)
      identity_manager_->Shutdown();
    identity_manager_.reset();

    if (temp_profile_dir_.IsValid()) {
      // We are actually re-creating everything. Delete the previously used
      // directory.
      ASSERT_TRUE(temp_profile_dir_.Delete());
    }

    ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());

#if BUILDFLAG(IS_ANDROID)
    // Required to create AccountTrackerService on Android.
    SetUpMockAccountManagerFacade();
#endif
    auto account_tracker_service = std::make_unique<AccountTrackerService>();
    account_tracker_service->Initialize(&pref_service_,
                                        temp_profile_dir_.GetPath());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    account_manager::AccountManager::RegisterPrefs(pref_service_.registry());
    auto* ash_account_manager = GetAccountManagerFactory()->GetAccountManager(
        temp_profile_dir_.GetPath().value());
    ash_account_manager->InitializeInEphemeralMode(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));
    ash_account_manager->SetPrefService(&pref_service_);

    auto* ash_account_manager_mojo_service =
        GetAccountManagerFactory()->GetAccountManagerMojoService(
            temp_profile_dir_.GetPath().value());

    mojo::Remote<crosapi::mojom::AccountManager> remote;
    ash_account_manager_mojo_service->BindReceiver(
        remote.BindNewPipeAndPassReceiver());
    account_manager_facade_ =
        std::make_unique<account_manager::AccountManagerFacadeImpl>(
            std::move(remote),
            /*remote_version=*/std::numeric_limits<uint32_t>::max(),
            /*account_manager_for_tests=*/
            ash_account_manager->GetWeakPtr());

    auto token_service = std::make_unique<CustomFakeProfileOAuth2TokenService>(
        &pref_service_,
        std::make_unique<TestProfileOAuth2TokenServiceDelegateChromeOS>(
            &signin_client_, account_tracker_service.get(),
            ash_account_manager_mojo_service,
            /*is_regular_profile=*/true));
#else
    auto token_service =
        std::make_unique<CustomFakeProfileOAuth2TokenService>(&pref_service_);
#endif

    auto gaia_cookie_manager_service =
        std::make_unique<GaiaCookieManagerService>(
            account_tracker_service.get(), token_service.get(),
            &signin_client_);

    auto account_fetcher_service = std::make_unique<AccountFetcherService>();
    account_fetcher_service->Initialize(
        &signin_client_, token_service.get(), account_tracker_service.get(),
        std::make_unique<image_fetcher::FakeImageDecoder>(),
        std::make_unique<FakeAccountCapabilitiesFetcherFactory>());

    DCHECK_EQ(account_consistency, AccountConsistencyMethod::kDisabled)
        << "AccountConsistency is not used by PrimaryAccountManager";
    auto primary_account_manager = std::make_unique<PrimaryAccountManager>(
        &signin_client_, token_service.get(), account_tracker_service.get());

    // Passing this switch ensures that the new PrimaryAccountManager starts
    // with a clean slate. Otherwise PrimaryAccountManager::Initialize will use
    // the account id stored in prefs::kGoogleServicesAccountId.
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitch(switches::kClearTokenService);

    if (primary_account_manager_setup ==
        PrimaryAccountManagerSetup::kWithAuthenticatedAccout) {
      CoreAccountId account_id =
          account_tracker_service->SeedAccountInfo(kTestGaiaId, kTestEmail);
      primary_account_manager->SetPrimaryAccountInfo(
          account_tracker_service->GetAccountInfo(account_id),
          ConsentLevel::kSync,
          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
    }

    IdentityManager::InitParameters init_params;

    init_params.accounts_cookie_mutator =
        std::make_unique<AccountsCookieMutatorImpl>(
            &signin_client_, token_service.get(),
            gaia_cookie_manager_service.get(), account_tracker_service.get());

    init_params.diagnostics_provider =
        std::make_unique<DiagnosticsProviderImpl>(
            token_service.get(), gaia_cookie_manager_service.get());

    init_params.primary_account_mutator =
        std::make_unique<PrimaryAccountMutatorImpl>(
            account_tracker_service.get(), primary_account_manager.get(),
            &pref_service_, &signin_client_);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    init_params.device_accounts_synchronizer =
        std::make_unique<DeviceAccountsSynchronizerImpl>(
            token_service->GetDelegate());
#else
    init_params.accounts_mutator = std::make_unique<AccountsMutatorImpl>(
        token_service.get(), account_tracker_service.get(),
        primary_account_manager.get(), &pref_service_);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    init_params.account_manager_facade = account_manager_facade_.get();
#endif
    init_params.signin_client = &signin_client_;
    init_params.account_fetcher_service = std::move(account_fetcher_service);
    init_params.account_tracker_service = std::move(account_tracker_service);
    init_params.gaia_cookie_manager_service =
        std::move(gaia_cookie_manager_service);
    init_params.primary_account_manager = std::move(primary_account_manager);
    init_params.token_service = std::move(token_service);

    identity_manager_ =
        std::make_unique<IdentityManager>(std::move(init_params));
    identity_manager_observer_ =
        std::make_unique<TestIdentityManagerObserver>(identity_manager_.get());
    identity_manager_diagnostics_observer_ =
        std::make_unique<TestIdentityManagerDiagnosticsObserver>(
            identity_manager_.get());

    // CustomFakeProfileOAuth2TokenService loads credentials immediately, so
    // several tests (for example, `AreRefreshTokensLoaded`) expect this
    // behavior from all platforms, even from ones where tokens are loaded
    // asynchronously (e.g., ChromeOS). Wait for loading to finish to meet these
    // expectations.
    // TODO(crbug.com/40175926): Move waiting to tests that need it.
    signin::WaitForRefreshTokensLoaded(identity_manager());
  }

  void SimulateCookieDeletedByUser(
      network::mojom::CookieChangeListener* listener,
      const net::CanonicalCookie& cookie) {
    listener->OnCookieChange(net::CookieChangeInfo(
        cookie, net::CookieAccessResult(), net::CookieChangeCause::EXPLICIT));
  }

  void SimulateOAuthMultiloginFinished(GaiaCookieManagerService* manager,
                                       SetAccountsInCookieResult error) {
    manager->OnSetAccountsFinished(error);
  }

  const CoreAccountId& primary_account_id() const {
    return primary_account_id_;
  }

  TestSigninClient* signin_client() { return &signin_client_; }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AccountManagerFactory* GetAccountManagerFactory() {
    return &account_manager_factory_;
  }
#endif

 private:
  base::ScopedTempDir temp_profile_dir_;
  base::test::TaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::AccountManagerFactory account_manager_factory_;
  std::unique_ptr<account_manager::AccountManagerFacadeImpl>
      account_manager_facade_;
#endif
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestSigninClient signin_client_;
  std::unique_ptr<IdentityManager> identity_manager_;
  std::unique_ptr<TestIdentityManagerObserver> identity_manager_observer_;
  std::unique_ptr<TestIdentityManagerDiagnosticsObserver>
      identity_manager_diagnostics_observer_;
  CoreAccountId primary_account_id_;
};

// Test that IdentityManager's constructor properly sets all passed parameters.
TEST_F(IdentityManagerTest, Construct) {
  EXPECT_NE(identity_manager()->GetAccountTrackerService(), nullptr);
  EXPECT_NE(identity_manager()->GetTokenService(), nullptr);
  EXPECT_NE(identity_manager()->GetGaiaCookieManagerService(), nullptr);
  EXPECT_NE(identity_manager()->GetPrimaryAccountManager(), nullptr);
  EXPECT_NE(identity_manager()->GetAccountFetcherService(), nullptr);
  EXPECT_NE(identity_manager()->GetPrimaryAccountMutator(), nullptr);
  EXPECT_NE(identity_manager()->GetAccountsCookieMutator(), nullptr);
  EXPECT_NE(identity_manager()->GetDiagnosticsProvider(), nullptr);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  EXPECT_EQ(identity_manager()->GetAccountsMutator(), nullptr);
  EXPECT_NE(identity_manager()->GetDeviceAccountsSynchronizer(), nullptr);
#else
  EXPECT_NE(identity_manager()->GetAccountsMutator(), nullptr);
  EXPECT_EQ(identity_manager()->GetDeviceAccountsSynchronizer(), nullptr);
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_NE(identity_manager()->GetAccountManagerFacade(), nullptr);
#endif
}

// Test that IdentityManager starts off with the information in
// PrimaryAccountManager.
TEST_F(IdentityManagerTest, PrimaryAccountInfoAtStartup) {
  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_info.email);

  // Primary account is by definition also unconsented primary account.
  EXPECT_EQ(primary_account_info,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));
  // There is no guarantee that this will be notified via callback on startup.
}

// Signin/signout tests aren't relevant and cannot build on ChromeOS, which
// doesn't support signin/signout.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Test that the user signing in results in firing of the IdentityManager
// observer callback and the IdentityManager's state being updated.
TEST_F(IdentityManagerTest, PrimaryAccountInfoAfterSignin) {
  ClearPrimaryAccount(identity_manager());

  SetPrimaryAccount(identity_manager(), kTestEmail, ConsentLevel::kSync);
  auto event = identity_manager_observer()->GetPrimaryAccountChangedEvent();
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
            event.GetEventTypeFor(ConsentLevel::kSync));
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kSet,
            event.GetEventTypeFor(ConsentLevel::kSignin));

  CoreAccountInfo primary_account_from_set_callback =
      event.GetCurrentState().primary_account;
  EXPECT_EQ(kTestGaiaId, primary_account_from_set_callback.gaia);
  EXPECT_EQ(kTestEmail, primary_account_from_set_callback.email);

  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_info.email);

  // Primary account is by definition also unconsented primary account.
  EXPECT_EQ(primary_account_info,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));

  CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  EXPECT_EQ(primary_account_id, CoreAccountId::FromGaiaId(kTestGaiaId));
  EXPECT_EQ(primary_account_id, primary_account_info.account_id);

  EXPECT_EQ(primary_account_id, identity_manager()->GetPrimaryAccountId(
                                    signin::ConsentLevel::kSignin));
}

// Test that the user signing out results in firing of the IdentityManager
// observer callback and the IdentityManager's state being updated.
TEST_F(IdentityManagerTest, PrimaryAccountInfoAfterSigninAndSignout) {
  ClearPrimaryAccount(identity_manager());
  // First ensure that the user is signed in from the POV of the
  // IdentityManager.
  SetPrimaryAccount(identity_manager(), kTestEmail, ConsentLevel::kSync);

  // Sign the user out and check that the IdentityManager responds
  // appropriately.
  ClearPrimaryAccount(identity_manager());
  auto event = identity_manager_observer()->GetPrimaryAccountChangedEvent();
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
            event.GetEventTypeFor(ConsentLevel::kSync));
  EXPECT_EQ(PrimaryAccountChangeEvent::Type::kCleared,
            event.GetEventTypeFor(ConsentLevel::kSignin));

  CoreAccountInfo primary_account_from_cleared_callback =
      event.GetPreviousState().primary_account;
  EXPECT_EQ(kTestGaiaId, primary_account_from_cleared_callback.gaia);
  EXPECT_EQ(kTestEmail, primary_account_from_cleared_callback.email);

  // After the sign-out, there is no unconsented primary account.
  EXPECT_TRUE(identity_manager()
                  ->GetPrimaryAccountInfo(ConsentLevel::kSignin)
                  .IsEmpty());

  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_EQ("", primary_account_info.gaia);
  EXPECT_EQ("", primary_account_info.email);
  EXPECT_EQ(primary_account_info,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));

  CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  EXPECT_TRUE(primary_account_id.empty());
  EXPECT_EQ(primary_account_id, primary_account_info.account_id);
  EXPECT_EQ(primary_account_id, identity_manager()->GetPrimaryAccountId(
                                    signin::ConsentLevel::kSignin));
}

// Test that the primary account's core info remains tracked by the
// IdentityManager after signing in even after having removed the refresh token
// without signing out.
TEST_F(IdentityManagerTest,
       PrimaryAccountInfoAfterSigninAndRefreshTokenRemoval) {
  ClearPrimaryAccount(identity_manager());
  // First ensure that the user is signed in from the POV of the
  // IdentityManager.
  SetPrimaryAccount(identity_manager(), kTestEmail, ConsentLevel::kSync);

  identity_manager()->account_fetcher_service_->EnableAccountRemovalForTest();
  // Revoke the primary's account credentials from the token service and
  // check that the returned CoreAccountInfo is still valid since the
  // identity_manager stores it.
  token_service()->RevokeCredentials(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync));

  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_info.email);
  EXPECT_EQ(CoreAccountId::FromGaiaId(kTestGaiaId),
            primary_account_info.account_id);
  EXPECT_EQ(primary_account_info,
            identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin));

  CoreAccountId primary_account_id =
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync);
  EXPECT_EQ(primary_account_id, CoreAccountId::FromGaiaId(kTestGaiaId));
  EXPECT_EQ(primary_account_id,
            identity_manager()->GetPrimaryAccountId(ConsentLevel::kSignin));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(IdentityManagerTest, HasPrimaryAccount) {
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

  // Removing the account from the AccountTrackerService should not cause
  // IdentityManager to think that there is no longer a primary account.
  account_tracker()->RemoveAccount(
      identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync));
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Signing out should cause IdentityManager to recognize that there is no
  // longer a primary account.
  ClearPrimaryAccount(identity_manager());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  EXPECT_FALSE(identity_manager_observer()
                   ->GetPrimaryAccountChangedEvent()
                   .GetPreviousState()
                   .primary_account.IsEmpty());
#endif
}

TEST_F(IdentityManagerTest, GetAccountsInteractionWithPrimaryAccount) {
  // Should not have any refresh tokens at initialization.
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  // Add a refresh token for the primary account and check that it shows up in
  // GetAccountsWithRefreshTokens().
  SetRefreshTokenForPrimaryAccount(identity_manager());

  std::vector<CoreAccountInfo> accounts_after_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, primary_account_id());
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail);

  // Update the token and check that it doesn't change the state (or blow up).
  SetRefreshTokenForPrimaryAccount(identity_manager());

  std::vector<CoreAccountInfo> accounts_after_second_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_second_update.size());
  EXPECT_EQ(accounts_after_second_update[0].account_id, primary_account_id());
  EXPECT_EQ(accounts_after_second_update[0].gaia, kTestGaiaId);
  EXPECT_EQ(accounts_after_second_update[0].email, kTestEmail);

  // Remove the token for the primary account and check that this is likewise
  // reflected.
  RemoveRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
}

TEST_F(IdentityManagerTest,
       QueryingOfRefreshTokensInteractionWithPrimaryAccount) {
  CoreAccountInfo account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);

  // Should not have a refresh token for the primary account at initialization.
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Add a refresh token for the primary account and check that it affects this
  // state.
  SetRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Update the token and check that it doesn't change the state (or blow up).
  SetRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Remove the token for the primary account and check that this is likewise
  // reflected.
  RemoveRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
}

TEST_F(IdentityManagerTest, QueryingOfRefreshTokensReflectsEmptyInitialState) {
  CoreAccountInfo account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  CoreAccountId account_id = account_info.account_id;

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  SetRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
}

TEST_F(IdentityManagerTest, GetAccountsInteractionWithSecondaryAccounts) {
  // Should not have any refresh tokens at initialization.
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());

  // Add a refresh token for a secondary account and check that it shows up in
  // GetAccountsWithRefreshTokens().
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  CoreAccountId account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id2);

  std::vector<CoreAccountInfo> accounts_after_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id2);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId2);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail2);

  // Add a refresh token for a different secondary account and check that it
  // also shows up in GetAccountsWithRefreshTokens().
  account_tracker()->SeedAccountInfo(kTestGaiaId3, kTestEmail3);
  CoreAccountId account_id3 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId3).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id3);

  std::vector<CoreAccountInfo> accounts_after_second_update =
      identity_manager()->GetAccountsWithRefreshTokens();
  EXPECT_EQ(2u, accounts_after_second_update.size());

  for (CoreAccountInfo account_info : accounts_after_second_update) {
    if (account_info.account_id == account_id2) {
      EXPECT_EQ(account_info.gaia, kTestGaiaId2);
      EXPECT_EQ(account_info.email, kTestEmail2);
    } else {
      EXPECT_EQ(account_info.gaia, kTestGaiaId3);
      EXPECT_EQ(account_info.email, kTestEmail3);
    }
  }

  // Remove the token for account2 and check that account3 is still present.
  RemoveRefreshTokenForAccount(identity_manager(), account_id2);

  std::vector<CoreAccountInfo> accounts_after_third_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_third_update.size());
  EXPECT_EQ(accounts_after_third_update[0].account_id, account_id3);
  EXPECT_EQ(accounts_after_third_update[0].gaia, kTestGaiaId3);
  EXPECT_EQ(accounts_after_third_update[0].email, kTestEmail3);
}

TEST_F(IdentityManagerTest,
       HasPrimaryAccountWithRefreshTokenInteractionWithSecondaryAccounts) {
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Adding a refresh token for a secondary account shouldn't change anything
  // about the primary account
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  CoreAccountId account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Adding a refresh token for a different secondary account should not do so
  // either.
  account_tracker()->SeedAccountInfo(kTestGaiaId3, kTestEmail3);
  CoreAccountId account_id3 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId3).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id3);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Removing the token for account2 should have no effect.
  RemoveRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
}

TEST_F(IdentityManagerTest,
       HasAccountWithRefreshTokenInteractionWithSecondaryAccounts) {
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  AccountInfo account_info2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2);
  CoreAccountId account_id2 = account_info2.account_id;

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Add a refresh token for account_info2 and check that this is reflected by
  // HasAccountWithRefreshToken(.account_id).
  SetRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Go through the same process for a different secondary account.
  account_tracker()->SeedAccountInfo(kTestGaiaId3, kTestEmail3);
  AccountInfo account_info3 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId3);
  CoreAccountId account_id3 = account_info3.account_id;

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info3.account_id));

  SetRefreshTokenForAccount(identity_manager(), account_id3);

  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info3.account_id));

  // Remove the token for account2.
  RemoveRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info3.account_id));
}

TEST_F(IdentityManagerTest,
       GetAccountsInteractionBetweenPrimaryAndSecondaryAccounts) {
  // Should not have any refresh tokens at initialization.
  EXPECT_TRUE(identity_manager()->GetAccountsWithRefreshTokens().empty());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // Add a refresh token for a secondary account and check that it shows up in
  // GetAccountsWithRefreshTokens().
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  CoreAccountId account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id2);

  std::vector<CoreAccountInfo> accounts_after_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id2);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId2);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));

  // The user still has a primary account.
  EXPECT_EQ(identity_manager()
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                .email,
            kTestEmail);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin).email,
      kTestEmail);

  // Add a refresh token for the primary account and check that it
  // also shows up in GetAccountsWithRefreshTokens().
  SetRefreshTokenForPrimaryAccount(identity_manager());

  std::vector<CoreAccountInfo> accounts_after_second_update =
      identity_manager()->GetAccountsWithRefreshTokens();
  EXPECT_EQ(2u, accounts_after_second_update.size());

  for (const CoreAccountInfo& account_info : accounts_after_second_update) {
    if (account_info.account_id == account_id2) {
      EXPECT_EQ(account_info.gaia, kTestGaiaId2);
      EXPECT_EQ(account_info.email, kTestEmail2);
    } else {
      EXPECT_EQ(account_info.gaia, kTestGaiaId);
      EXPECT_EQ(account_info.email, kTestEmail);
    }
  }

  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  EXPECT_EQ(identity_manager()
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                .email,
            kTestEmail);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin).email,
      kTestEmail);

  // Remove the token for the primary account and check that account2 is still
  // present.
  RemoveRefreshTokenForPrimaryAccount(identity_manager());

  std::vector<CoreAccountInfo> accounts_after_third_update =
      identity_manager()->GetAccountsWithRefreshTokens();

  EXPECT_EQ(1u, accounts_after_third_update.size());
  EXPECT_EQ(accounts_after_update[0].account_id, account_id2);
  EXPECT_EQ(accounts_after_update[0].gaia, kTestGaiaId2);
  EXPECT_EQ(accounts_after_update[0].email, kTestEmail2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  // The user still has a primary account.
  EXPECT_EQ(identity_manager()
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                .email,
            kTestEmail);
  EXPECT_EQ(
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin).email,
      kTestEmail);
}

TEST_F(
    IdentityManagerTest,
    HasPrimaryAccountWithRefreshTokenInteractionBetweenPrimaryAndSecondaryAccounts) {
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

  // Add a refresh token for a secondary account and check that it doesn't
  // impact the above state.
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  CoreAccountId account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

  // Add a refresh token for the primary account and check that it
  // *does* impact the stsate of
  // HasPrimaryAccountWithRefreshToken(signin::ConsentLevel::kSync).
  SetRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

  // Remove the token for the secondary account and check that this doesn't flip
  // the state.
  RemoveRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_TRUE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));

  // Remove the token for the primary account and check that this flips the
  // state.
  RemoveRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
}

TEST_F(
    IdentityManagerTest,
    HasAccountWithRefreshTokenInteractionBetweenPrimaryAndSecondaryAccounts) {
  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  CoreAccountId primary_account_id = primary_account_info.account_id;

  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  AccountInfo account_info2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2);
  CoreAccountId account_id2 = account_info2.account_id;

  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Add a refresh token for account_info2 and check that this is reflected by
  // HasAccountWithRefreshToken(.account_id).
  SetRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Go through the same process for the primary account.
  SetRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));

  // Remove the token for account2.
  RemoveRefreshTokenForAccount(identity_manager(), account_id2);

  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshToken(account_info2.account_id));
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToErrorStateOfRefreshTokenForAccount) {
  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  CoreAccountId primary_account_id = primary_account_info.account_id;
  SetRefreshTokenForPrimaryAccount(identity_manager());

  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  AccountInfo account_info2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2);
  CoreAccountId account_id2 = account_info2.account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id2);

  GoogleServiceAuthError user_not_signed_up_error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::USER_NOT_SIGNED_UP);
  GoogleServiceAuthError invalid_gaia_credentials_error =
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
  GoogleServiceAuthError transient_error = GoogleServiceAuthError(
      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);

  // Set a persistent error for |account_id2| and check that it's reflected.
  token_service()->UpdateAuthErrorForTesting(account_id2,
                                             user_not_signed_up_error);
  EXPECT_EQ(account_id2,
            identity_manager_observer()
                ->AccountFromErrorStateOfRefreshTokenUpdatedCallback()
                .account_id);
  EXPECT_EQ(user_not_signed_up_error,
            identity_manager_observer()
                ->ErrorFromErrorStateOfRefreshTokenUpdatedCallback());

  // A transient error should not cause a callback.
  token_service()->UpdateAuthErrorForTesting(primary_account_id,
                                             transient_error);
  EXPECT_EQ(account_id2,
            identity_manager_observer()
                ->AccountFromErrorStateOfRefreshTokenUpdatedCallback()
                .account_id);
  EXPECT_EQ(user_not_signed_up_error,
            identity_manager_observer()
                ->ErrorFromErrorStateOfRefreshTokenUpdatedCallback());

  // Set a different persistent error for the primary account and check that
  // it's reflected.
  token_service()->UpdateAuthErrorForTesting(primary_account_id,
                                             invalid_gaia_credentials_error);
  EXPECT_EQ(primary_account_id,
            identity_manager_observer()
                ->AccountFromErrorStateOfRefreshTokenUpdatedCallback()
                .account_id);
  EXPECT_EQ(invalid_gaia_credentials_error,
            identity_manager_observer()
                ->ErrorFromErrorStateOfRefreshTokenUpdatedCallback());
}

TEST_F(IdentityManagerTest, GetErrorStateOfRefreshTokenForAccount) {
  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  CoreAccountId primary_account_id = primary_account_info.account_id;

  // A primary account without a refresh token should not be in an error
  // state, and setting a refresh token should not affect that.
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  SetRefreshTokenForPrimaryAccount(identity_manager());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // A secondary account without a refresh token should not be in an error
  // state, and setting a refresh token should not affect that.
  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  AccountInfo account_info2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2);
  CoreAccountId account_id2 = account_info2.account_id;
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));

  SetRefreshTokenForAccount(identity_manager(), account_id2);
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));

  GoogleServiceAuthError user_not_signed_up_error =
      GoogleServiceAuthError(GoogleServiceAuthError::State::USER_NOT_SIGNED_UP);
  GoogleServiceAuthError invalid_gaia_credentials_error =
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
  GoogleServiceAuthError transient_error = GoogleServiceAuthError(
      GoogleServiceAuthError::State::SERVICE_UNAVAILABLE);

  // Set a persistent error for |account_id2| and check that it's reflected.
  token_service()->UpdateAuthErrorForTesting(account_id2,
                                             user_not_signed_up_error);
  EXPECT_EQ(
      user_not_signed_up_error,
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // A transient error should cause no change in the error state.
  token_service()->UpdateAuthErrorForTesting(primary_account_id,
                                             transient_error);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(),
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // Set a different persistent error for the primary account and check that
  // it's reflected.
  token_service()->UpdateAuthErrorForTesting(primary_account_id,
                                             invalid_gaia_credentials_error);
  EXPECT_EQ(
      user_not_signed_up_error,
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));
  EXPECT_EQ(invalid_gaia_credentials_error,
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));

  // Remove the token for account2 and check that it goes back to having no
  // error.
  RemoveRefreshTokenForAccount(identity_manager(), account_id2);
  EXPECT_EQ(
      GoogleServiceAuthError::AuthErrorNone(),
      identity_manager()->GetErrorStateOfRefreshTokenForAccount(account_id2));
  EXPECT_FALSE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          account_id2));
  EXPECT_EQ(invalid_gaia_credentials_error,
            identity_manager()->GetErrorStateOfRefreshTokenForAccount(
                primary_account_id));
  EXPECT_TRUE(
      identity_manager()->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_id));
}

TEST_F(IdentityManagerTest, RemoveAccessTokenFromCache) {
  std::set<std::string> scopes{"scope"};
  std::string access_token = "access_token";

  identity_manager()->GetAccountTrackerService()->SeedAccountInfo(kTestGaiaId,
                                                                  kTestEmail);
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      primary_account_id(), ConsentLevel::kSync,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  SetRefreshTokenForAccount(identity_manager(), primary_account_id(),
                            "refresh_token");

  base::RunLoop run_loop;
  token_service()->set_on_access_token_invalidated_info(
      primary_account_id(), scopes, access_token, run_loop.QuitClosure());

  identity_manager()->RemoveAccessTokenFromCache(primary_account_id(), scopes,
                                                 access_token);

  run_loop.Run();

  // RemoveAccessTokenFromCache should lead to OnAccessTokenRemovedFromCache
  // from IdentityManager::DiagnosticsObserver.
  EXPECT_EQ(
      primary_account_id(),
      identity_manager_diagnostics_observer()->token_remover_account_id());
  EXPECT_EQ(scopes,
            identity_manager_diagnostics_observer()->token_remover_scopes());
}

TEST_F(IdentityManagerTest, CreateAccessTokenFetcher) {
  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync),
          kTestConsumerId, scopes, std::move(callback),
          AccessTokenFetcher::Mode::kImmediate);
  EXPECT_TRUE(token_fetcher);
}

TEST_F(IdentityManagerTest,
       CreateAccessTokenFetcherWithCustomURLLoaderFactory) {
  base::RunLoop run_loop;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_requested_callback(run_loop.QuitClosure());

  identity_manager()->GetAccountTrackerService()->SeedAccountInfo(kTestGaiaId,
                                                                  kTestEmail);
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      primary_account_id(), ConsentLevel::kSync,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  SetRefreshTokenForAccount(identity_manager(), primary_account_id(),
                            "refresh_token");

  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});

  // We first create and AccessTokenFetcher with a custom URLLoaderFactory,
  // to check that such factory is actually used in the requests generated.
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_url_loader_factory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory));
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          primary_account_id(), kTestConsumerId, test_shared_url_loader_factory,
          scopes, std::move(callback), AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  // The URLLoaderFactory present in the pending request should match
  // the one we specified when creating the AccessTokenFetcher.
  std::vector<FakeOAuth2AccessTokenManager::PendingRequest> pending_requests =
      token_service()->GetPendingRequests();
  EXPECT_EQ(pending_requests.size(), 1U);
  EXPECT_EQ(pending_requests[0].url_loader_factory,
            test_shared_url_loader_factory);

  // The account ID and consumer's name should match the data passed as well.
  EXPECT_EQ(
      primary_account_id(),
      identity_manager_diagnostics_observer()->token_requestor_account_id());
  EXPECT_EQ(
      kTestConsumerId,
      identity_manager_diagnostics_observer()->token_requestor_consumer_id());

  // Cancel the pending request in preparation to check that creating an
  // AccessTokenFetcher without a custom factory works as expected as well.
  token_service()->IssueErrorForAllPendingRequestsForAccount(
      primary_account_id(),
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));

  // Now add a second account and request an access token for it to test
  // that the default URLLoaderFactory is used if none is specified.
  base::RunLoop run_loop2;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_requested_callback(run_loop2.QuitClosure());

  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  CoreAccountId account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id2, "refresh_token");

  // No changes to the declared scopes, we can reuse it.
  callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::unique_ptr<AccessTokenFetcher> token_fetcher2 =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          account_id2, kTestConsumerId2, scopes, std::move(callback),
          AccessTokenFetcher::Mode::kImmediate);

  run_loop2.Run();

  // There should be one pending request now as well, just like before.
  std::vector<FakeOAuth2AccessTokenManager::PendingRequest> pending_requests2 =
      token_service()->GetPendingRequests();
  EXPECT_EQ(pending_requests2.size(), 1U);

  // The URLLoaderFactory present in the pending request should match
  // the one created by default for the token service's delegate.
  ProfileOAuth2TokenServiceDelegate* service_delegate =
      token_service()->GetDelegate();
  EXPECT_EQ(pending_requests2[0].url_loader_factory,
            service_delegate->GetURLLoaderFactory());

  // The account ID and consumer's name should match the data passed again.
  EXPECT_EQ(
      account_id2,
      identity_manager_diagnostics_observer()->token_requestor_account_id());
  EXPECT_EQ(
      kTestConsumerId2,
      identity_manager_diagnostics_observer()->token_requestor_consumer_id());
}

TEST_F(IdentityManagerTest, ObserveAccessTokenFetch) {
  base::RunLoop run_loop;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_requested_callback(run_loop.QuitClosure());

  identity_manager()->GetAccountTrackerService()->SeedAccountInfo(kTestGaiaId,
                                                                  kTestEmail);
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      primary_account_id(), ConsentLevel::kSync,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  SetRefreshTokenForAccount(identity_manager(), primary_account_id(),
                            "refresh_token");

  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync),
          kTestConsumerId, scopes, std::move(callback),
          AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  EXPECT_EQ(
      primary_account_id(),
      identity_manager_diagnostics_observer()->token_requestor_account_id());
  EXPECT_EQ(
      kTestConsumerId,
      identity_manager_diagnostics_observer()->token_requestor_consumer_id());
  EXPECT_EQ(scopes,
            identity_manager_diagnostics_observer()->token_requestor_scopes());
}

TEST_F(IdentityManagerTest,
       ObserveAccessTokenRequestCompletionWithoutRefreshToken) {
  base::RunLoop run_loop;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_request_completed_callback(run_loop.QuitClosure());

  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  // Account has no refresh token.
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync),
          kTestConsumerId, scopes, std::move(callback),
          AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  EXPECT_TRUE(token_fetcher);
  EXPECT_EQ(GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP),
            identity_manager_diagnostics_observer()
                ->on_access_token_request_completed_error());
}

TEST_F(IdentityManagerTest,
       ObserveAccessTokenRequestCompletionWithRefreshToken) {
  base::RunLoop run_loop;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_request_completed_callback(run_loop.QuitClosure());

  identity_manager()->GetAccountTrackerService()->SeedAccountInfo(kTestGaiaId,
                                                                  kTestEmail);
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      primary_account_id(), ConsentLevel::kSync,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  SetRefreshTokenForAccount(identity_manager(), primary_account_id(),
                            "refresh_token");
  token_service()->set_auto_post_fetch_response_on_message_loop(true);

  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  // This should result in a request for an access token without an error.
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          identity_manager()->GetPrimaryAccountId(signin::ConsentLevel::kSync),
          kTestConsumerId, scopes, std::move(callback),
          AccessTokenFetcher::Mode::kImmediate);

  run_loop.Run();

  EXPECT_TRUE(token_fetcher);
  EXPECT_EQ(primary_account_id(),
            identity_manager_diagnostics_observer()
                ->on_access_token_request_completed_account_id());
  EXPECT_EQ(kTestConsumerId,
            identity_manager_diagnostics_observer()
                ->on_access_token_request_completed_consumer_id());
  EXPECT_EQ(scopes, identity_manager_diagnostics_observer()
                        ->on_access_token_request_completed_scopes());
  EXPECT_EQ(GoogleServiceAuthError(GoogleServiceAuthError::NONE),
            identity_manager_diagnostics_observer()
                ->on_access_token_request_completed_error());
}

TEST_F(IdentityManagerTest,
       ObserveAccessTokenRequestCompletionAfterRevokingRefreshToken) {
  base::RunLoop run_loop;
  identity_manager_diagnostics_observer()
      ->set_on_access_token_request_completed_callback(run_loop.QuitClosure());

  account_tracker()->SeedAccountInfo(kTestGaiaId2, kTestEmail2);
  CoreAccountId account_id2 =
      account_tracker()->FindAccountInfoByGaiaId(kTestGaiaId2).account_id;
  SetRefreshTokenForAccount(identity_manager(), account_id2, "refresh_token");

  std::set<std::string> scopes{"scope"};
  AccessTokenFetcher::TokenCallback callback = base::BindOnce(
      [](GoogleServiceAuthError error, AccessTokenInfo access_token_info) {});
  // This should result in a request for an access token.
  std::unique_ptr<AccessTokenFetcher> token_fetcher =
      identity_manager()->CreateAccessTokenFetcherForAccount(
          account_id2, kTestConsumerId2, scopes, std::move(callback),
          AccessTokenFetcher::Mode::kImmediate);

  // Revoke the refresh token result cancelling access token request.
  RemoveRefreshTokenForAccount(identity_manager(), account_id2);

  run_loop.Run();

  EXPECT_TRUE(token_fetcher);
  EXPECT_EQ(GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP),
            identity_manager_diagnostics_observer()
                ->on_access_token_request_completed_error());
}

TEST_F(IdentityManagerTest, GetAccountsCookieMutator) {
  AccountsCookieMutator* mutator =
      identity_manager()->GetAccountsCookieMutator();
  EXPECT_TRUE(mutator);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// On ChromeOS, AccountTrackerService first receives the normalized email
// address from GAIA and then later has it updated with the user's
// originally-specified version of their email address (at the time of that
// address' creation). This latter will differ if the user's originally-
// specified address was not in normalized form (e.g., if it contained
// periods). This test simulates such a flow in order to verify that
// IdentityManager correctly reflects the updated version. See crbug.com/842041
// and crbug.com/842670 for further details.
TEST_F(IdentityManagerTest, IdentityManagerReflectsUpdatedEmailAddress) {
  CoreAccountInfo primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmail, primary_account_info.email);

  // Simulate the flow wherein the user's email address was updated
  // to the originally-created non-normalized version.
  SimulateSuccessfulFetchOfAccountInfo(
      identity_manager(), primary_account_info.account_id, kTestEmailWithPeriod,
      kTestGaiaId, kTestHostedDomain, kTestFullName, kTestGivenName,
      kTestLocale, kTestPictureUrl);
  // Verify that IdentityManager reflects the update.
  primary_account_info =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
  EXPECT_EQ(kTestGaiaId, primary_account_info.gaia);
  EXPECT_EQ(kTestEmailWithPeriod, primary_account_info.email);
  EXPECT_EQ(identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin),
            primary_account_info);
}
#endif

TEST_F(IdentityManagerTest,
       CallbackSentOnPrimaryAccountRefreshTokenUpdateWithValidToken) {
  SetRefreshTokenForPrimaryAccount(identity_manager());

  CoreAccountInfo account_info =
      identity_manager_observer()->AccountFromRefreshTokenUpdatedCallback();
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
}

TEST_F(IdentityManagerTest,
       CallbackSentOnPrimaryAccountRefreshTokenUpdateWithInvalidToken) {
  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager());

  CoreAccountInfo account_info =
      identity_manager_observer()->AccountFromRefreshTokenUpdatedCallback();
  EXPECT_EQ(kTestGaiaId, account_info.gaia);
  EXPECT_EQ(kTestEmail, account_info.email);
}

TEST_F(IdentityManagerTest, CallbackSentOnPrimaryAccountRefreshTokenRemoval) {
  SetRefreshTokenForPrimaryAccount(identity_manager());

  RemoveRefreshTokenForPrimaryAccount(identity_manager());

  EXPECT_EQ(
      primary_account_id(),
      identity_manager_observer()->AccountIdFromRefreshTokenRemovedCallback());
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSecondaryAccountRefreshTokenUpdateWithValidToken) {
  AccountInfo expected_account_info =
      MakeAccountAvailable(identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  CoreAccountInfo account_info =
      identity_manager_observer()->AccountFromRefreshTokenUpdatedCallback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSecondaryAccountRefreshTokenUpdateWithInvalidToken) {
  AccountInfo expected_account_info =
      MakeAccountAvailable(identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  SetInvalidRefreshTokenForAccount(identity_manager(),
                                   expected_account_info.account_id);

  CoreAccountInfo account_info =
      identity_manager_observer()->AccountFromRefreshTokenUpdatedCallback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);
}

TEST_F(IdentityManagerTest, CallbackSentOnSecondaryAccountRefreshTokenRemoval) {
  AccountInfo expected_account_info =
      MakeAccountAvailable(identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  RemoveRefreshTokenForAccount(identity_manager(),
                               expected_account_info.account_id);

  EXPECT_EQ(
      expected_account_info.account_id,
      identity_manager_observer()->AccountIdFromRefreshTokenRemovedCallback());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(
    IdentityManagerTest,
    CallbackSentOnSecondaryAccountRefreshTokenUpdateWithValidTokenWhenNoPrimaryAccount) {
  ClearPrimaryAccount(identity_manager());

  // Add an unconsented primary account, incl. proper cookies.
  AccountInfo expected_account_info = MakeAccountAvailable(
      identity_manager(),
      AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie(true)
          .WithGaiaId(kTestGaiaId2)
          .Build(kTestEmail2));
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  CoreAccountInfo account_info =
      identity_manager_observer()->AccountFromRefreshTokenUpdatedCallback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);
}

TEST_F(
    IdentityManagerTest,
    CallbackSentOnSecondaryAccountRefreshTokenUpdateWithInvalidTokenWhenNoPrimaryAccount) {
  ClearPrimaryAccount(identity_manager());

  // Add an unconsented primary account, incl. proper cookies.
  AccountInfo expected_account_info = MakeAccountAvailable(
      identity_manager(),
      AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie(true)
          .WithGaiaId(kTestGaiaId2)
          .Build(kTestEmail2));
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  SetInvalidRefreshTokenForAccount(identity_manager(),
                                   expected_account_info.account_id);

  CoreAccountInfo account_info =
      identity_manager_observer()->AccountFromRefreshTokenUpdatedCallback();
  EXPECT_EQ(expected_account_info.account_id, account_info.account_id);
  EXPECT_EQ(expected_account_info.gaia, account_info.gaia);
  EXPECT_EQ(expected_account_info.email, account_info.email);
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSecondaryAccountRefreshTokenRemovalWhenNoPrimaryAccount) {
  ClearPrimaryAccount(identity_manager());

  // Add an unconsented primary account, incl. proper cookies.
  AccountInfo expected_account_info = MakeAccountAvailable(
      identity_manager(),
      AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie(true)
          .WithGaiaId(kTestGaiaId2)
          .Build(kTestEmail2));
  EXPECT_EQ(kTestEmail2, expected_account_info.email);

  RemoveRefreshTokenForAccount(identity_manager(),
                               expected_account_info.account_id);

  EXPECT_EQ(
      expected_account_info.account_id,
      identity_manager_observer()->AccountIdFromRefreshTokenRemovedCallback());
}

TEST_F(IdentityManagerTest, CallbackSentOnRefreshTokenRemovalOfUnknownAccount) {
  // When the token service is still loading credentials, it may send token
  // revoked callbacks for accounts that it has never sent a token available
  // callback. Our common test setup actually completes this loading, so use the
  // *for_testing() method below to simulate the race condition and ensure that
  // IdentityManager passes on the callback in this case.
  token_service()->set_all_credentials_loaded_for_testing(false);

  CoreAccountId dummy_account_id = CoreAccountId::FromGaiaId("dummy_account");

  base::RunLoop run_loop;
  token_service()->RevokeCredentials(dummy_account_id);
  run_loop.RunUntilIdle();

  EXPECT_EQ(
      dummy_account_id,
      identity_manager_observer()->AccountIdFromRefreshTokenRemovedCallback());
}
#endif

TEST_F(IdentityManagerTest, IdentityManagerGetsTokensLoadedEvent) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnRefreshTokensLoadedCallback(
      run_loop.QuitClosure());

  // Credentials are already loaded in PrimaryAccountManager::Initialize()
  // which runs even before the IdentityManager is created. That's why
  // we fake the credentials loaded state and force another load in
  // order to be able to capture the TokensLoaded event.
  token_service()->set_all_credentials_loaded_for_testing(false);
  token_service()->LoadCredentials(CoreAccountId(), /*is_syncing=*/false);
  run_loop.Run();
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToAccountsInCookieWithNoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  SetListAccountsResponseNoAccounts(test_url_loader_factory());
  identity_manager()->GetGaiaCookieManagerService()->TriggerListAccounts();
  run_loop.Run();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_info =
      identity_manager_observer()
          ->AccountsInfoFromAccountsInCookieUpdatedCallback();
  EXPECT_TRUE(accounts_in_cookie_jar_info.AreAccountsFresh());
  EXPECT_TRUE(
      accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
          .empty());
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToAccountsInCookieWithOneAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  SetListAccountsResponseOneAccount(kTestEmail, kTestGaiaId,
                                    test_url_loader_factory());
  identity_manager()->GetGaiaCookieManagerService()->TriggerListAccounts();
  run_loop.Run();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_info =
      identity_manager_observer()
          ->AccountsInfoFromAccountsInCookieUpdatedCallback();
  EXPECT_TRUE(accounts_in_cookie_jar_info.AreAccountsFresh());
  ASSERT_EQ(1u,
            accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
                .size());
  ASSERT_TRUE(accounts_in_cookie_jar_info.GetSignedOutAccounts().empty());

  gaia::ListedAccount listed_account =
      accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()[0];
  EXPECT_EQ(
      identity_manager()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
      listed_account.id);
  EXPECT_EQ(kTestGaiaId, listed_account.gaia_id);
  EXPECT_EQ(kTestEmail, listed_account.email);
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToAccountsInCookieWithTwoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  SetListAccountsResponseTwoAccounts(kTestEmail, kTestGaiaId, kTestEmail2,
                                     kTestGaiaId2, test_url_loader_factory());
  identity_manager()->GetGaiaCookieManagerService()->TriggerListAccounts();
  run_loop.Run();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_info =
      identity_manager_observer()
          ->AccountsInfoFromAccountsInCookieUpdatedCallback();
  EXPECT_TRUE(accounts_in_cookie_jar_info.AreAccountsFresh());
  ASSERT_EQ(2u,
            accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
                .size());
  ASSERT_TRUE(accounts_in_cookie_jar_info.GetSignedOutAccounts().empty());

  // Verify not only that both accounts are present but that they are listed in
  // the expected order as well.
  gaia::ListedAccount listed_account1 =
      accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()[0];
  EXPECT_EQ(
      identity_manager()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
      listed_account1.id);
  EXPECT_EQ(kTestGaiaId, listed_account1.gaia_id);
  EXPECT_EQ(kTestEmail, listed_account1.email);

  gaia::ListedAccount account_info2 =
      accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()[1];
  EXPECT_EQ(
      identity_manager()->PickAccountIdForAccount(kTestGaiaId2, kTestEmail2),
      account_info2.id);
  EXPECT_EQ(kTestGaiaId2, account_info2.gaia_id);
  EXPECT_EQ(kTestEmail2, account_info2.email);
}

TEST_F(IdentityManagerTest, CallbackSentOnUpdateToSignOutAccountsInCookie) {
  struct SignedOutStatus {
    bool account_1;
    bool account_2;
  } signed_out_status_set[] = {
      {false, false}, {true, false}, {false, true}, {true, true}};

  for (const auto& signed_out_status : signed_out_status_set) {
    base::RunLoop run_loop;
    identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
        run_loop.QuitClosure());

    SetListAccountsResponseWithParams(
        {{kTestEmail, kTestGaiaId, true /* valid */,
          signed_out_status.account_1 /* signed_out */, true /* verified */},
         {kTestEmail2, kTestGaiaId2, true /* valid */,
          signed_out_status.account_2 /* signed_out */, true /* verified */}},
        test_url_loader_factory());

    identity_manager()->GetGaiaCookieManagerService()->TriggerListAccounts();
    run_loop.Run();

    size_t accounts_signed_out = size_t{signed_out_status.account_1} +
                                 size_t{signed_out_status.account_2};
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info =
        identity_manager_observer()
            ->AccountsInfoFromAccountsInCookieUpdatedCallback();
    EXPECT_TRUE(accounts_in_cookie_jar_info.AreAccountsFresh());
    ASSERT_EQ(
        2 - accounts_signed_out,
        accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
            .size());
    ASSERT_EQ(accounts_signed_out,
              accounts_in_cookie_jar_info.GetSignedOutAccounts().size());

    // Verify not only that both accounts are present but that they are listed
    // in the expected order as well.
    //
    // The two variables below, control the lookup indexes signed in and signed
    // out accounts list, respectively.
    int i = 0, j = 0;
    gaia::ListedAccount listed_account1 =
        signed_out_status.account_1
            ? accounts_in_cookie_jar_info.GetSignedOutAccounts()[i++]
            : accounts_in_cookie_jar_info
                  .GetPotentiallyInvalidSignedInAccounts()[j++];
    if (!signed_out_status.account_1)
      EXPECT_EQ(
          identity_manager()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
          listed_account1.id);
    EXPECT_EQ(kTestGaiaId, listed_account1.gaia_id);
    EXPECT_EQ(kTestEmail, listed_account1.email);

    gaia::ListedAccount listed_account2 =
        signed_out_status.account_2
            ? accounts_in_cookie_jar_info.GetSignedOutAccounts()[i++]
            : accounts_in_cookie_jar_info
                  .GetPotentiallyInvalidSignedInAccounts()[j++];
    if (!signed_out_status.account_2)
      EXPECT_EQ(identity_manager()->PickAccountIdForAccount(kTestGaiaId2,
                                                            kTestEmail2),
                listed_account2.id);
    EXPECT_EQ(kTestGaiaId2, listed_account2.gaia_id);
    EXPECT_EQ(kTestEmail2, listed_account2.email);
  }
}

TEST_F(IdentityManagerTest,
       CallbackSentOnUpdateToAccountsInCookieWithStaleAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  // Configure list accounts to return a permanent Gaia auth error.
  SetListAccountsResponseWithUnexpectedServiceResponse(
      test_url_loader_factory());
  identity_manager()->GetGaiaCookieManagerService()->TriggerListAccounts();
  run_loop.Run();

  const AccountsInCookieJarInfo& accounts_in_cookie_jar_info =
      identity_manager_observer()
          ->AccountsInfoFromAccountsInCookieUpdatedCallback();
  EXPECT_FALSE(accounts_in_cookie_jar_info.AreAccountsFresh());
  EXPECT_TRUE(
      accounts_in_cookie_jar_info.GetPotentiallyInvalidSignedInAccounts()
          .empty());
  EXPECT_TRUE(accounts_in_cookie_jar_info.GetSignedOutAccounts().empty());
}

TEST_F(IdentityManagerTest, GetAccountsInCookieJarWithNoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  SetListAccountsResponseNoAccounts(test_url_loader_factory());

  // Do an initial call to GetAccountsInCookieJar(). This call should return no
  // accounts but should also trigger an internal update and eventual
  // notification that the accounts in the cookie jar have been updated.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_FALSE(accounts_in_cookie_jar.AreAccountsFresh());
  EXPECT_TRUE(
      accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts().empty());
  EXPECT_TRUE(accounts_in_cookie_jar.GetSignedOutAccounts().empty());

  run_loop.Run();

  // The state of the accounts in IdentityManager should now reflect the
  // internal update.
  const AccountsInCookieJarInfo updated_accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();

  EXPECT_TRUE(updated_accounts_in_cookie_jar.AreAccountsFresh());
  EXPECT_TRUE(
      updated_accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts()
          .empty());
  EXPECT_TRUE(updated_accounts_in_cookie_jar.GetSignedOutAccounts().empty());
}

TEST_F(IdentityManagerTest, GetAccountsInCookieJarWithOneAccount) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  SetListAccountsResponseOneAccount(kTestEmail, kTestGaiaId,
                                    test_url_loader_factory());

  // Do an initial call to GetAccountsInCookieJar(). This call should return no
  // accounts but should also trigger an internal update and eventual
  // notification that the accounts in the cookie jar have been updated.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_FALSE(accounts_in_cookie_jar.AreAccountsFresh());
  EXPECT_TRUE(
      accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts().empty());
  EXPECT_TRUE(accounts_in_cookie_jar.GetSignedOutAccounts().empty());

  run_loop.Run();

  // The state of the accounts in IdentityManager should now reflect the
  // internal update.
  const AccountsInCookieJarInfo& updated_accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();

  EXPECT_TRUE(updated_accounts_in_cookie_jar.AreAccountsFresh());
  ASSERT_EQ(
      1u, updated_accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts()
              .size());
  ASSERT_TRUE(updated_accounts_in_cookie_jar.GetSignedOutAccounts().empty());

  gaia::ListedAccount listed_account =
      updated_accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts()[0];
  EXPECT_EQ(
      identity_manager()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
      listed_account.id);
  EXPECT_EQ(kTestGaiaId, listed_account.gaia_id);
  EXPECT_EQ(kTestEmail, listed_account.email);
}

TEST_F(IdentityManagerTest, GetAccountsInCookieJarWithTwoAccounts) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnAccountsInCookieUpdatedCallback(
      run_loop.QuitClosure());

  SetListAccountsResponseTwoAccounts(kTestEmail, kTestGaiaId, kTestEmail2,
                                     kTestGaiaId2, test_url_loader_factory());

  // Do an initial call to GetAccountsInCookieJar(). This call should return no
  // accounts but should also trigger an internal update and eventual
  // notification that the accounts in the cookie jar have been updated.
  const AccountsInCookieJarInfo& accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();
  EXPECT_FALSE(accounts_in_cookie_jar.AreAccountsFresh());
  EXPECT_TRUE(
      accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts().empty());
  EXPECT_TRUE(accounts_in_cookie_jar.GetSignedOutAccounts().empty());

  run_loop.Run();

  // The state of the accounts in IdentityManager should now reflect the
  // internal update.
  const AccountsInCookieJarInfo& updated_accounts_in_cookie_jar =
      identity_manager()->GetAccountsInCookieJar();

  EXPECT_TRUE(updated_accounts_in_cookie_jar.AreAccountsFresh());
  ASSERT_EQ(
      2u, updated_accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts()
              .size());
  ASSERT_TRUE(updated_accounts_in_cookie_jar.GetSignedOutAccounts().empty());

  // Verify not only that both accounts are present but that they are listed in
  // the expected order as well.
  gaia::ListedAccount listed_account1 =
      updated_accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts()[0];
  EXPECT_EQ(
      identity_manager()->PickAccountIdForAccount(kTestGaiaId, kTestEmail),
      listed_account1.id);
  EXPECT_EQ(kTestGaiaId, listed_account1.gaia_id);
  EXPECT_EQ(kTestEmail, listed_account1.email);

  gaia::ListedAccount listed_account2 =
      updated_accounts_in_cookie_jar.GetPotentiallyInvalidSignedInAccounts()[1];
  EXPECT_EQ(
      identity_manager()->PickAccountIdForAccount(kTestGaiaId2, kTestEmail2),
      listed_account2.id);
  EXPECT_EQ(kTestGaiaId2, listed_account2.gaia_id);
  EXPECT_EQ(kTestEmail2, listed_account2.email);
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSetAccountsInCookieCompleted_Success) {
  const CoreAccountId kTestAccountId = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId kTestAccountId2 =
      CoreAccountId::FromGaiaId("account_id2");
  const std::vector<std::pair<CoreAccountId, std::string>> accounts = {
      {kTestAccountId, kTestAccountId.ToString()},
      {kTestAccountId2, kTestAccountId2.ToString()}};

  SetAccountsInCookieResult
      error_from_set_accounts_in_cookie_completed_callback;
  auto completion_callback = base::BindLambdaForTesting(
      [&error_from_set_accounts_in_cookie_completed_callback](
          SetAccountsInCookieResult error) {
        error_from_set_accounts_in_cookie_completed_callback = error;
      });

  // Needed to insert request in the queue.
  identity_manager()->GetGaiaCookieManagerService()->SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER, accounts,
      gaia::GaiaSource::kChrome, std::move(completion_callback));

  SimulateOAuthMultiloginFinished(
      identity_manager()->GetGaiaCookieManagerService(),
      SetAccountsInCookieResult::kSuccess);

  EXPECT_EQ(error_from_set_accounts_in_cookie_completed_callback,
            SetAccountsInCookieResult::kSuccess);
}

TEST_F(IdentityManagerTest,
       CallbackSentOnSetAccountsInCookieCompleted_Failure) {
  const CoreAccountId kTestAccountId = CoreAccountId::FromGaiaId("account_id");
  const CoreAccountId kTestAccountId2 =
      CoreAccountId::FromGaiaId("account_id2");
  const std::vector<std::pair<CoreAccountId, std::string>> accounts = {
      {kTestAccountId, kTestAccountId.ToString()},
      {kTestAccountId2, kTestAccountId2.ToString()}};

  SetAccountsInCookieResult
      error_from_set_accounts_in_cookie_completed_callback;
  auto completion_callback = base::BindLambdaForTesting(
      [&error_from_set_accounts_in_cookie_completed_callback](
          SetAccountsInCookieResult error) {
        error_from_set_accounts_in_cookie_completed_callback = error;
      });

  // Needed to insert request in the queue.
  identity_manager()->GetGaiaCookieManagerService()->SetAccountsInCookie(
      gaia::MultiloginMode::MULTILOGIN_UPDATE_COOKIE_ACCOUNTS_ORDER, accounts,
      gaia::GaiaSource::kChrome, std::move(completion_callback));

  // Sample an erroneous response.
  SetAccountsInCookieResult error = SetAccountsInCookieResult::kPersistentError;

  SimulateOAuthMultiloginFinished(
      identity_manager()->GetGaiaCookieManagerService(), error);

  EXPECT_EQ(error_from_set_accounts_in_cookie_completed_callback, error);
}

TEST_F(IdentityManagerTest, CallbackSentOnAccountsCookieDeletedByUserAction) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnCookieDeletedByUserCallback(
      run_loop.QuitClosure());
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "SAPISID", std::string(), ".google.com", "/", base::Time(), base::Time(),
      base::Time(), base::Time(), /*secure=*/true, false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  SimulateCookieDeletedByUser(identity_manager()->GetGaiaCookieManagerService(),
                              *cookie);
  run_loop.Run();
}

TEST_F(IdentityManagerTest, OnNetworkInitialized) {
  auto test_cookie_manager = std::make_unique<network::TestCookieManager>();
  network::TestCookieManager* test_cookie_manager_ptr =
      test_cookie_manager.get();
  signin_client()->set_cookie_manager(std::move(test_cookie_manager));

  identity_manager()->OnNetworkInitialized();

  base::RunLoop run_loop;
  identity_manager_observer()->SetOnCookieDeletedByUserCallback(
      run_loop.QuitClosure());

  // Dispatch a known change of a known cookie instance *through the mojo
  // pipe* in order to ensure the GCMS is listening to CookieManager changes.
  //
  // It is important the the cause of the change is known here (ie
  // network::mojom::CookieChangeCause::EXPLICIT) so the test can block of the
  // proper IdentityManager observer callback to be called (in this case
  // OnAccountsCookieDeletedByUserAction).
  //
  // Note that this call differs from calling SimulateCookieDeletedByUser()
  // directly in the sense that SimulateCookieDeletedByUser() does not go
  // through any mojo pipe.
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "SAPISID", std::string(), ".google.com", "/", base::Time(), base::Time(),
      base::Time(), base::Time(), /*secure=*/true, false,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT);
  test_cookie_manager_ptr->DispatchCookieChange(net::CookieChangeInfo(
      *cookie, net::CookieAccessResult(), net::CookieChangeCause::EXPLICIT));
  run_loop.Run();
}

TEST_F(IdentityManagerTest,
       BatchChangeObserversAreNotifiedOnCredentialsUpdate) {
  identity_manager()->GetAccountTrackerService()->SeedAccountInfo(kTestGaiaId,
                                                                  kTestEmail);
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      primary_account_id(), ConsentLevel::kSync,
      signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  SetRefreshTokenForAccount(identity_manager(), primary_account_id(),
                            "refresh_token");

  EXPECT_EQ(1ul, identity_manager_observer()->BatchChangeRecords().size());
  EXPECT_EQ(1ul,
            identity_manager_observer()->BatchChangeRecords().at(0).size());
  EXPECT_EQ(primary_account_id(),
            identity_manager_observer()->BatchChangeRecords().at(0).at(0));
}

// Check that FindExtendedAccountInfo returns a valid account info iff the
// account is known, and all the extended information is available.
TEST_F(IdentityManagerTest, FindExtendedAccountInfo) {
  CoreAccountInfo account_info;
  account_info.email = kTestEmail2;
  account_info.gaia = kTestGaiaId2;
  account_info.account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia, account_info.email);

  // FindExtendedAccountInfo() returns empty if the account_info is invalid.
  EXPECT_TRUE(
      identity_manager()->FindExtendedAccountInfo(CoreAccountInfo{}).IsEmpty());

  // FindExtendedAccountInfo() returns empty if the account_info is unknown.
  EXPECT_TRUE(
      identity_manager()->FindExtendedAccountInfo(account_info).IsEmpty());

  // Insert the core account information in the AccountTrackerService.
  const CoreAccountId account_id =
      account_tracker()->SeedAccountInfo(account_info.gaia, account_info.email);
  ASSERT_EQ(account_info.account_id, account_id);

  // The refresh token is not available.
  EXPECT_TRUE(
      identity_manager()->FindExtendedAccountInfo(account_info).IsEmpty());

  // FindExtendedAccountInfo() returns extended account information if the
  // account is known and the token is available.
  SetRefreshTokenForAccount(identity_manager(), account_info.account_id,
                            "token");
  const AccountInfo extended_account_info =
      identity_manager()->FindExtendedAccountInfo(account_info);
  EXPECT_TRUE(!extended_account_info.IsEmpty());
  EXPECT_EQ(account_info.gaia, extended_account_info.gaia);
  EXPECT_EQ(account_info.email, extended_account_info.email);
  EXPECT_EQ(account_info.account_id, extended_account_info.account_id);
}

// Checks that FindExtendedAccountInfoByAccountId() returns information about
// the account if the account is found, or an empty account info.
TEST_F(IdentityManagerTest, FindExtendedAccountInfoByAccountId) {
  CoreAccountInfo account_info;
  account_info.email = kTestEmail2;
  account_info.gaia = kTestGaiaId2;
  account_info.account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia, account_info.email);

  // Account is unknown.
  AccountInfo maybe_account_info =
      identity_manager()->FindExtendedAccountInfoByAccountId(
          account_info.account_id);
  EXPECT_TRUE(maybe_account_info.IsEmpty());

  // Refresh token is not available.
  const CoreAccountId account_id =
      account_tracker()->SeedAccountInfo(account_info.gaia, account_info.email);
  maybe_account_info = identity_manager()->FindExtendedAccountInfoByAccountId(
      account_info.account_id);
  EXPECT_TRUE(maybe_account_info.IsEmpty());

  // Account with refresh token.
  SetRefreshTokenForAccount(identity_manager(), account_info.account_id,
                            "token");
  maybe_account_info = identity_manager()->FindExtendedAccountInfoByAccountId(
      account_info.account_id);
  EXPECT_FALSE(maybe_account_info.IsEmpty());
  EXPECT_EQ(account_info.account_id, maybe_account_info.account_id);
  EXPECT_EQ(account_info.email, maybe_account_info.email);
  EXPECT_EQ(account_info.gaia, maybe_account_info.gaia);
}

// Checks that FindExtendedAccountInfoByEmailAddress() returns information about
// the account if the account is found, or an empty account info.
TEST_F(IdentityManagerTest, FindExtendedAccountInfoByEmailAddress) {
  CoreAccountInfo account_info;
  account_info.email = kTestEmail2;
  account_info.gaia = kTestGaiaId2;
  account_info.account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia, account_info.email);

  // Account is unknown.
  AccountInfo maybe_account_info =
      identity_manager()->FindExtendedAccountInfoByEmailAddress(
          account_info.email);
  EXPECT_TRUE(maybe_account_info.IsEmpty());

  // Refresh token is not available.
  const CoreAccountId account_id =
      account_tracker()->SeedAccountInfo(account_info.gaia, account_info.email);
  maybe_account_info =
      identity_manager()->FindExtendedAccountInfoByEmailAddress(
          account_info.email);
  EXPECT_TRUE(maybe_account_info.IsEmpty());

  // Account with refresh token.
  SetRefreshTokenForAccount(identity_manager(), account_info.account_id,
                            "token");
  maybe_account_info =
      identity_manager()->FindExtendedAccountInfoByEmailAddress(
          account_info.email);
  EXPECT_FALSE(maybe_account_info.IsEmpty());
  EXPECT_EQ(account_info.account_id, maybe_account_info.account_id);
  EXPECT_EQ(account_info.email, maybe_account_info.email);
  EXPECT_EQ(account_info.gaia, maybe_account_info.gaia);
}

// Checks that FindExtendedAccountInfoByGaiaId() returns information about the
// account if the account is found, or an empty account info.
TEST_F(IdentityManagerTest, FindExtendedAccountInfoByGaiaId) {
  CoreAccountInfo account_info;
  account_info.email = kTestEmail2;
  account_info.gaia = kTestGaiaId2;
  account_info.account_id = identity_manager()->PickAccountIdForAccount(
      account_info.gaia, account_info.email);

  // Account is unknown.
  AccountInfo maybe_account_info =
      identity_manager()->FindExtendedAccountInfoByGaiaId(account_info.gaia);
  EXPECT_TRUE(maybe_account_info.IsEmpty());

  // Refresh token is not available.
  const CoreAccountId account_id =
      account_tracker()->SeedAccountInfo(account_info.gaia, account_info.email);
  maybe_account_info =
      identity_manager()->FindExtendedAccountInfoByGaiaId(account_info.gaia);
  EXPECT_TRUE(maybe_account_info.IsEmpty());

  // Account with refresh token.
  SetRefreshTokenForAccount(identity_manager(), account_info.account_id,
                            "token");
  maybe_account_info =
      identity_manager()->FindExtendedAccountInfoByGaiaId(account_info.gaia);
  EXPECT_FALSE(maybe_account_info.IsEmpty());
  EXPECT_EQ(account_info.account_id, maybe_account_info.account_id);
  EXPECT_EQ(account_info.email, maybe_account_info.email);
  EXPECT_EQ(account_info.gaia, maybe_account_info.gaia);
}

TEST_F(IdentityManagerTest, FindExtendedPrimaryAccountInfo) {
  // AccountInfo found for primary account, even without token.
  RemoveRefreshTokenForPrimaryAccount(identity_manager());
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  ASSERT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      ConsentLevel::kSignin));
  AccountInfo extended_info =
      identity_manager()->FindExtendedPrimaryAccountInfo(ConsentLevel::kSignin);
  CoreAccountInfo core_info =
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin);
  EXPECT_FALSE(extended_info.IsEmpty());
  EXPECT_EQ(core_info.account_id, extended_info.account_id);
  EXPECT_EQ(core_info.email, extended_info.email);
  EXPECT_EQ(core_info.gaia, extended_info.gaia);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // It's not possible to sign out on Ash.
  ClearPrimaryAccount(identity_manager());
  SetRefreshTokenForAccount(identity_manager(), core_info.account_id, "token");
  // No info found if there is no primary account.
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  ASSERT_TRUE(
      identity_manager()->HasAccountWithRefreshToken(core_info.account_id));
  EXPECT_TRUE(identity_manager()
                  ->FindExtendedPrimaryAccountInfo(ConsentLevel::kSignin)
                  .IsEmpty());
#endif
}

// Checks that AreRefreshTokensLoaded() returns true after LoadCredentials.
TEST_F(IdentityManagerTest, AreRefreshTokensLoaded) {
  base::RunLoop run_loop;
  identity_manager_observer()->SetOnRefreshTokensLoadedCallback(
      run_loop.QuitClosure());

  // Credentials are already loaded in PrimaryAccountManager::Initialize()
  // which runs even before the IdentityManager is created. That's why
  // we fake the credentials loaded state and force another load in
  // order to test AreRefreshTokensLoaded.
  token_service()->set_all_credentials_loaded_for_testing(false);
  EXPECT_FALSE(identity_manager()->AreRefreshTokensLoaded());
  token_service()->LoadCredentials(CoreAccountId(), /*is_syncing=*/false);
  run_loop.Run();
  EXPECT_TRUE(identity_manager()->AreRefreshTokensLoaded());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(IdentityManagerTest, SetPrimaryAccount) {
  signin_client()->SetInitialPrimaryAccountForTests(
      account_manager::Account{
          account_manager::AccountKey{kTestGaiaId,
                                      account_manager::AccountType::kGaia},
          kTestEmail},
      /*is_child=*/false);
  // Do not sign into a primary account as part of the test setup.
  RecreateIdentityManager(AccountConsistencyMethod::kDisabled,
                          PrimaryAccountManagerSetup::kNoAuthenticatedAccount);

  // We should have a non-syncing Primary Account set up automatically.
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_EQ(
      kTestGaiaId,
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin).gaia);
}

// TODO(crbug.com/40774609): Remove this when all the users are migrated.
TEST_F(IdentityManagerTest, SetPrimaryAccountClearsExistingPrimaryAccount) {
  signin_client()->SetInitialPrimaryAccountForTests(
      account_manager::Account{
          account_manager::AccountKey{kTestGaiaId2,
                                      account_manager::AccountType::kGaia},
          kTestEmail2},
      /*is_child=*/false);

  // RecreateIdentityManager will create PrimaryAccountManager with the primary
  // account set to kTestGaiaId. After that, IdentityManager ctor should clear
  // this existing primary account and set the new one to the initial value
  // provided by the SigninClient.
  RecreateIdentityManager(AccountConsistencyMethod::kDisabled,
                          PrimaryAccountManagerSetup::kWithAuthenticatedAccout);
  ASSERT_TRUE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSignin));
  ASSERT_FALSE(identity_manager()->HasPrimaryAccount(ConsentLevel::kSync));
  EXPECT_EQ(
      kTestGaiaId2,
      identity_manager()->GetPrimaryAccountInfo(ConsentLevel::kSignin).gaia);
}
#endif

// Checks that IdentityManager::Observer gets OnAccountUpdated when account info
// is updated.
TEST_F(IdentityManagerTest, ObserveOnAccountUpdated) {
  const AccountInfo account_info =
      MakeAccountAvailable(identity_manager(), kTestEmail3);

  SimulateSuccessfulFetchOfAccountInfo(
      identity_manager(), account_info.account_id, account_info.email,
      account_info.gaia, kTestHostedDomain, kTestFullName, kTestGivenName,
      kTestLocale, kTestPictureUrl);

  EXPECT_EQ(account_info.account_id, identity_manager_observer()
                                         ->AccountFromAccountUpdatedCallback()
                                         .account_id);
  EXPECT_EQ(
      account_info.email,
      identity_manager_observer()->AccountFromAccountUpdatedCallback().email);
}

TEST_F(IdentityManagerTest, TestOnAccountRemovedWithInfoCallback) {
  AccountInfo account_info =
      MakeAccountAvailable(identity_manager(), kTestEmail2);
  EXPECT_EQ(kTestEmail2, account_info.email);

  account_tracker()->RemoveAccount(account_info.account_id);

  // Check if OnAccountRemovedWithInfo is called after removing |account_info|
  // by RemoveAccount().
  EXPECT_TRUE(
      identity_manager_observer()->WasCalledAccountRemovedWithInfoCallback());

  // Check if the passed AccountInfo is the same to the removing one.
  EXPECT_EQ(account_info.account_id,
            identity_manager_observer()
                ->AccountFromAccountRemovedWithInfoCallback()
                .account_id);
  EXPECT_EQ(account_info.email,
            identity_manager_observer()
                ->AccountFromAccountRemovedWithInfoCallback()
                .email);
}

TEST_F(IdentityManagerTest, TestPickAccountIdForAccount) {
  EXPECT_EQ(kTestGaiaId, identity_manager()
                             ->PickAccountIdForAccount(kTestGaiaId, kTestEmail)
                             .ToString());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(IdentityManagerTest, RefreshAccountInfoIfStale) {
  // The flow of this test results in an interaction with
  // ChildAccountInfoFetcherAndroid, which requires initialization of
  // AccountManagerFacade in java code to avoid a crash.
  SetUpMockAccountManagerFacade();

  identity_manager()->GetAccountFetcherService()->OnNetworkInitialized();
  AccountInfo account_info =
      MakeAccountAvailable(identity_manager(), kTestEmail2);
  identity_manager()->RefreshAccountInfoIfStale(account_info.account_id);

  SimulateSuccessfulFetchOfAccountInfo(
      identity_manager(), account_info.account_id, account_info.email,
      account_info.gaia, kTestHostedDomain, kTestFullName, kTestGivenName,
      kTestLocale, kTestPictureUrl);

  const AccountInfo& refreshed_account_info =
      identity_manager_observer()->AccountFromAccountUpdatedCallback();
  EXPECT_EQ(account_info.account_id, refreshed_account_info.account_id);
  EXPECT_EQ(account_info.email, refreshed_account_info.email);
  EXPECT_EQ(account_info.gaia, refreshed_account_info.gaia);
  EXPECT_EQ(kTestHostedDomain, refreshed_account_info.hosted_domain);
  EXPECT_EQ(kTestFullName, refreshed_account_info.full_name);
  EXPECT_EQ(kTestGivenName, refreshed_account_info.given_name);
  EXPECT_EQ(kTestLocale, refreshed_account_info.locale);
  EXPECT_EQ(kTestPictureUrl, refreshed_account_info.picture_url);
}
#endif

}  // namespace signin
