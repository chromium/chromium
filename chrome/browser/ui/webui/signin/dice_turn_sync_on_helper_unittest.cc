// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/browser/unified_consent/unified_consent_test_util.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/browser_sync/profile_sync_service_mock.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/signin/core/browser/signin_pref_names.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::ReturnRef;

class DiceTurnSyncOnHelperTestBase;

namespace {

const char kEmail[] = "foo@gmail.com";
const char kGaiaID[] = "foo_gaia_id";
const char kPreviousEmail[] = "notme@bar.com";
const char kEnterpriseEmail[] = "enterprise@managed.com";
const char kEnterpriseGaiaID[] = "enterprise_gaia_id";

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER;
const signin_metrics::PromoAction kSigninPromoAction =
    signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT;
const signin_metrics::Reason kSigninReason =
    signin_metrics::Reason::REASON_REAUTHENTICATION;

// Dummy delegate forwarding all the calls the test fixture.
// Owned by the DiceTurnOnSyncHelper.
class TestDiceTurnSyncOnHelperDelegate : public DiceTurnSyncOnHelper::Delegate {
 public:
  explicit TestDiceTurnSyncOnHelperDelegate(
      DiceTurnSyncOnHelperTestBase* test_fixture);
  ~TestDiceTurnSyncOnHelperDelegate() override;

 private:
  // DiceTurnSyncOnHelper::Delegate:
  void ShowLoginError(const std::string& email,
                      const std::string& error_message) override;
  void ShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) override;
  void ShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) override;
  void ShowSyncSettings() override;
  void ShowSigninPageInNewProfile(Profile* new_profile,
                                  const std::string& username) override;

  DiceTurnSyncOnHelperTestBase* test_fixture_;
};

// Simple ProfileManager creating testing profiles.
class UnittestProfileManager : public ProfileManagerWithoutInit {
 public:
  explicit UnittestProfileManager(const base::FilePath& user_data_dir)
      : ProfileManagerWithoutInit(user_data_dir) {}

 protected:
  Profile* CreateProfileHelper(const base::FilePath& file_path) override {
    if (!base::PathExists(file_path) && !base::CreateDirectory(file_path))
      return nullptr;
    return new TestingProfile(file_path, nullptr);
  }

  Profile* CreateProfileAsyncHelper(const base::FilePath& path,
                                    Delegate* delegate) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&base::CreateDirectory), path));
    return new TestingProfile(path, this);
  }
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
                              identity::IdentityManager* identity_manager)
      : UserPolicySigninService(profile,
                                nullptr,
                                nullptr,
                                nullptr,
                                identity_manager,
                                nullptr) {}

  void set_dm_token(const std::string& dm_token) { dm_token_ = dm_token; }
  void set_client_id(const std::string& client_id) { client_id_ = client_id; }
  void set_account(const std::string& account_id, const std::string& email) {
    account_id_ = account_id;
    email_ = email;
  }

  // policy::UserPolicySigninService:
  void RegisterForPolicyWithAccountId(
      const std::string& username,
      const std::string& account_id,
      const PolicyRegistrationCallback& callback) override {
    EXPECT_EQ(email_, username);
    EXPECT_EQ(account_id_, account_id);
    callback.Run(dm_token_, client_id_);
  }

  // policy::UserPolicySigninServiceBase:
  void FetchPolicyForSignedInUser(
      const AccountId& account_id,
      const std::string& dm_token,
      const std::string& client_id,
      scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory,
      const PolicyFetchCallback& callback) override {
    callback.Run(true);
  }

 private:
  std::string dm_token_;
  std::string client_id_;
  std::string account_id_;
  std::string email_;
};

}  // namespace

