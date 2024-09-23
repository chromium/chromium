// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/turn_sync_on_helper.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_file_util.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_device_id_helper.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/test/base/fake_profile_manager.h"
#include "chrome/test/base/profile_waiter.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_service.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync/test/sync_user_settings_mock.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/account_manager/account_profile_mapper.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/mock_account_manager_facade.h"
#endif

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;

class TurnSyncOnHelperTest;

namespace {

const char kEmail[] = "foo@gmail.com";
const char kPreviousEmail[] = "notme@bar.com";
const char kPreviousGaiaId[] = "gaia_id_for_not_me_at_bar_com";
const char kEnterpriseEmail[] = "enterprise@managed.com";
const char kEnterpriseHostedDomain[] = "managed.com";
const char kUserAffiliationId[] = "user-affiliation-id";

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER;
const signin_metrics::PromoAction kSigninPromoAction =
    signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT;

struct ExpectedMetricsState {
  // Access point that triggered sign-in, might be different from the one
  // associated sync opt-in, which is always `kAccessPoint`.
  std::optional<signin_metrics::AccessPoint> sign_in_access_point =
      std::nullopt;

  // Whether TurnSyncOnHelper is expected to have recorded a sign-in.
  bool sign_in_recorded = false;

  bool sync_opt_in_started = false;
  bool sync_opt_in_completed = false;
  bool sync_settings_opened = false;
  std::optional<signin_metrics::ProfileSignout> profile_signout;
  bool sync_turn_off = false;
};

// Helper class to wait until an account has been removed from the
// `IdentityManager`.
class AccountRemovedWaiter : public signin::IdentityManager::Observer {
 public:
  AccountRemovedWaiter(signin::IdentityManager* identity_manager,
                       const CoreAccountId& account_id)
      : identity_manager_(identity_manager), account_id_(account_id) {
    DCHECK(identity_manager_);
  }

  void Wait() {
    if (!identity_manager_->HasAccountWithRefreshToken(account_id_)) {
      return;
    }
    observation_.Observe(identity_manager_.get());
    run_loop_.Run();
  }

 private:
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override {
    if (account_id != account_id_) {
      return;
    }
    observation_.Reset();
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const CoreAccountId account_id_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      observation_{this};
};

// Dummy delegate forwarding all the calls the test fixture.
// Owned by the TurnOnSyncHelper.
class TestTurnSyncOnHelperDelegate : public TurnSyncOnHelper::Delegate {
 public:
  explicit TestTurnSyncOnHelperDelegate(TurnSyncOnHelperTest* test_fixture);
  ~TestTurnSyncOnHelperDelegate() override;

 private:
  // TurnSyncOnHelper::Delegate:
  void ShowLoginError(const SigninUIError& error) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      signin::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  bool ShouldAbortBeforeShowSyncDisabledConfirmation() override;
  void ShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void SwitchToProfile(Profile* new_profile) override;

  raw_ptr<TurnSyncOnHelperTest> test_fixture_;
};

// Simple ProfileManager creating testing profiles and allowing to register a
// callback for the next profile creation.
class UnittestProfileManager : public FakeProfileManager {
 public:
  using ProfileBuilderCallback = base::RepeatingCallback<std::unique_ptr<
      TestingProfile>(const base::FilePath&, Delegate*, Profile::CreateMode)>;

  explicit UnittestProfileManager(
      const base::FilePath& user_data_dir,
      ProfileBuilderCallback profile_builder_callback)
      : FakeProfileManager(user_data_dir),
        profile_builder_callback_(std::move(profile_builder_callback)) {}

  void NextProfileCreatedCallback(
      base::OnceCallback<void(Profile*)> next_profile_created_callback) {
    next_profile_created_callback_ = std::move(next_profile_created_callback);
  }

  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Delegate* delegate,
      Profile::CreateMode create_mode) override {
    auto profile = profile_builder_callback_.Run(path, delegate, create_mode);
    if (next_profile_created_callback_) {
      std::move(next_profile_created_callback_).Run(profile.get());
    }
    return profile;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetAccountProfileMapper(std::unique_ptr<AccountProfileMapper> mapper) {
    FakeProfileManager::SetAccountProfileMapperForTests(std::move(mapper));
  }
#endif

 private:
  base::OnceCallback<void(Profile*)> next_profile_created_callback_;
  ProfileBuilderCallback profile_builder_callback_;
};

// Fake user policy signin service immediately invoking the callbacks.
class FakeUserPolicySigninService : public policy::UserPolicySigninService {
 public:
  // Static method to use with BrowserContextKeyedServiceFactory.
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    return std::make_unique<FakeUserPolicySigninService>(
        profile, IdentityManagerFactory::GetForProfile(profile));
  }

  FakeUserPolicySigninService(Profile* profile,
                              signin::IdentityManager* identity_manager)
      : UserPolicySigninService(profile,
                                nullptr,
                                nullptr,
                                nullptr,
                                identity_manager,
                                nullptr) {
    add_user_affiliation_id(kUserAffiliationId);
  }

  void set_dm_token(const std::string& dm_token) { dm_token_ = dm_token; }
  void set_client_id(const std::string& client_id) { client_id_ = client_id; }
  void set_account(const CoreAccountId& account_id, const std::string& email) {
    account_id_ = account_id;
    email_ = email;
  }
  void add_user_affiliation_id(const std::string& id) {
    user_affiliation_ids_.push_back(id);
  }
  void set_is_hanging(bool is_hanging) { is_hanging_ = is_hanging; }

  // policy::UserPolicySigninService:
  void RegisterForPolicyWithAccountId(
      const std::string& username,
      const CoreAccountId& account_id,
      PolicyRegistrationCallback callback) override {
    EXPECT_EQ(email_, username);
    EXPECT_EQ(account_id_, account_id);
    if (!is_hanging_) {
      std::move(callback).Run(dm_token_, client_id_,
                              /*user_affiliation_ids=*/user_affiliation_ids_);
    }
  }

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      const std::vector<std::string>& user_affiliation_ids,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      PolicyFetchCallback callback) override {
    EXPECT_EQ(1u, user_affiliation_ids.size());
    EXPECT_EQ(user_affiliation_ids_, user_affiliation_ids);
    if (!is_hanging_) {
      std::move(callback).Run(true);
    }
  }

 private:
  std::string dm_token_;
  std::string client_id_;
  std::vector<std::string> user_affiliation_ids_;

  CoreAccountId account_id_;
  std::string email_;
  bool is_hanging_ = false;
};

class FakePolicyService : public testing::NiceMock<policy::MockPolicyService> {
 public:
  void SimulateCloudPolicyUpdate() {
    ASSERT_TRUE(observer_);
    // `provider` must match `UserCloudPolicyManager` which is nullptr in tests.
    observer_->OnProviderUpdatePropagated(/*provider=*/nullptr);
  }

  // policy::MockPolicyService:
  void AddProviderUpdateObserver(ProviderUpdateObserver* observer) override {
    ASSERT_FALSE(observer_);
    observer_ = observer;
  }
  void RemoveProviderUpdateObserver(ProviderUpdateObserver* observer) override {
    ASSERT_EQ(observer, observer_);
    observer_ = nullptr;
  }

 private:
  raw_ptr<policy::PolicyService::ProviderUpdateObserver> observer_ = nullptr;
};

