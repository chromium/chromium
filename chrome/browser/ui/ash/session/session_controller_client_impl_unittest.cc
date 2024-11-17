// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/ui/ash/assistant/assistant_browser_delegate_impl.h"
#include "chrome/browser/ui/ash/session/test_session_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

using session_manager::SessionState;

namespace {

constexpr char kUser[] = "user@test.com";
constexpr char kUserGaiaId[] = "0123456789";

std::unique_ptr<KeyedService> CreateTestPolicyCertService(
    content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
      Profile::FromBrowserContext(context));
}

}  // namespace

class SessionControllerClientImplTest : public testing::Test {
 public:
  SessionControllerClientImplTest(const SessionControllerClientImplTest&) =
      delete;
  SessionControllerClientImplTest& operator=(
      const SessionControllerClientImplTest&) = delete;

 protected:
  SessionControllerClientImplTest() = default;
  ~SessionControllerClientImplTest() override {}

  void SetUp() override {
    ash::LoginState::Initialize();

    // Initialize the UserManager singleton.
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    // Initialize AssistantBrowserDelegate singleton.
    assistant_delegate_ = std::make_unique<AssistantBrowserDelegateImpl>();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &local_state_);
    ASSERT_TRUE(profile_manager_->SetUp());

    browser_manager_ = std::make_unique<crosapi::FakeBrowserManager>();

    cros_settings_test_helper_ =
        std::make_unique<ash::ScopedCrosSettingsTestHelper>();
  }

  void TearDown() override {
    cros_settings_test_helper_.reset();
    browser_manager_.reset();

    for (user_manager::User* user : user_manager_->GetUsers()) {
      user_manager_->OnUserProfileWillBeDestroyed(user->GetAccountId());
    }
    profile_manager_.reset();

    // We must ensure that the network::CertVerifierWithTrustAnchors outlives
    // the PolicyCertService so shutdown the profile here. Additionally, we need
    // to run the message loop between freeing the PolicyCertService and
    // freeing the network::CertVerifierWithTrustAnchors (see
    // PolicyCertService::OnTrustAnchorsChanged() which is called from
    // PolicyCertService::Shutdown()).
    base::RunLoop().RunUntilIdle();

    assistant_delegate_.reset();
    user_manager_.Reset();

    ash::LoginState::Shutdown();
  }

  // Add and log in a user to the session.
  void UserAddedToSession(const AccountId& account_id, bool is_child = false) {
    const user_manager::User* user =
        is_child ? user_manager()->AddChildUser(account_id)
                 : user_manager()->AddUser(account_id);
    session_manager_.CreateSession(account_id, user->username_hash(), is_child);

    // Simulate that user profile is loaded.
    CreateTestingProfile(user);
    session_manager_.NotifyUserProfileLoaded(account_id);

    session_manager_.SetSessionState(SessionState::ACTIVE);
  }

  // Get the active user.
  const std::string& GetActiveUserEmail() {
    return user_manager::UserManager::Get()
        ->GetActiveUser()
        ->GetAccountId()
        .GetUserEmail();
  }

  ash::FakeChromeUserManager* user_manager() { return user_manager_.Get(); }

  // Adds a regular user with a profile.
  TestingProfile* InitForMultiProfile() {
    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
    const user_manager::User* user = user_manager()->AddUser(account_id);

    // Note that user profiles are created after user login in reality.
    return CreateTestingProfile(user);
  }

  // Calls private methods to create a testing profile. The created profile
  // is owned by ProfileManager.
  TestingProfile* CreateTestingProfile(const user_manager::User* user) {
    const AccountId& account_id = user->GetAccountId();
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    profile->set_profile_name(account_id.GetUserEmail());
    user_manager()->OnUserProfileCreated(account_id, profile->GetPrefs());
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user, profile);
    return profile;
  }

  session_manager::SessionManager& session_manager() {
    return session_manager_;
  }
  ash::SessionTerminationManager& session_termination_manager() {
    return session_termination_manager_;
  }

 private:
  // Sorted in the production initialization order.
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};
  session_manager::SessionManager session_manager_;
  ash::SessionTerminationManager session_termination_manager_;
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;
  std::unique_ptr<AssistantBrowserDelegateImpl> assistant_delegate_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<crosapi::FakeBrowserManager> browser_manager_;
  std::unique_ptr<ash::ScopedCrosSettingsTestHelper> cros_settings_test_helper_;
};

// Make sure that cycling one user does not cause any harm.
TEST_F(SessionControllerClientImplTest, CyclingOneUser) {
  UserAddedToSession(
      AccountId::FromUserEmailGaiaId("firstuser@test.com", "1111111111"));

  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  SessionControllerClientImpl::DoCycleActiveUser(ash::CycleUserDirection::NEXT);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  SessionControllerClientImpl::DoCycleActiveUser(
      ash::CycleUserDirection::PREVIOUS);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
}