class DiceTurnSyncOnHelperTestBase : public testing::Test {
 public:
  DiceTurnSyncOnHelperTestBase()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new UnittestProfileManager(temp_dir_.GetPath()));

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildFakeProfileOAuth2TokenService));
    profile_builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(),
        base::BindRepeating(&BuildFakeSigninManagerForTesting));
    profile_builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    profile_builder.AddTestingFactory(
        ProfileSyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockProfileSyncService));
    profile_builder.AddTestingFactory(
        policy::UserPolicySigninServiceFactory::GetInstance(),
        base::BindRepeating(&FakeUserPolicySigninService::Build));
    profile_builder.AddTestingFactory(
        UnifiedConsentServiceFactory::GetInstance(),
        base::BindRepeating(&BuildUnifiedConsentServiceForTesting));
    profile_ = profile_builder.Build();
    account_tracker_service_ =
        AccountTrackerServiceFactory::GetForProfile(profile());
    account_id_ = account_tracker_service_->SeedAccountInfo(kGaiaID, kEmail);
    user_policy_signin_service_ = static_cast<FakeUserPolicySigninService*>(
        policy::UserPolicySigninServiceFactory::GetForProfile(profile()));
    user_policy_signin_service_->set_account(account_id_, kEmail);
    token_service_ = ProfileOAuth2TokenServiceFactory::GetForProfile(profile());
    token_service_->UpdateCredentials(account_id_, "refresh_token");
    signin_manager_ = SigninManagerFactory::GetForProfile(profile());
    EXPECT_TRUE(token_service_->RefreshTokenIsAvailable(account_id_));
  }

  ~DiceTurnSyncOnHelperTestBase() override {
    DCHECK(delegate_destroyed_);
    // Destroy extra profiles.
    TestingBrowserProcess::GetGlobal()->SetProfileManager(nullptr);
    base::RunLoop().RunUntilIdle();
  }

  // Basic accessors.
  Profile* profile() { return profile_.get(); }
  ProfileOAuth2TokenService* token_service() { return token_service_; }
  SigninManager* signin_manager() { return signin_manager_; }
  const std::string& account_id() { return account_id_; }
  FakeUserPolicySigninService* user_policy_signin_service() {
    return user_policy_signin_service_;
  }

  // Gets the ProfileSyncServiceMock.
  browser_sync::ProfileSyncServiceMock* GetProfileSyncServiceMock() {
    return static_cast<browser_sync::ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetForProfile(profile()));
  }

  DiceTurnSyncOnHelper* CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode mode) {
    return new DiceTurnSyncOnHelper(
        profile(), kAccessPoint, kSigninPromoAction, kSigninReason, account_id_,
        mode, std::make_unique<TestDiceTurnSyncOnHelperDelegate>(this));
  }

  void UseEnterpriseAccount() {
    account_id_ = account_tracker_service_->SeedAccountInfo(kEnterpriseGaiaID,
                                                            kEnterpriseEmail);
    user_policy_signin_service_->set_account(account_id_, kEnterpriseEmail);
    token_service_->UpdateCredentials(account_id_, "enterprise_refresh_token");
  }

  void UseInvalidAccount() { account_id_ = "invalid_account"; }

  void SetExpectationsForSyncStartupCompleted() {
    browser_sync::ProfileSyncServiceMock* sync_service_mock =
        GetProfileSyncServiceMock();
    EXPECT_CALL(*sync_service_mock, GetSetupInProgressHandle()).Times(1);
    ON_CALL(*sync_service_mock, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
    ON_CALL(*sync_service_mock, GetTransportState())
        .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));
  }

  void SetExpectationsForSyncStartupPending() {
    browser_sync::ProfileSyncServiceMock* sync_service_mock =
        GetProfileSyncServiceMock();
    EXPECT_CALL(*sync_service_mock, GetSetupInProgressHandle()).Times(1);
    ON_CALL(*sync_service_mock, GetDisableReasons())
        .WillByDefault(Return(syncer::SyncService::DISABLE_REASON_NONE));
    ON_CALL(*sync_service_mock, GetTransportState())
        .WillByDefault(
            Return(syncer::SyncService::TransportState::INITIALIZING));
    ON_CALL(*sync_service_mock, GetAuthError())
        .WillByDefault(ReturnRef(kNoAuthError));
  }

  void CheckDelegateCalls() {
    EXPECT_EQ(expected_login_error_email_, login_error_email_);
    EXPECT_EQ(expected_login_error_message_, login_error_message_);
    EXPECT_EQ(expected_merge_data_previous_email_, merge_data_previous_email_);
    EXPECT_EQ(expected_merge_data_new_email_, merge_data_new_email_);
    EXPECT_EQ(expected_enterprise_confirmation_email_,
              enterprise_confirmation_email_);
    EXPECT_EQ(expected_new_profile_username_, new_profile_username_);
    EXPECT_EQ(expected_sync_confirmation_shown_, sync_confirmation_shown_);
    EXPECT_EQ(expected_sync_settings_shown_, sync_settings_shown_);
  }

  // Functions called by the DiceTurnSyncOnHelper::Delegate:
  void OnShowLoginError(const std::string& email,
                        const std::string& error_message) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(email.empty());
    EXPECT_TRUE(login_error_email_.empty())
        << "Login error should be shown only once.";
    login_error_email_ = email;
    login_error_message_ = error_message;  // May be empty.
  }

  void OnShowMergeSyncDataConfirmation(
      const std::string& previous_email,
      const std::string& new_email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(previous_email.empty());
    EXPECT_FALSE(new_email.empty());
    EXPECT_TRUE(merge_data_previous_email_.empty())
        << "Merge data confirmation should be shown only once";
    EXPECT_TRUE(merge_data_new_email_.empty())
        << "Merge data confirmation should be shown only once";
    merge_data_previous_email_ = previous_email;
    merge_data_new_email_ = new_email;
    std::move(callback).Run(merge_data_choice_);
  }

  void OnShowEnterpriseAccountConfirmation(
      const std::string& email,
      DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
    EXPECT_FALSE(sync_confirmation_shown_);
    EXPECT_FALSE(email.empty());
    EXPECT_TRUE(enterprise_confirmation_email_.empty())
        << "Enterprise confirmation should be shown only once.";
    enterprise_confirmation_email_ = email;
    std::move(callback).Run(enterprise_choice_);
  }

  void OnShowSyncConfirmation(
      base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
          callback) {
    EXPECT_FALSE(sync_confirmation_shown_)
        << "Sync confirmation should be shown only once.";
    sync_confirmation_shown_ = true;
    std::move(callback).Run(sync_confirmation_result_);
  }

  void OnShowSyncSettings() {
    EXPECT_TRUE(sync_confirmation_shown_)
        << "Must show sync confirmation first";
    EXPECT_FALSE(sync_settings_shown_);
    sync_settings_shown_ = true;
  }

  void OnShowSigninPageInNewProfile(Profile* new_profile,
                                    const std::string& username) {
    EXPECT_TRUE(new_profile);
    EXPECT_NE(profile(), new_profile)
        << "new_profile should not be the existing profile";
    EXPECT_FALSE(username.empty());
    EXPECT_TRUE(new_profile_username_.empty())
        << "Signin page should be shown only once";
    new_profile_username_ = username;
  }

  void OnDelegateDestroyed() { delegate_destroyed_ = true; }

 protected:
  // Delegate behavior.
  DiceTurnSyncOnHelper::SigninChoice merge_data_choice_ =
      DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL;
  DiceTurnSyncOnHelper::SigninChoice enterprise_choice_ =
      DiceTurnSyncOnHelper::SIGNIN_CHOICE_CANCEL;
  LoginUIService::SyncConfirmationUIClosedResult sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::ABORT_SIGNIN;

  // Expected delegate calls.
  std::string expected_login_error_email_;
  std::string expected_login_error_message_;
  std::string expected_enterprise_confirmation_email_;
  std::string expected_merge_data_previous_email_;
  std::string expected_merge_data_new_email_;
  std::string expected_new_profile_username_;
  bool expected_sync_confirmation_shown_ = false;
  bool expected_sync_settings_shown_ = false;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState local_state_;
  std::string account_id_;
  std::unique_ptr<TestingProfile> profile_;
  AccountTrackerService* account_tracker_service_ = nullptr;
  ProfileOAuth2TokenService* token_service_ = nullptr;
  SigninManager* signin_manager_ = nullptr;
  FakeUserPolicySigninService* user_policy_signin_service_ = nullptr;

  // State of the delegate calls.
  bool delegate_destroyed_ = false;
  std::string login_error_email_;
  std::string login_error_message_;
  std::string enterprise_confirmation_email_;
  std::string merge_data_previous_email_;
  std::string merge_data_new_email_;
  std::string new_profile_username_;
  bool sync_confirmation_shown_ = false;
  bool sync_settings_shown_ = false;

  // Note: This needs to be a member variable for testing::ReturnRef.
  const GoogleServiceAuthError kNoAuthError =
      GoogleServiceAuthError::AuthErrorNone();
};