std::unique_ptr<KeyedService> BuildMockSyncService(
    content::BrowserContext* context) {
  auto service = std::make_unique<testing::NiceMock<syncer::MockSyncService>>();
  ON_CALL(*service, HasSyncConsent()).WillByDefault(Return(true));
  return service;
}

class MockSigninManager : public SigninManager {
 public:
  class Handle : public AccountSelectionInProgressHandle {
   public:
    explicit Handle(MockSigninManager* signin_manager)
        : signin_manager_(signin_manager) {
      ++signin_manager_->handle_creation_count_;
    }
    ~Handle() override { ++signin_manager_->handle_deletion_count_; }

   private:
    raw_ptr<MockSigninManager> signin_manager_;
  };

  explicit MockSigninManager(Profile* profile)
      : SigninManager(*profile->GetPrefs(),
                      *IdentityManagerFactory::GetForProfile(profile),
                      *ChromeSigninClientFactory::GetForProfile(profile)) {}
  ~MockSigninManager() override = default;

  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto signin_manager =
        std::make_unique<testing::NiceMock<MockSigninManager>>(profile);
    ON_CALL(*signin_manager, CreateAccountSelectionInProgressHandle())
        .WillByDefault(
            Invoke(signin_manager.get(), &MockSigninManager::MakeHandle));
    return signin_manager;
  }

  int handle_creation_count() { return handle_creation_count_; }
  int handle_deletion_count() { return handle_deletion_count_; }

  MOCK_METHOD(std::unique_ptr<AccountSelectionInProgressHandle>,
              CreateAccountSelectionInProgressHandle,
              (),
              (override));

 private:
  int handle_creation_count_ = 0;
  int handle_deletion_count_ = 0;

  std::unique_ptr<AccountSelectionInProgressHandle> MakeHandle() {
    return std::make_unique<Handle>(this);
  }
};

// Helper to obtain a `base::OnceClosure` that allows checking if it did run and
// that will not cause issues if it is run while the originating instance goes
// out of scope.
class WeakClosure {
 public:
  WeakClosure() = default;
  ~WeakClosure() = default;
  void Run() { did_run_ = true; }

  base::OnceClosure Get() {
    return base::BindOnce(&WeakClosure::Run, weak_ptr_factory_.GetWeakPtr());
  }

  bool did_run() const { return did_run_; }

 private:
  bool did_run_ = false;

  base::WeakPtrFactory<WeakClosure> weak_ptr_factory_{this};
};

}  // namespace

class TurnSyncOnHelperTest : public testing::Test {
 public:
  TurnSyncOnHelperTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    const base::FilePath temp_user_data_dir =
        base::CreateUniqueTempDirectoryScopedToTest();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        std::make_unique<UnittestProfileManager>(
            temp_user_data_dir,
            base::BindRepeating(&TurnSyncOnHelperTest::BuildTestingProfile,
                                base::Unretained(this))));
    auto testing_profile = BuildTestingProfile(
        temp_user_data_dir.Append(FILE_PATH_LITERAL("profile")),
        /*delegate=*/nullptr, Profile::CreateMode::kSynchronous);
    profile_ = testing_profile.get();

    // Makes sure the profile has profile storage entries.
    profile_manager()->RegisterTestingProfile(std::move(testing_profile),
                                              /*add_to_storage=*/true);
    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    account_id_ = identity_test_env()->MakeAccountAvailable(kEmail).account_id;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Lacros expects that the main profile always exists.
    auto main_profile = BuildTestingProfile(
        temp_user_data_dir.Append(FILE_PATH_LITERAL("Default")),
        /*delegate=*/nullptr, Profile::CreateMode::kSynchronous);
    profile_manager()->RegisterTestingProfile(std::move(main_profile),
                                              /*add_to_storage=*/true);

    // Configure a mock account manager facade to return two accounts (`kEmail`
    // and `kEnterpriseEmail`) with no error.
    ON_CALL(mock_account_manager_facade_, GetAccounts(testing::_))
        .WillByDefault(
            [](base::OnceCallback<void(
                   const std::vector<account_manager::Account>&)> callback) {
              account_manager::AccountKey key(
                  signin::GetTestGaiaIdForEmail(kEmail),
                  account_manager::AccountType::kGaia);
              account_manager::AccountKey enterprise_key(
                  signin::GetTestGaiaIdForEmail(kEnterpriseEmail),
                  account_manager::AccountType::kGaia);
              std::move(callback).Run({{key, kEmail},
                                       { enterprise_key,
                                         kEnterpriseEmail }});
            });
    ON_CALL(mock_account_manager_facade_, GetPersistentErrorForAccount)
        .WillByDefault(
            [](const account_manager::AccountKey&,
               base::OnceCallback<void(const GoogleServiceAuthError&)>
                   callback) {
              std::move(callback).Run(GoogleServiceAuthError::AuthErrorNone());
            });
    profile_manager()->SetAccountProfileMapper(
        std::make_unique<AccountProfileMapper>(
            &mock_account_manager_facade_,
            &profile_manager()->GetProfileAttributesStorage(),
            g_browser_process->local_state()));