// Cycle three users forwards and backwards to see that it works.
TEST_F(SessionControllerClientImplTest, CyclingThreeUsers) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client;
  TestSessionController session_controller;
  client.Init();

  const AccountId first_user =
      AccountId::FromUserEmailGaiaId("firstuser@test.com", "1111111111");
  const AccountId second_user =
      AccountId::FromUserEmailGaiaId("seconduser@test.com", "2222222222");
  const AccountId third_user =
      AccountId::FromUserEmailGaiaId("thirduser@test.com", "3333333333");
  UserAddedToSession(first_user);
  UserAddedToSession(second_user);
  UserAddedToSession(third_user);
  user_manager()->SwitchActiveUser(first_user);

  // Cycle forward.
  const ash::CycleUserDirection forward = ash::CycleUserDirection::NEXT;
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
  SessionControllerClientImpl::DoCycleActiveUser(forward);
  EXPECT_EQ("seconduser@test.com", GetActiveUserEmail());
  SessionControllerClientImpl::DoCycleActiveUser(forward);
  EXPECT_EQ("thirduser@test.com", GetActiveUserEmail());
  SessionControllerClientImpl::DoCycleActiveUser(forward);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());

  // Cycle backwards.
  const ash::CycleUserDirection backward = ash::CycleUserDirection::PREVIOUS;
  SessionControllerClientImpl::DoCycleActiveUser(backward);
  EXPECT_EQ("thirduser@test.com", GetActiveUserEmail());
  SessionControllerClientImpl::DoCycleActiveUser(backward);
  EXPECT_EQ("seconduser@test.com", GetActiveUserEmail());
  SessionControllerClientImpl::DoCycleActiveUser(backward);
  EXPECT_EQ("firstuser@test.com", GetActiveUserEmail());
}

// Make sure MultiProfile disabled by primary user policy.
TEST_F(SessionControllerClientImplTest, MultiProfileDisallowedByUserPolicy) {
  TestingProfile* user_profile = InitForMultiProfile();
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444"));
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  user_profile->GetPrefs()->SetString(
      user_manager::prefs::kMultiProfileUserBehaviorPref,
      user_manager::MultiUserSignInPolicyToPrefValue(
          user_manager::MultiUserSignInPolicy::kNotAllowed));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure MultiProfile is disabled for Family Link users.
TEST_F(SessionControllerClientImplTest,
       MultiProfileDisallowedForFamilyLinkUsers) {
  InitForMultiProfile();
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId("child@gmail.com", "12345678"));
  UserAddedToSession(account_id, /*is_child=*/true);

  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure MultiProfile is allowed if the primary user has used
// policy-provided trust anchors.
TEST_F(SessionControllerClientImplTest,
       MultiProfileAllowedWithPolicyCertificates) {
  TestingProfile* user_profile = InitForMultiProfile();
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444"));

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          user_profile, base::BindRepeating(&CreateTestPolicyCertService)));
  policy::PolicyCertServiceFactory::GetForProfile(user_profile)
      ->SetUsedPolicyCertificates();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure MultiProfile is allowed if the primary user has policy-provided