// Test class with only DiceMigration enabled.
class DiceTurnSyncOnHelperTest : public DiceTurnSyncOnHelperTestBase {
 public:
  DiceTurnSyncOnHelperTest() = default;

 private:
  ScopedAccountConsistencyDiceMigration scoped_dice_;
};

// Test class with Dice and UnifiedConsent enabled.
class DiceTurnSyncOnHelperTestWithUnifiedConsent
    : public DiceTurnSyncOnHelperTestBase {
 public:
  DiceTurnSyncOnHelperTestWithUnifiedConsent()
      : scoped_unified_consent_(
            unified_consent::UnifiedConsentFeatureState::kEnabledNoBump) {}
  ~DiceTurnSyncOnHelperTestWithUnifiedConsent() override {}

 private:
  ScopedAccountConsistencyDice scoped_dice_;
  unified_consent::ScopedUnifiedConsent scoped_unified_consent_;
};

// TestDiceTurnSyncOnHelperDelegate implementation.

TestDiceTurnSyncOnHelperDelegate::TestDiceTurnSyncOnHelperDelegate(
    DiceTurnSyncOnHelperTestBase* test_fixture)
    : test_fixture_(test_fixture) {}

TestDiceTurnSyncOnHelperDelegate::~TestDiceTurnSyncOnHelperDelegate() {
  test_fixture_->OnDelegateDestroyed();
}