#endif

    user_policy_signin_service_ = static_cast<FakeUserPolicySigninService*>(
        policy::UserPolicySigninServiceFactory::GetForProfile(profile()));
    user_policy_signin_service_->set_account(account_id_, kEmail);
    EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id_));
    initial_device_id_ = GetSigninScopedDeviceIdForProfile(profile());
    EXPECT_FALSE(initial_device_id_.empty());

    ResetHistogramTester();
  }

  ~TurnSyncOnHelperTest() override {
    DCHECK_GT(delegate_destroyed_, 0);
    ClearProfile();
    base::RunLoop().RunUntilIdle();
  }

  // Basic accessors.
  Profile* profile() { return profile_; }
  Profile* new_profile() { return new_profile_; }
  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }
  signin::IdentityManager* identity_manager() {
    return identity_test_env()->identity_manager();
  }
  const CoreAccountId& account_id() { return account_id_; }
  FakeUserPolicySigninService* user_policy_signin_service() {
    return user_policy_signin_service_;
  }
  FakePolicyService* policy_service(Profile* profile = nullptr) {
    if (!profile) {
      profile = profile_;
    }
    return static_cast<FakePolicyService*>(
        profile->GetProfilePolicyConnector()->policy_service());
  }
  const std::string initial_device_id() { return initial_device_id_; }
  int delegate_destroyed() const { return delegate_destroyed_; }
  std::string enterprise_confirmation_email() const {
    return enterprise_confirmation_email_;
  }

  UnittestProfileManager* profile_manager() {
    return static_cast<UnittestProfileManager*>(
        TestingBrowserProcess::GetGlobal()->profile_manager());
  }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  // Builds a testing profile with the right setup for this test.
  std::unique_ptr<TestingProfile> BuildTestingProfile(
      const base::FilePath& path,
      Profile::Delegate* delegate,
      Profile::CreateMode create_mode) {
    TestingProfile::Builder profile_builder;

    AddTestingProfileFactories(profile_builder);

    profile_builder.SetDelegate(delegate);
    profile_builder.SetCreateMode(create_mode);
    profile_builder.SetPath(path);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_builder.SetIsMainProfile(Profile::IsMainProfilePath(path));
#endif
    profile_builder.SetPolicyService(std::make_unique<FakePolicyService>());

    return IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment(profile_builder);
  }

  virtual void AddTestingProfileFactories(
      TestingProfile::Builder& profile_builder) {
    profile_builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockSyncService));
    profile_builder.AddTestingFactory(
        policy::UserPolicySigninServiceFactory::GetInstance(),
        base::BindRepeating(&FakeUserPolicySigninService::Build));
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  }

  void ClearProfile() {
    identity_test_env_profile_adaptor_.reset();
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    profile_ = nullptr;
    user_policy_signin_service_ = nullptr;
  }

  syncer::MockSyncService* GetMockSyncService() {
    return GetMockSyncService(profile());
  }

  syncer::MockSyncService* GetMockSyncService(Profile* profile) {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile));
  }

  // Creates a `TurnSyncOnHelper` with the provided `mode`.
  //
  // Flow completion will be tracked internally, and waiting until the
  // flow is completed can be done by calling `WaitUntilFlowCompletion()`.
  // If we use this method to create more than one `TurnSyncOnHelper`,
  // `WaitUntilFlowCompletion()` will only wait until the first completion among
  // all of the created helpers.
  TurnSyncOnHelper* CreateTurnOnSyncHelper(
      TurnSyncOnHelper::SigninAbortedMode mode) {
    WeakClosure weak_closure;

    auto* helper = new TurnSyncOnHelper(
        profile(), kAccessPoint, kSigninPromoAction, account_id_, mode,
        std::make_unique<TestTurnSyncOnHelperDelegate>(this),
        weak_closure.Get().Then(flow_completion_loop_.QuitClosure()));

    // In no circumstance should the flow complete synchronously. It can cause
    // some crashes, see https://crbug.com/1367078.
    EXPECT_FALSE(weak_closure.did_run());

    return helper;
  }

  void WaitUntilFlowCompletion() { flow_completion_loop_.Run(); }

  void UseEnterpriseAccount() {
    CoreAccountInfo core_account_info =
        identity_test_env()->MakeAccountAvailable(kEnterpriseEmail);
    account_id_ = core_account_info.account_id;
    user_policy_signin_service_->set_account(account_id_, kEnterpriseEmail);

    // Update the account info to have a consistent hosted domain field.
    AccountInfo account_info =
        identity_manager()->FindExtendedAccountInfo(core_account_info);
    EXPECT_FALSE(account_info.IsEmpty());
    account_info.hosted_domain = kEnterpriseHostedDomain;
    signin::UpdateAccountInfoForAccount(identity_manager(), account_info);
  }

  void UseInvalidAccount() {
    account_id_ = CoreAccountId::FromGaiaId("invalid_account_gaia_id");
  }

  void SetExpectationsForSyncStartupCompleted(Profile* profile) {
    syncer::MockSyncService* mock_sync_service = GetMockSyncService(profile);
    EXPECT_CALL(*mock_sync_service, GetSetupInProgressHandle());
    ON_CALL(*mock_sync_service, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
    ON_CALL(*mock_sync_service, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  }

  void SetExpectationsForSyncStartupCompletedForNextProfileCreated() {
    profile_manager()->NextProfileCreatedCallback(base::BindOnce(
        &TurnSyncOnHelperTest::SetExpectationsForSyncStartupCompleted,
        base::Unretained(this)));
  }

  void SetExpectationsForSyncStartupPending(Profile* profile) {
    syncer::MockSyncService* mock_sync_service = GetMockSyncService(profile);
    EXPECT_CALL(*mock_sync_service, GetSetupInProgressHandle());
    ON_CALL(*mock_sync_service, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DisableReasonSet()));
    ON_CALL(*mock_sync_service, GetTransportState())
        .WillByDefault(
            Return(syncer::SyncService::TransportState::INITIALIZING));
  }

  void SetExpectationsForSyncDisabled(Profile* profile) {
    syncer::MockSyncService* mock_sync_service = GetMockSyncService(profile);
    ON_CALL(*mock_sync_service, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DisableReasonSet(
            {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY})));
  }

  void SetExpectationsForSyncAborted() {
    EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
                SetInitialSyncFeatureSetupComplete(
                    syncer::SyncFirstSetupCompleteSource::BASIC_FLOW))
        .Times(0);
  }

  void CheckSyncAborted(bool kept_account, int destroyed_delegate_count = 1) {
    EXPECT_FALSE(
        identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
    EXPECT_EQ(kept_account,
              identity_manager()->HasPrimaryAccountWithRefreshToken(
                  signin::ConsentLevel::kSignin));
    EXPECT_EQ(delegate_destroyed_, destroyed_delegate_count);
  }

  void CheckDelegateCalls() {
    EXPECT_EQ(expected_login_error_, login_error_);
    EXPECT_EQ(expected_merge_data_previous_email_, merge_data_previous_email_);
    EXPECT_EQ(expected_merge_data_new_email_, merge_data_new_email_);
    EXPECT_EQ(expected_enterprise_confirmation_email_,
              enterprise_confirmation_email_);
    EXPECT_EQ(expected_switched_to_new_profile_, switched_to_new_profile_);
    EXPECT_EQ(expected_sync_confirmation_shown_, sync_confirmation_shown_);
    EXPECT_EQ(expected_sync_disabled_confirmation_,
              sync_disabled_confirmation_);
    EXPECT_EQ(expected_sync_settings_shown_, sync_settings_shown_);
  }

  void CheckSigninMetrics(ExpectedMetricsState expected) {
    using base::Bucket;
    using testing::ElementsAreArray;

    std::vector<Bucket> expected_sign_in_buckets;
    if (expected.sign_in_access_point.has_value()) {
      expected_sign_in_buckets.emplace_back(*expected.sign_in_access_point, 1);
    }
    EXPECT_THAT(histogram_tester_->GetAllSamples("Signin.SignIn.Completed"),
                ElementsAreArray(expected_sign_in_buckets));

    EXPECT_THAT(
        histogram_tester_->GetAllSamples("Signin.SigninCompletedAccessPoint"),
        BucketsAre(Bucket(kAccessPoint, expected.sign_in_recorded)));

    EXPECT_THAT(histogram_tester_->GetAllSamples("Signin.SyncOptIn.Completed"),
                BucketsAre(Bucket(kAccessPoint,
                                  expected.sync_opt_in_completed ? 1 : 0)));

    EXPECT_THAT(
        histogram_tester_->GetAllSamples("Signin.SyncOptIn.OpenedSyncSettings"),
        BucketsAre(
            Bucket(kAccessPoint, expected.sync_settings_opened ? 1 : 0)));

    if (expected.profile_signout) {
      EXPECT_THAT(histogram_tester_->GetAllSamples("Signin.SignOut.Completed"),
                  BucketsAre(Bucket(expected.profile_signout.value(), 1)));
    } else {
      histogram_tester_->ExpectTotalCount("Signin.SignOut.Completed", 0);
    }

    EXPECT_THAT(
        histogram_tester_->GetAllSamples("Signin.SyncTurnOff.Completed"),
        BucketsAre(Bucket(signin_metrics::ProfileSignout::kTest,
                          expected.sync_turn_off ? 1 : 0)));

    histogram_tester_->ExpectUniqueTimeSample(
        "Signin.SyncOptIn.PreSyncConfirmationLatency", base::Milliseconds(0),
        expected.sync_opt_in_started ? 1 : 0);

    // Reset the tester so that these histograms don't need to be taken into
    // account for future verifications.
    ResetHistogramTester();
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Functions called by the TurnSyncOnHelper::Delegate:
  void OnShowLoginError(const SigninUIError& error) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(error.IsOk());
    EXPECT_FALSE(login_error_.has_value())
        << "Login error should be shown only once.";
    login_error_ = error;
  }

  void OnShowMergeSyncDataConfirmation(const std::string& previous_email,
                                       const std::string& new_email,
                                       signin::SigninChoiceCallback callback) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(previous_email.empty());
    EXPECT_FALSE(new_email.empty());
    EXPECT_TRUE(merge_data_previous_email_.empty())
        << "Merge data confirmation should be shown only once";
    EXPECT_TRUE(merge_data_new_email_.empty())
        << "Merge data confirmation should be shown only once";
    merge_data_previous_email_ = previous_email;
    merge_data_new_email_ = new_email;
    if (run_delegate_callbacks_) {
      std::move(callback).Run(merge_data_choice_);
    }
  }

  void OnShowEnterpriseAccountConfirmation(
      const AccountInfo& account_info,
      signin::SigninChoiceCallback callback) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(account_info.email.empty());
    EXPECT_TRUE(enterprise_confirmation_email_.empty())
        << "Enterprise confirmation should be shown only once.";
    enterprise_confirmation_email_ = account_info.email;
    if (run_delegate_callbacks_) {
      std::move(callback).Run(enterprise_choice_);
    }
  }

  void OnShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) {
    EXPECT_FALSE(sync_confirmation_shown_)
        << "Sync confirmation should be shown only once.";
    sync_confirmation_shown_ = true;
    if (run_delegate_callbacks_) {
      std::move(callback).Run(sync_confirmation_result_);
    }
  }

  bool OnShouldAbortBeforeShowSyncDisabledConfirmation() {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_EQ(sync_disabled_confirmation_, kNotShown);
    if (abort_before_show_sync_disabled_confirmation_) {
      sync_disabled_confirmation_ = kAbortedBeforeShown;
    }
    return abort_before_show_sync_disabled_confirmation_;
  }

  void OnShowSyncDisabledConfirmation(
      bool is_managed_account,
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) {
    EXPECT_EQ(sync_disabled_confirmation_, kNotShown)
        << "Sync disabled confirmation should be shown only once or "
           "aborted "
           "without showing.";
    sync_disabled_confirmation_ =
        is_managed_account ? kShownManaged : kShownNonManaged;
    if (run_delegate_callbacks_) {
      std::move(callback).Run(sync_confirmation_result_);
    }
  }

  void OnShowSyncSettings() {
    EXPECT_TRUE(sync_confirmation_shown_)
        << "Must show sync confirmation first";
    EXPECT_FALSE(sync_settings_shown_);
    sync_settings_shown_ = true;
  }

  virtual void SwitchToProfile(Profile* new_profile) {
    EXPECT_TRUE(new_profile);
    EXPECT_NE(profile(), new_profile)
        << "new_profile should not be the existing profile";
    EXPECT_FALSE(switched_to_new_profile_)
        << "Flow should only be restarted once";

    auto* new_identity_manager =
        IdentityManagerFactory::GetForProfile(new_profile);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // On Lacros, the `FakeProfileOAuth2TokenService` is used, and it is not
    // connected to the `AccountProfileMapper`. As a result, it does not pick up
    // the new account automatically, and the `SigninManager` is not triggered
    // to set the primary account. The test only checks that the account mapping
    // was updated correctly.
    CoreAccountInfo core_account_info =
        identity_manager()->FindExtendedAccountInfoByAccountId(account_id());
    EXPECT_TRUE(profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(new_profile->GetPath())
                    ->GetGaiaIds()
                    .contains(core_account_info.gaia));
    EXPECT_FALSE(profile_manager()
                     ->GetProfileAttributesStorage()
                     .GetProfileAttributesWithPath(profile()->GetPath())
                     ->GetGaiaIds()
                     .contains(core_account_info.gaia));
    // Simulate the token service removing the account in the source profile.
    signin::RemoveRefreshTokenForAccount(identity_manager(), account_id());
#else
    // The token has been transferred to the new token service, regardless of
    // SigninAbortedMode.
    EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id_));