// trust anchors in memory.
TEST_F(SessionControllerClientImplTest,
       MultiProfileDisallowedByPrimaryUserCertificatesInMemory) {
  TestingProfile* user_profile = InitForMultiProfile();
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444"));

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          user_profile, base::BindRepeating(&CreateTestPolicyCertService)));
  policy::PolicyCertService* service =
      policy::PolicyCertServiceFactory::GetForProfile(user_profile);
  ASSERT_TRUE(service);

  EXPECT_FALSE(service->has_policy_certificates());
  net::CertificateList certificates;
  certificates.push_back(
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem"));
  service->SetPolicyTrustAnchorsForTesting(/*trust_anchors=*/certificates);
  EXPECT_TRUE(service->has_policy_certificates());
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure adding users to multiprofiles disabled by reaching maximum
// number of users in sessions.
TEST_F(SessionControllerClientImplTest,
       AddUserToMultiprofileDisallowedByMaximumUsers) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
  AccountId account_id(AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  while (user_manager()->GetLoggedInUsers().size() <
         session_manager::kMaximumNumberOfUserSessions) {
    account_id = AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444");
    user_manager()->AddUser(account_id);
    user_manager()->LoginUser(account_id);
  }
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled by logging in all possible
// users.
TEST_F(SessionControllerClientImplTest,
       AddUserToMultiprofileDisallowedByAllUsersLogged) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  UserAddedToSession(AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444"));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled by primary user policy.
TEST_F(SessionControllerClientImplTest,
       AddUserToMultiprofileDisallowedByPrimaryUserPolicy) {
  TestingProfile* user_profile = InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  user_profile->GetPrefs()->SetString(
      user_manager::prefs::kMultiProfileUserBehaviorPref,
      user_manager::MultiUserSignInPolicyToPrefValue(
          user_manager::MultiUserSignInPolicy::kNotAllowed));
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444"));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled because device is locked
// to single user.
TEST_F(SessionControllerClientImplTest,
       AddUserToMultiprofileDisallowedByLockToSingleUser) {
  InitForMultiProfile();

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  session_termination_manager().SetDeviceLockedToSingleUser();
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444"));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_LOCKED_TO_SINGLE_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

TEST_F(SessionControllerClientImplTest, SendUserSession) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client;
  TestSessionController session_controller;
  client.Init();

  // No user session sent yet.
  EXPECT_EQ(0, session_controller.update_user_session_count());

  // Simulate login.
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId("user@test.com", "5555555555"));
  const user_manager::User* user = user_manager()->AddUser(account_id);
  CreateTestingProfile(user);
  session_manager().CreateSession(account_id, user->username_hash(), false);
  session_manager().SetSessionState(SessionState::ACTIVE);

  // User session was sent.
  EXPECT_EQ(1, session_controller.update_user_session_count());
  ASSERT_TRUE(session_controller.last_user_session());

  // Simulate a request for an update where nothing changed.
  client.SendUserSession(*user_manager()->GetLoggedInUsers()[0]);

  // Session was not updated because nothing changed.
  EXPECT_EQ(1, session_controller.update_user_session_count());
}

TEST_F(SessionControllerClientImplTest, SetUserSessionOrder) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client;
  TestSessionController session_controller;
  client.Init();

  // User session order is not sent.
  EXPECT_EQ(0, session_controller.set_user_session_order_count());

  // Simulate a not-signed-in user has the user image changed.
  const AccountId not_signed_in(
      AccountId::FromUserEmailGaiaId("not_signed_in@test.com", "12345"));
  user_manager::User* not_signed_in_user =
      user_manager()->AddUser(not_signed_in);
  user_manager()->NotifyUserImageChanged(*not_signed_in_user);

  // User session order should not be sent.
  EXPECT_EQ(0, session_controller.set_user_session_order_count());

  // Simulate login.
  UserAddedToSession(
      AccountId::FromUserEmailGaiaId("signed_in@test.com", "67890"));

  // User session order is sent after the sign-in.
  EXPECT_EQ(1, session_controller.set_user_session_order_count());
}

TEST_F(SessionControllerClientImplTest, UserPrefsChange) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client;
  TestSessionController session_controller;
  client.Init();

  // Simulate login.
  const AccountId account_id(
      AccountId::FromUserEmailGaiaId("user@test.com", "5555555555"));
  const user_manager::User* user = user_manager()->AddUser(account_id);
  session_manager().CreateSession(account_id, user->username_hash(), false);

  // Simulate the notification that the profile is ready.
  TestingProfile* const user_profile = CreateTestingProfile(user);
  session_manager().NotifyUserProfileLoaded(account_id);

  // User session could only be made active after user profile is loaded.
  session_manager().SetSessionState(SessionState::ACTIVE);

  // Manipulate user prefs and verify SessionController is updated.
  PrefService* const user_prefs = user_profile->GetPrefs();

  user_prefs->SetBoolean(ash::prefs::kAllowScreenLock, true);
  EXPECT_TRUE(session_controller.last_session_info()->can_lock_screen);
  user_prefs->SetBoolean(ash::prefs::kAllowScreenLock, false);
  EXPECT_FALSE(session_controller.last_session_info()->can_lock_screen);
  user_prefs->SetBoolean(ash::prefs::kEnableAutoScreenLock, true);
  EXPECT_TRUE(
      session_controller.last_session_info()->should_lock_screen_automatically);
  user_prefs->SetBoolean(ash::prefs::kEnableAutoScreenLock, false);
  EXPECT_FALSE(
      session_controller.last_session_info()->should_lock_screen_automatically);
}

TEST_F(SessionControllerClientImplTest, SessionLengthLimit) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client;
  TestSessionController session_controller;
  client.Init();

  // By default there is no session length limit.
  EXPECT_TRUE(session_controller.last_session_length_limit().is_zero());
  EXPECT_TRUE(session_controller.last_session_start_time().is_null());

  // Setting a session length limit in local state sends it to ash.
  const base::TimeDelta length_limit = base::Hours(1);
  const base::Time start_time = base::Time::Now();
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(prefs::kSessionLengthLimit,
                          length_limit.InMilliseconds());
  local_state->SetInt64(prefs::kSessionStartTime, start_time.ToInternalValue());
  EXPECT_EQ(length_limit, session_controller.last_session_length_limit());
  EXPECT_EQ(start_time, session_controller.last_session_start_time());
}

TEST_F(SessionControllerClientImplTest, FirstSessionReady) {
  SessionControllerClientImpl client;
  TestSessionController session_controller;
  client.Init();

  ASSERT_EQ(0, session_controller.first_session_ready_count());

  // Simulate post login tasks finish.
  session_manager().HandleUserSessionStartUpTaskCompleted();

  EXPECT_EQ(1, session_controller.first_session_ready_count());
}