void TestDiceTurnSyncOnHelperDelegate::ShowLoginError(
    const std::string& email,
    const std::string& error_message) {
  test_fixture_->OnShowLoginError(email, error_message);
}

void TestDiceTurnSyncOnHelperDelegate::ShowMergeSyncDataConfirmation(
    const std::string& previous_email,
    const std::string& new_email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  test_fixture_->OnShowMergeSyncDataConfirmation(previous_email, new_email,
                                                 std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowEnterpriseAccountConfirmation(
    const std::string& email,
    DiceTurnSyncOnHelper::SigninChoiceCallback callback) {
  test_fixture_->OnShowEnterpriseAccountConfirmation(email,
                                                     std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowSyncConfirmation(
    base::OnceCallback<void(LoginUIService::SyncConfirmationUIClosedResult)>
        callback) {
  test_fixture_->OnShowSyncConfirmation(std::move(callback));
}

void TestDiceTurnSyncOnHelperDelegate::ShowSyncSettings() {
  test_fixture_->OnShowSyncSettings();
}

void TestDiceTurnSyncOnHelperDelegate::ShowSigninPageInNewProfile(
    Profile* new_profile,
    const std::string& username) {
  test_fixture_->OnShowSigninPageInNewProfile(new_profile, username);
}

// Check that the invalid account is supported.
TEST_F(DiceTurnSyncOnHelperTest, InvalidAccount) {
  UseInvalidAccount();
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  CheckDelegateCalls();
}

// Tests that the login error is displayed and that the account is kept.
TEST_F(DiceTurnSyncOnHelperTest, CanOfferSigninErrorKeepAccount) {
  // Set expectations.
  expected_login_error_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Check expectations.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Tests that the login error is displayed and that the account is removed.
TEST_F(DiceTurnSyncOnHelperTest, CanOfferSigninErrorRemoveAccount) {
  // Set expectations.
  expected_login_error_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  base::RunLoop().RunUntilIdle();
  // Check expectations.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Aborts the flow after the cross account dialog.
TEST_F(DiceTurnSyncOnHelperTest, CrossAccountAbort) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  // Configure the test.
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                   kPreviousEmail);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Merge data after the cross account dialog.
TEST_F(DiceTurnSyncOnHelperTest, CrossAccountContinue) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  // Configure the test.
  merge_data_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE;
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                   kPreviousEmail);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Create a new profile after the cross account dialog and show the signin page.
TEST_F(DiceTurnSyncOnHelperTest, CrossAccountNewProfile) {
  // Set expectations.
  expected_merge_data_previous_email_ = kPreviousEmail;
  expected_merge_data_new_email_ = kEmail;
  expected_new_profile_username_ = kEmail;
  // Configure the test.
  merge_data_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE;
  profile()->GetPrefs()->SetString(prefs::kGoogleServicesLastUsername,
                                   kPreviousEmail);
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  base::RunLoop().RunUntilIdle();  // Profile creation is asynchronous.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Abort after the enterprise confirmation prompt.
TEST_F(DiceTurnSyncOnHelperTest, EnterpriseConfirmationAbort) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Continue after the enterprise confirmation prompt.
TEST_F(DiceTurnSyncOnHelperTest, EnterpriseConfirmationContinue) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_sync_confirmation_shown_ = true;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_CONTINUE;
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Continue with a new profile after the enterprise confirmation prompt.
TEST_F(DiceTurnSyncOnHelperTest, EnterpriseConfirmationNewProfile) {
  // Set expectations.
  expected_enterprise_confirmation_email_ = kEmail;
  expected_new_profile_username_ = kEmail;
  // Configure the test.
  user_policy_signin_service()->set_dm_token("foo");
  user_policy_signin_service()->set_client_id("bar");
  enterprise_choice_ = DiceTurnSyncOnHelper::SIGNIN_CHOICE_NEW_PROFILE;
  // Signin flow.
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  base::RunLoop().RunUntilIdle();  // Profile creation is asynchronous.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Tests that the sync confirmation is shown and the user can abort.
TEST_F(DiceTurnSyncOnHelperTest, UndoSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock(), SetFirstSetupComplete()).Times(0);

  // Signin flow.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  EXPECT_FALSE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Tests that the sync settings page is shown.
TEST_F(DiceTurnSyncOnHelperTest, ConfigureSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  expected_sync_settings_shown_ = true;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock(), SetFirstSetupComplete()).Times(0);

  // Configure the test.
  sync_confirmation_result_ =
      LoginUIService::SyncConfirmationUIClosedResult::CONFIGURE_SYNC_FIRST;
  // Signin flow.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_TRUE(signin_manager()->IsAuthenticated());
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id()));
  CheckDelegateCalls();
}

// Tests that the user is signed in and Sync configuration is complete.
TEST_F(DiceTurnSyncOnHelperTest, StartSync) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock(), SetFirstSetupComplete()).Times(1);
  // Configure the test.
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  // Signin flow.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);
  // Check expectations.
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id()));
  EXPECT_EQ(account_id(), signin_manager()->GetAuthenticatedAccountId());
  CheckDelegateCalls();
}