#endif

    EXPECT_TRUE(new_identity_manager->HasAccountWithRefreshToken(account_id_));

    // TODO(crbug.com/40201807): Fix device ids on Lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    // The initial device ID is no longer used by any profile.
    EXPECT_NE(initial_device_id(),
              GetSigninScopedDeviceIdForProfile(profile()));
    EXPECT_NE(initial_device_id(),
              GetSigninScopedDeviceIdForProfile(new_profile));
#endif

    new_profile_ = new_profile;
    switched_to_new_profile_ = true;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // The `FakeProfileOAuth2TokenService` is not connected to the account
  // mapping. Set the account manually. The mapping is tested in
  // `SwitchToProfile()`.
  void SimulateAccountAddedToProfileLacros(Profile* profile) {
    auto* new_identity_manager = IdentityManagerFactory::GetForProfile(profile);
    CoreAccountInfo core_account_info =
        identity_manager()->FindExtendedAccountInfoByAccountId(account_id());
    signin::MakeAccountAvailable(new_identity_manager, core_account_info.email);
    signin::SetPrimaryAccount(new_identity_manager, core_account_info.email,
                              signin::ConsentLevel::kSignin);
  }
#endif

  void OnDelegateDestroyed() { ++delegate_destroyed_; }

 protected:
  // Type of sync disabled confirmation shown.
  enum SyncDisabledConfirmation {
    kNotShown,
    kAbortedBeforeShown,
    kShownManaged,
    kShownNonManaged
  };

  // Delegate behavior.
  signin::SigninChoice merge_data_choice_ = signin::SIGNIN_CHOICE_CANCEL;
  signin::SigninChoice enterprise_choice_ = signin::SIGNIN_CHOICE_CANCEL;
  LoginUIService::SyncConfirmationUIClosedResult sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC;
  bool abort_before_show_sync_disabled_confirmation_ = false;
  bool run_delegate_callbacks_ = true;

  // Expected delegate calls.
  std::optional<SigninUIError> expected_login_error_;
  std::string expected_enterprise_confirmation_email_;
  std::string expected_merge_data_previous_email_;
  std::string expected_merge_data_new_email_;
  bool expected_switched_to_new_profile_ = false;
  bool expected_sync_confirmation_shown_ = false;
  SyncDisabledConfirmation expected_sync_disabled_confirmation_ = kNotShown;
  bool expected_sync_settings_shown_ = false;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState local_state_;
  CoreAccountId account_id_;
  raw_ptr<TestingProfile, DanglingUntriaged> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  raw_ptr<FakeUserPolicySigninService, DanglingUntriaged>
      user_policy_signin_service_ = nullptr;
  std::string initial_device_id_;
  testing::NiceMock<syncer::SyncUserSettingsMock> mock_sync_settings_;

  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      browser_management_;
  std::unique_ptr<policy::ScopedManagementServiceOverrideForTesting>
      platform_management_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  testing::NiceMock<account_manager::MockAccountManagerFacade>
      mock_account_manager_facade_;
#endif

  // State of the delegate calls.
  int delegate_destroyed_ = 0;
  std::optional<SigninUIError> login_error_;
  std::string enterprise_confirmation_email_;
  std::string merge_data_previous_email_;
  std::string merge_data_new_email_;
  bool switched_to_new_profile_ = false;
  raw_ptr<Profile, DanglingUntriaged> new_profile_ = nullptr;
  bool sync_confirmation_shown_ = false;
  SyncDisabledConfirmation sync_disabled_confirmation_ = kNotShown;
  bool sync_settings_shown_ = false;
  base::RunLoop flow_completion_loop_;
};

class TurnSyncOnHelperWithMockSigninManagerTest : public TurnSyncOnHelperTest {
 public:
  void AddTestingProfileFactories(
      TestingProfile::Builder& profile_builder) override {
    TurnSyncOnHelperTest::AddTestingProfileFactories(profile_builder);

    profile_builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&MockSigninManager::Build));
  }

  MockSigninManager* GetMockSigninManager(Profile* profile) {
    return static_cast<MockSigninManager*>(
        SigninManagerFactory::GetForProfile(profile));
  }

  std::pair<int, int> GetSignInManagerHandleState() {
    auto* mock_signin_manager = GetMockSigninManager(profile());
    return {mock_signin_manager->handle_creation_count(),
            mock_signin_manager->handle_deletion_count()};
  }

  static std::optional<signin::ConsentLevel>
  GetExpectedPreSyncFlowConsentLevel() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // For the primary profile, there is always a primary account set by the
    // `SigninManager`.
    return signin::ConsentLevel::kSignin;
#else
    return std::nullopt;
#endif
  }
};

TestTurnSyncOnHelperDelegate::TestTurnSyncOnHelperDelegate(
    TurnSyncOnHelperTest* test_fixture)
    : test_fixture_(test_fixture) {}

TestTurnSyncOnHelperDelegate::~TestTurnSyncOnHelperDelegate() {
  test_fixture_->OnDelegateDestroyed();
}

void TestTurnSyncOnHelperDelegate::ShowLoginError(const SigninUIError& error) {
  test_fixture_->OnShowLoginError(error);
}

void TestTurnSyncOnHelperDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    signin::SigninChoiceCallback callback) {
  test_fixture_->OnShowMergeSyncDataConfirmation(previous_email, new_email,
                                                 std::move(callback));
}

void TestTurnSyncOnHelperDelegate::ShowEnterpriseAccountConfirmation(
    const AccountInfo& account_info,
    signin::SigninChoiceCallback callback) {
  test_fixture_->OnShowEnterpriseAccountConfirmation(account_info,
                                                     std::move(callback));
}

void TestTurnSyncOnHelperDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  test_fixture_->OnShowSyncConfirmation(std::move(callback));
}

bool TestTurnSyncOnHelperDelegate::
    ShouldAbortBeforeShowSyncDisabledConfirmation() {
  return test_fixture_->OnShouldAbortBeforeShowSyncDisabledConfirmation();
}

void TestTurnSyncOnHelperDelegate::ShowSyncDisabledConfirmation(
    bool is_managed_account,
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  test_fixture_->OnShowSyncDisabledConfirmation(is_managed_account,
                                                std::move(callback));
}

void TestTurnSyncOnHelperDelegate::ShowSyncSettings() {
  test_fixture_->OnShowSyncSettings();
}

void TestTurnSyncOnHelperDelegate::SwitchToProfile(Profile* new_profile) {
  test_fixture_->SwitchToProfile(new_profile);
}

// Check that the invalid account is supported.
TEST_F(TurnSyncOnHelperTest, InvalidAccount) {
  UseInvalidAccount();
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();
  CheckDelegateCalls();
  CheckSigninMetrics({});
}

class TurnSyncOnHelperTestWithSigninAllowedDisabled
    : public TurnSyncOnHelperTest {
 public:
  void SetUp() override {
    // In those tests, `prefs::kSigninAllowed` pref is set to false after the
    // Profile is created which will change the
    // `signin::AccountConsistencyMethod` in the
    // `AccountConsistencyModeManager`. This is not expected, so we disable the
    // browser signin from the command line prior to the creation of the
    // Profile.
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        "allow-browser-signin", "false");

    TurnSyncOnHelperTest::SetUp();
  }

 private:
  base::test::ScopedCommandLine scoped_command_line;
};

// Tests that the login error is displayed and that the account is kept.
TEST_F(TurnSyncOnHelperTestWithSigninAllowedDisabled,
       CanOfferSigninErrorKeepAccount) {
  // Set expectations.
  expected_login_error_ = SigninUIError::Other(kEmail);
  // Configure the test.
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  CheckSigninMetrics({});
}

// Tests that the login error is displayed and that the account is removed.
TEST_F(TurnSyncOnHelperTestWithSigninAllowedDisabled,
       CanOfferSigninErrorRemoveAccount) {
  // Set expectations.
  expected_login_error_ = SigninUIError::Other(kEmail);
  // Configure the test.
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  CheckSigninMetrics({});
}

// Tests that the sync disabled message is displayed and that the account is
// removed upon the ABORT_SYNC action.
TEST_F(TurnSyncOnHelperTest, SyncDisabledAbortRemoveAccount) {
  // Set expectations.
  expected_sync_disabled_confirmation_ = kShownNonManaged;
  SetExpectationsForSyncDisabled(profile());
  // Configure the test.
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC;

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  CheckSyncAborted(/*kept_account=*/false);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .profile_signout = signin_metrics::ProfileSignout::
                          kCancelSyncConfirmationRemoveAccount});
}

// Tests that the sync disabled message is displayed and that the account is
// removed upon the ABORT_SYNC action (despite SigninAbortedMode::KEEP_ACCOUNT).
TEST_F(TurnSyncOnHelperTest, SyncDisabledAbortKeepAccount) {
  // Set expectations.
  expected_sync_disabled_confirmation_ = kShownNonManaged;
  SetExpectationsForSyncDisabled(profile());
  // Configure the test.
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC;

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  CheckSyncAborted(/*kept_account=*/false);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .profile_signout = signin_metrics::ProfileSignout::
                          kCancelSyncConfirmationRemoveAccount});
}

// Tests that the sync disabled message is displayed and that the account is
// kept upon the SYNC_WITH_DEFAULT_SETTINGS action.
TEST_F(TurnSyncOnHelperWithMockSigninManagerTest,
       SyncDisabledContinueKeepAccount) {
  // Set expectations.
  expected_sync_disabled_confirmation_ = kShownNonManaged;
  SetExpectationsForSyncDisabled(profile());
  // Configure the test.
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;

  // Signin flow.
  EXPECT_EQ(GetExpectedPreSyncFlowConsentLevel(),
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations. We deliberately upgrade to kSync when the user chooses
  // to keep the account, mostly to stick with the historical behavior.
  EXPECT_EQ(signin::ConsentLevel::kSync,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/1),
            GetSignInManagerHandleState());
  CheckSigninMetrics(
      {.sign_in_access_point =
           GetExpectedPreSyncFlowConsentLevel().has_value()
               ? std::nullopt
               : std::optional<signin_metrics::AccessPoint>(kAccessPoint),
       .sign_in_recorded = true,
       .sync_opt_in_started = true,
       .sync_opt_in_completed = true});
}

// Tests that the sync disabled message is displayed and that the account is
// kept upon the SYNC_WITH_DEFAULT_SETTINGS action.
TEST_F(TurnSyncOnHelperWithMockSigninManagerTest,
       SyncDisabledManagedContinueKeepAccount) {
  // Reset the account info to be an enterprise account.
  UseEnterpriseAccount();
  // Set expectations.
  expected_sync_disabled_confirmation_ = kShownManaged;
  SetExpectationsForSyncDisabled(profile());
  // Configure the test.
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;

  // Signin flow.
  EXPECT_EQ(GetExpectedPreSyncFlowConsentLevel(),
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations. We deliberately upgrade to kSync when the user chooses
  // to keep the account, mostly to stick with the historical behavior.
  EXPECT_EQ(signin::ConsentLevel::kSync,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/1),
            GetSignInManagerHandleState());
  CheckSigninMetrics(
      {.sign_in_access_point =
           GetExpectedPreSyncFlowConsentLevel().has_value()
               ? std::nullopt
               : std::optional<signin_metrics::AccessPoint>(kAccessPoint),
       .sign_in_recorded = true,
       .sync_opt_in_started = true,
       .sync_opt_in_completed = true});
}