// Tests that the user is signed in and Sync configuration is complete.
// Regression test for http://crbug.com/812546
TEST_F(DiceTurnSyncOnHelperTest, ShowSyncDialogForEndConsumerAccount) {
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock(), SetFirstSetupComplete()).Times(1);

  // Signin flow.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check expectations.
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id()));
  EXPECT_EQ(account_id(), signin_manager()->GetAuthenticatedAccountId());
  CheckDelegateCalls();
}

// Tests that the user enabled unified consent,
TEST_F(DiceTurnSyncOnHelperTestWithUnifiedConsent,
       ShowSyncDialogForEndConsumerAccount_UnifiedConsentEnabled) {
  ASSERT_TRUE(unified_consent::IsUnifiedConsentFeatureEnabled());
  // Set expectations.
  expected_sync_confirmation_shown_ = true;
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  SetExpectationsForSyncStartupCompleted();
  EXPECT_CALL(*GetProfileSyncServiceMock(), SetFirstSetupComplete()).Times(1);

  // Signin flow.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check expectations.
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id()));
  EXPECT_EQ(account_id(), signin_manager()->GetAuthenticatedAccountId());
  CheckDelegateCalls();
  EXPECT_TRUE(UnifiedConsentServiceFactory::GetForProfile(profile())
                  ->IsUnifiedConsentGiven());
}

// For enterprise user, tests that the user is signed in only after Sync engine
// starts.
// Regression test for http://crbug.com/812546
TEST_F(DiceTurnSyncOnHelperTest,
       ShowSyncDialogBlockedUntilSyncStartupCompletedForEnterpriseAccount) {
  // Reset the account info to be an enterprise account.
  UseEnterpriseAccount();

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending();

  // Signin flow.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  DiceTurnSyncOnHelper* dice_sync_starter = CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check that the account was set in the sign-in manager, but the sync
  // confirmation dialog was not yet shown.
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id()));
  EXPECT_EQ(account_id(), signin_manager()->GetAuthenticatedAccountId());
  CheckDelegateCalls();

  // Simulate that sync startup has completed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetProfileSyncServiceMock(), SetFirstSetupComplete()).Times(1);
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  dice_sync_starter->SyncStartupCompleted();
  CheckDelegateCalls();
}

// For enterprise user, tests that the user is signed in only after Sync engine
// fails to start.
// Regression test for http://crbug.com/812546
TEST_F(DiceTurnSyncOnHelperTest,
       ShowSyncDialogBlockedUntilSyncStartupFailedForEnterpriseAccount) {
  // Reset the account info to be an enterprise account.
  UseEnterpriseAccount();

  // Set expectations.
  expected_sync_confirmation_shown_ = false;
  SetExpectationsForSyncStartupPending();

  // Signin flow.
  EXPECT_FALSE(signin_manager()->IsAuthenticated());
  DiceTurnSyncOnHelper* dice_sync_starter = CreateDiceTurnOnSyncHelper(
      DiceTurnSyncOnHelper::SigninAbortedMode::REMOVE_ACCOUNT);

  // Check that the primary account was added to the token service and in the
  // sign-in manager.
  EXPECT_TRUE(token_service()->RefreshTokenIsAvailable(account_id()));
  EXPECT_EQ(account_id(), signin_manager()->GetAuthenticatedAccountId());
  CheckDelegateCalls();

  // Simulate that sync startup has failed.
  expected_sync_confirmation_shown_ = true;
  EXPECT_CALL(*GetProfileSyncServiceMock(), SetFirstSetupComplete()).Times(1);
  sync_confirmation_result_ = LoginUIService::SyncConfirmationUIClosedResult::
      SYNC_WITH_DEFAULT_SETTINGS;
  dice_sync_starter->SyncStartupFailed();
  CheckDelegateCalls();
}