// Tests that the sync aborted before displaying the sync disabled message and
// `SigninAbortedMode::REMOVE_ACCOUNT` is honored.
TEST_F(TurnSyncOnHelperTest, SyncDisabledAbortWithoutShowingUIRemoveAccount) {
  // Set expectations.
  expected_sync_disabled_confirmation_ = kAbortedBeforeShown;
  SetExpectationsForSyncDisabled(profile());
  // Configure the test.
  abort_before_show_sync_disabled_confirmation_ = true;

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  CheckSyncAborted(/*kept_account=*/false);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .profile_signout = signin_metrics::ProfileSignout::
                          kCancelSyncConfirmationRemoveAccount});
}

// Tests that the sync aborted before displaying the sync disabled message and
// `SigninAbortedMode::KEEP_ACCOUNT` is honored.
TEST_F(TurnSyncOnHelperTest, SyncDisabledAbortWithoutShowingUI_KeepAccount) {
  // Set expectations.
  expected_sync_disabled_confirmation_ = kAbortedBeforeShown;
  SetExpectationsForSyncDisabled(profile());
  // Configure the test.
  abort_before_show_sync_disabled_confirmation_ = true;

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  CheckSyncAborted(/*kept_account=*/true);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true});
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that the sync aborted before displaying the sync disabled message and
// there is no crash with a primary profile.
// Regression test for crbug.com/1367078.
TEST_F(TurnSyncOnHelperTest, SyncDisabledAbortWithoutShowingUI_PrimaryProfile) {
  profile()->AsTestingProfile()->SetIsMainProfile(true);
  profile()->GetProfilePolicyConnector()->OverrideIsManagedForTesting(true);

  // Set expectations.
  expected_sync_disabled_confirmation_ = kAbortedBeforeShown;
  expected_enterprise_confirmation_email_ = kEnterpriseEmail;
  SetExpectationsForSyncDisabled(profile());

  // Configure the test.
  UseEnterpriseAccount();
  enterprise_choice_ = signin::SIGNIN_CHOICE_CONTINUE;
  abort_before_show_sync_disabled_confirmation_ = true;

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  CheckSyncAborted(/*kept_account=*/true);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true});
}
#endif

// Aborts the flow after the cross account dialog.
TEST_F(TurnSyncOnHelperTest, CrossAccountAbort) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingUsername,
                                   kPreviousEmail);
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   kPreviousGaiaId);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  CheckSigninMetrics({});
}

// Aborts the flow after the cross account dialog.
TEST_F(TurnSyncOnHelperTest, CrossAccountAbortAlreadyManaged) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingUsername,
                                   kPreviousEmail);
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   kPreviousGaiaId);
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  CheckSigninMetrics({});
}

// Merge data after the cross account dialog.
TEST_F(TurnSyncOnHelperTest, CrossAccountContinue) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted(profile());
  // Configure the test.
  merge_data_choice_ = signin::SIGNIN_CHOICE_CONTINUE;
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingUsername,
                                   kPreviousEmail);
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   kPreviousGaiaId);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  CheckSyncAborted(/*kept_account=*/false);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .profile_signout = signin_metrics::ProfileSignout::
                          kCancelSyncConfirmationRemoveAccount});
}

// Merge data after the cross account dialog.
TEST_F(TurnSyncOnHelperTest, CrossAccountContinueAlreadyManaged) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted(profile());
  // Configure the test.
  merge_data_choice_ = signin::SIGNIN_CHOICE_CONTINUE;
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingUsername,
                                   kPreviousEmail);
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   kPreviousGaiaId);
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  EXPECT_EQ(0, delegate_destroyed());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, delegate_destroyed());

  policy_service()->SimulateCloudPolicyUpdate();
  // Check expectations.
  // This was already a signed-in and managed enterprise account so we keep the
  // user signed-in, overriding SigninAbortedMode::REMOVE_ACCOUNT.
  EXPECT_EQ(1, delegate_destroyed());
  CheckSyncAborted(/*kept_account=*/true);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true});
}

// Create a new profile after the cross account dialog and show the signin page.
TEST_F(TurnSyncOnHelperTest, CrossAccountNewProfile) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  expected_switched_to_new_profile_ = true;
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompletedForNextProfileCreated();
  // Configure the test.
  merge_data_choice_ = signin::SIGNIN_CHOICE_NEW_PROFILE;
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingUsername,
                                   kPreviousEmail);
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastSyncingGaiaId,
                                   kPreviousGaiaId);
  // Signin flow.
  ProfileWaiter profile_waiter;
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  Profile* created_profile = profile_waiter.WaitForProfileAdded();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SimulateAccountAddedToProfileLacros(created_profile);
#endif

  AccountRemovedWaiter account_removed_waiter(identity_manager(), account_id());
  account_removed_waiter.Wait();
  // Check expectations.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  // The token has been removed from the source profile even though
  // KEEP_ACCOUNT was used.
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  ProfileAttributesEntry* created_entry =
      profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(created_profile->GetPath());
  EXPECT_FALSE(created_entry->IsOmitted());
  EXPECT_FALSE(created_entry->IsEphemeral());
  CheckDelegateCalls();
  CheckSigninMetrics({
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      .sign_in_access_point =
          signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
#else
      .sign_in_access_point =
          switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
              ? signin_metrics::AccessPoint::
                    ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE
              : kAccessPoint,
#endif
      .sign_in_recorded = true,
      .sync_opt_in_started = true});
}

// Abort after the enterprise confirmation prompt.
TEST_F(TurnSyncOnHelperTest, EnterpriseConfirmationAbort) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  CheckSigninMetrics({});
}

// Continue after the enterprise confirmation prompt.
TEST_F(TurnSyncOnHelperTest, EnterpriseConfirmationContinue) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = signin::SIGNIN_CHOICE_CONTINUE;
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  EXPECT_EQ(0, delegate_destroyed());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, delegate_destroyed());

  policy_service()->SimulateCloudPolicyUpdate();
  EXPECT_EQ(1, delegate_destroyed());

  // Check expectations.
  // Account is kept if the user accespts account management.
  CheckSyncAborted(/*kept_account=*/true);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true});
}

// Continue with a new profile after the enterprise confirmation prompt.
TEST_F(TurnSyncOnHelperTest, EnterpriseConfirmationNewProfile) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_switched_to_new_profile_ = true;
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompletedForNextProfileCreated();
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = signin::SIGNIN_CHOICE_NEW_PROFILE;
  // Signin flow.
  ProfileWaiter profile_waiter;
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  Profile* created_profile = profile_waiter.WaitForProfileAdded();
  EXPECT_TRUE(created_profile);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SimulateAccountAddedToProfileLacros(created_profile);
#endif
  policy_service(created_profile)->SimulateCloudPolicyUpdate();

  AccountRemovedWaiter account_removed_waiter(identity_manager(), account_id());
  account_removed_waiter.Wait();
  // Check expectations.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  CheckSigninMetrics({
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      .sign_in_access_point =
          signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
#else
      .sign_in_access_point =
          switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
              ? signin_metrics::AccessPoint::
                    ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE
              : kAccessPoint,
#endif
      .sign_in_recorded = true,
      .sync_opt_in_started = true});
}

// Wait for cloud policy to be merged before showing sync confirmation.
TEST_F(TurnSyncOnHelperTest, LoadPolicyBeforeShowingSyncConfirmation) {
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();

  // Sync confirmation is awaiting a policy update.
  CheckDelegateCalls();

  policy_service()->SimulateCloudPolicyUpdate();
  expected_sync_confirmation_shown_ = true;
  CheckDelegateCalls();
}

// Cloud policy update times out.
TEST_F(TurnSyncOnHelperTest, LoadPolicyBeforeShowingSyncConfirmation_Timeout) {
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_util::SetUserAcceptedAccountManagement(profile(), true);
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Sync confirmation is awaiting a policy update.
  CheckDelegateCalls();

  // `timeout_delta` must be bigger than `kPolicyUpdateTimeout`.
  base::TimeDelta timeout_delta = base::Seconds(4);
  task_environment()->FastForwardBy(timeout_delta);
  expected_sync_confirmation_shown_ = true;
  CheckDelegateCalls();
}

// Test that the unconsented primary account is kept if the user creates a new
// profile and cancels sync activation.
TEST_F(TurnSyncOnHelperTest, SignedInAccountUndoSyncKeepAccount) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEnterpriseEmail;
  expected_switched_to_new_profile_ = true;
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC;
  SetExpectationsForSyncStartupCompletedForNextProfileCreated();
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = signin::SIGNIN_CHOICE_NEW_PROFILE;
  UseEnterpriseAccount();
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id(), signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);

  CheckSigninMetrics(
      {.sign_in_access_point =
           signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN});

  // Signin flow.
  ProfileWaiter profile_waiter;
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  Profile* created_profile = profile_waiter.WaitForProfileAdded();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SimulateAccountAddedToProfileLacros(created_profile);
#endif
  policy_service(created_profile)->SimulateCloudPolicyUpdate();

  // The account is removed from the source profile.
  AccountRemovedWaiter account_removed_waiter(identity_manager(), account_id());
  account_removed_waiter.Wait();

  // Check expectations.
  ASSERT_TRUE(new_profile());
  EXPECT_EQ(created_profile, new_profile());
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_FALSE(identity_manager()->HasAccountWithRefreshToken(account_id()));

  auto* new_identity_manager =
      IdentityManagerFactory::GetForProfile(created_profile);
  DCHECK_NE(new_identity_manager, identity_manager());
  EXPECT_EQ(account_id(), new_identity_manager->GetPrimaryAccountId(
                              signin::ConsentLevel::kSignin));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(new_identity_manager));
  CheckDelegateCalls();
  CheckSigninMetrics({
#if BUILDFLAG(IS_CHROMEOS_LACROS)
      .sign_in_access_point =
          signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS,
#else
      .sign_in_access_point =
          switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
              ? signin_metrics::AccessPoint::
                    ACCESS_POINT_SIGNIN_INTERCEPT_FIRST_RUN_EXPERIENCE
              : kAccessPoint,
#endif
      .sign_in_recorded = true,
      .sync_opt_in_started = true,
      .profile_signout =
          switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
              ? std::optional<signin_metrics::ProfileSignout>(
                    signin_metrics::ProfileSignout::kMovePrimaryAccount)
              : std::nullopt});
}

TEST_F(TurnSyncOnHelperTest, SearchEngineImportedToNewProfile) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEnterpriseEmail;
  expected_switched_to_new_profile_ = true;
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC;
  SetExpectationsForSyncStartupCompletedForNextProfileCreated();
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = signin::SIGNIN_CHOICE_NEW_PROFILE;
  UseEnterpriseAccount();
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id(), signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);

  CheckSigninMetrics(
      {.sign_in_access_point =
           signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN});

  // Set some search engine in the source profile.
  const char kCustomSearchEngineDomain[] = "bar.com";
  int64_t search_engine_choice_timestamp =
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds();
  const char kChoiceVersion[] = "1.2.3.4";
  PrefService* pref_service = profile()->GetPrefs();
  pref_service->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      search_engine_choice_timestamp);
  pref_service->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      kChoiceVersion);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetShortName(base::UTF8ToUTF16(std::string(kCustomSearchEngineDomain)));
  data.SetKeyword(base::UTF8ToUTF16(std::string(kCustomSearchEngineDomain)));
  data.SetURL("https://" + std::string(kCustomSearchEngineDomain) +
              "url?bar={searchTerms}");
  data.new_tab_url =
      "https://" + std::string(kCustomSearchEngineDomain) + "newtab";
  data.alternate_urls.push_back("https://" +
                                std::string(kCustomSearchEngineDomain) +
                                "alt#quux={searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // Signin flow.
  ProfileWaiter profile_waiter;
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  Profile* created_profile = profile_waiter.WaitForProfileAdded();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  SimulateAccountAddedToProfileLacros(created_profile);
#endif
  policy_service(created_profile)->SimulateCloudPolicyUpdate();
  // The account is removed from the source profile.
  AccountRemovedWaiter account_removed_waiter(identity_manager(), account_id());
  account_removed_waiter.Wait();
  CheckDelegateCalls();

  // Check that the search engine was imported in the created profile.
  PrefService* new_pref_service = created_profile->GetPrefs();
  EXPECT_EQ(new_pref_service->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
            search_engine_choice_timestamp);
  EXPECT_EQ(new_pref_service->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            kChoiceVersion);

  TemplateURLService* new_template_url_service =
      TemplateURLServiceFactory::GetForProfile(created_profile);
  EXPECT_EQ(new_template_url_service->GetDefaultSearchProvider()->short_name(),
            base::UTF8ToUTF16(std::string(kCustomSearchEngineDomain)));
}

// Test that the unconsented primary account is kept if the user  cancels sync
// activation in a managed profile.
TEST_F(TurnSyncOnHelperTest, SignedInAccountUndoSyncRemoveAccount) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEnterpriseEmail;
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SYNC;
  SetExpectationsForSyncStartupCompleted(profile());
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = signin::SIGNIN_CHOICE_CONTINUE;
  UseEnterpriseAccount();
  identity_manager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id(), signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);
  CheckSigninMetrics(
      {.sign_in_access_point =
           signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN});

  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  policy_service()->SimulateCloudPolicyUpdate();
  // This was already a signed-in and managed enterprise account so we keep the
  // user signed-in, overriding SigninAbortedMode::REMOVE_ACCOUNT.
  CheckSyncAborted(/*kept_account=*/true);
  CheckDelegateCalls();

  CheckSigninMetrics({.sign_in_recorded = true, .sync_opt_in_started = true});
}

// Tests that the sync confirmation is shown and the user can abort.
TEST_F(TurnSyncOnHelperTest, UndoSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted(profile());
  SetExpectationsForSyncAborted();

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Check expectations.
  CheckSyncAborted(/*kept_account=*/false);
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .profile_signout = signin_metrics::ProfileSignout::
                          kCancelSyncConfirmationRemoveAccount});
}

// Tests that the sync settings page is shown.
TEST_F(TurnSyncOnHelperTest, ConfigureSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  expected_sync_settings_shown_ = true;
  SetExpectationsForSyncStartupCompleted(profile());
  EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::BASIC_FLOW))
      .Times(0);

  // Configure the test.
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::CONFIGURE_SYNC_FIRST;
  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .sync_opt_in_completed = true,
                      .sync_settings_opened = true});
}

// Tests that the user is signed in and Sync configuration is complete.
TEST_F(TurnSyncOnHelperTest, StartSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted(profile());
  EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  // Configure the test.
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSync));
  CheckDelegateCalls();
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .sync_opt_in_completed = true});
}

// Tests that the user is signed in and Sync configuration is complete.
// Also tests that turning sync on enables URL-keyed anonymized data collection.
// Regression test for http://crbug.com/812546
TEST_F(TurnSyncOnHelperTest, ShowSyncDialogForEndConsumerAccount) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  SetExpectationsForSyncStartupCompleted(profile());
  EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  PrefService* pref_service = profile()->GetPrefs();
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      url_keyed_collection_helper =
          unified_consent::UrlKeyedDataCollectionConsentHelper::
              NewAnonymizedDataCollectionConsentHelper(pref_service);
  EXPECT_FALSE(url_keyed_collection_helper->IsEnabled());

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  WaitUntilFlowCompletion();

  // Check expectations.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSync));
  CheckDelegateCalls();
  EXPECT_TRUE(url_keyed_collection_helper->IsEnabled());
}

// For users on a cloud managed device, tests that the user is signed in only
// after Sync engine starts.
// Regression test for http://crbug.com/812546
TEST_F(TurnSyncOnHelperWithMockSigninManagerTest,
       ShowSyncDialogBlockedUntilSyncStartupCompletedForCloudManagedDevices) {
  // Simulate a managed browser.
  policy::ScopedManagementServiceOverrideForTesting browser_management(
      policy::ManagementServiceFactory::GetForProfile(profile()),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending(profile());

  // Signin flow.
  EXPECT_EQ(GetExpectedPreSyncFlowConsentLevel(),
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  TurnSyncOnHelper* sync_starter = CreateTurnOnSyncHelper(
      TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();

  // Check that the primary account was set with IdentityManager, but the sync
  // confirmation dialog was not yet shown.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSignin));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/0),
            GetSignInManagerHandleState());

  // Simulate that sync startup has completed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  sync_starter->OnSyncStartupStateChanged(
      SyncStartupTracker::ServiceStartupState::kComplete);
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSync));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/1),
            GetSignInManagerHandleState());
}

// For enterprise user, tests that the user is signed in only after Sync engine
// starts.
// Regression test for http://crbug.com/812546
TEST_F(TurnSyncOnHelperWithMockSigninManagerTest,
       ShowSyncDialogBlockedUntilSyncStartupCompletedForEnterpriseAccount) {
  // Reset the account info to be an enterprise account.
  UseEnterpriseAccount();

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending(profile());

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  TurnSyncOnHelper* sync_starter = CreateTurnOnSyncHelper(
      TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();

  // Check that the primary account was set with IdentityManager, but the sync
  // confirmation dialog was not yet shown.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSignin));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/0),
            GetSignInManagerHandleState());

  // Simulate that sync startup has completed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  sync_starter->OnSyncStartupStateChanged(
      SyncStartupTracker::ServiceStartupState::kComplete);
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSync));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/1),
            GetSignInManagerHandleState());
}

// For enterprise user, tests that the user is signed in only after Sync engine
// fails to start.
// Regression test for http://crbug.com/812546
TEST_F(TurnSyncOnHelperWithMockSigninManagerTest,
       ShowSyncDialogBlockedUntilSyncStartupFailedForEnterpriseAccount) {
  // Reset the account info to be an enterprise account.
  UseEnterpriseAccount();

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending(profile());
  EXPECT_EQ(std::make_pair(/*creations=*/0, /*deletions=*/0),
            GetSignInManagerHandleState());

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  TurnSyncOnHelper* sync_starter = CreateTurnOnSyncHelper(
      TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();

  // Check that the primary account was added to the token service and in the
  // sign-in manager.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSignin));
  EXPECT_EQ(signin::ConsentLevel::kSignin,
            signin::GetPrimaryAccountConsentLevel(identity_manager()));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/0),
            GetSignInManagerHandleState());

  // Simulate that sync startup has failed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  sync_starter->OnSyncStartupStateChanged(
      SyncStartupTracker::ServiceStartupState::kError);
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSignin));
  CheckDelegateCalls();
  EXPECT_EQ(std::make_pair(/*creations=*/1, /*deletions=*/1),
            GetSignInManagerHandleState());
}

// For users on a cloud managed device, tests that the user is signed in only
// after Sync engine fails to start.
// Regression test for http://crbug.com/812546
TEST_F(TurnSyncOnHelperTest,
       ShowSyncDialogBlockedUntilSyncStartupFailedForCloudManagedDevices) {
  // Simulate a managed platform.
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending(profile());

  // Signin flow.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  TurnSyncOnHelper* sync_starter = CreateTurnOnSyncHelper(
      TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();

  // Check that the primary account was set with IdentityManager, but the sync
  // confirmation dialog was not yet shown.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSignin));
  CheckDelegateCalls();

  // Simulate that sync startup has failed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetMockSyncService()->GetMockUserSettings(),
              SetInitialSyncFeatureSetupComplete(
                  syncer::SyncFirstSetupCompleteSource::BASIC_FLOW));
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  sync_starter->OnSyncStartupStateChanged(
      SyncStartupTracker::ServiceStartupState::kError);
  EXPECT_EQ(account_id(), identity_manager()->GetPrimaryAccountId(
                              signin::ConsentLevel::kSync));
  CheckDelegateCalls();
}

// Checks that the profile can be deleted in the middle of the flow.
TEST_F(TurnSyncOnHelperTest, ProfileDeletion) {
  run_delegate_callbacks_ = false;  // Delegate is hanging.

  // Show the enterprise confirmation dialog.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = signin::SIGNIN_CHOICE_CONTINUE;
  // Signin flow.
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();

  // Delegate is now hanging at the enterprise confirmation dialog.
  // Dialog has been shown.
  EXPECT_EQ(kEmail, enterprise_confirmation_email());
  // But signin is not finished.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));

  // Delete the profile.
  ClearProfile();

  // TurnSyncOnHelper was destroyed.
  EXPECT_EQ(1, delegate_destroyed());
  CheckSigninMetrics({});
}

// Checks that an existing instance is deleted when a new one is created.
TEST_F(TurnSyncOnHelperTest, AbortExisting) {
  // Create a first instance, stuck on policy requests.
  user_policy_signin_service()->set_is_hanging(true);
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Check that it did not complete.
  EXPECT_FALSE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  CheckDelegateCalls();

  // Create a new helper and let it complete.
  user_policy_signin_service()->set_is_hanging(false);
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  SetExpectationsForSyncStartupCompleted(profile());
  CreateTurnOnSyncHelper(TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Check that it completed.
  CheckDelegateCalls();
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSync));
  // The token is still there, even though the first helper had REMOVE_ACCOUNT.
  EXPECT_TRUE(identity_manager()->HasAccountWithRefreshToken(account_id()));
  // Both delegates were destroyed.
  EXPECT_EQ(2, delegate_destroyed());
  CheckSigninMetrics({.sign_in_access_point = kAccessPoint,
                      .sign_in_recorded = true,
                      .sync_opt_in_started = true,
                      .sync_opt_in_completed = true});
}
