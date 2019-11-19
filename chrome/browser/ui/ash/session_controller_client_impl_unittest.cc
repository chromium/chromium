// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session_controller_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_pref_names.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/multi_profile_user_controller.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/ash/assistant/assistant_client.h"
#include "chrome/browser/ui/ash/test_session_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::FakeChromeUserManager;
using session_manager::SessionState;

namespace {

constexpr char kUser[] = "user@test.com";
constexpr char kUserGaiaId[] = "0123456789";

std::unique_ptr<KeyedService> CreateTestPolicyCertService(
    content::BrowserContext* context) {
  return policy::PolicyCertService::CreateForTesting(
      kUser, user_manager::UserManager::Get());
}

// A user manager that does not set profiles as loaded and notifies observers
// when users being added to a session.
class TestChromeUserManager : public FakeChromeUserManager {
 public:
  TestChromeUserManager() = default;
  ~TestChromeUserManager() override = default;

  // user_manager::UserManager:
  void UserLoggedIn(const AccountId& account_id,
                    const std::string& user_id_hash,
                    bool browser_restart,
                    bool is_child) override {
    FakeChromeUserManager::UserLoggedIn(account_id, user_id_hash,
                                        browser_restart, is_child);
    active_user_ = const_cast<user_manager::User*>(FindUser(account_id));
    NotifyUserAddedToSession(active_user_, false);
    NotifyOnLogin();
  }

  user_manager::UserList GetUnlockUsers() const override {
    // Test case UserPrefsChange expects that the list of the unlock users
    // depends on prefs::kAllowScreenLock.
    user_manager::UserList unlock_users;
    for (user_manager::User* user : users_) {
      Profile* user_profile =
          chromeos::ProfileHelper::Get()->GetProfileByUser(user);
      // Skip if user has a profile and kAllowScreenLock is set to false.
      if (user_profile &&
          !user_profile->GetPrefs()->GetBoolean(ash::prefs::kAllowScreenLock)) {
        continue;
      }

      unlock_users.push_back(user);
    }

    return unlock_users;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestChromeUserManager);
};

}  // namespace

class SessionControllerClientImplTest : public testing::Test {
 protected:
  SessionControllerClientImplTest() {}
  ~SessionControllerClientImplTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    chromeos::LoginState::Initialize();

    // Initialize the UserManager singleton.
    user_manager_ = new TestChromeUserManager;
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_));
    // Initialize AssistantClient singleton.
    assistant_client_ = std::make_unique<AssistantClient>();

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    cros_settings_test_helper_ =
        std::make_unique<chromeos::ScopedCrosSettingsTestHelper>();
  }

  void TearDown() override {
    assistant_client_.reset();
    user_manager_enabler_.reset();
    user_manager_ = nullptr;
    profile_manager_.reset();

    // We must ensure that the network::CertVerifierWithTrustAnchors outlives
    // the PolicyCertService so shutdown the profile here. Additionally, we need
    // to run the message loop between freeing the PolicyCertService and
    // freeing the network::CertVerifierWithTrustAnchors (see
    // PolicyCertService::OnTrustAnchorsChanged() which is called from
    // PolicyCertService::Shutdown()).
    base::RunLoop().RunUntilIdle();

    chromeos::LoginState::Shutdown();
    testing::Test::TearDown();
  }

  // Add and log in a user to the session.
  void UserAddedToSession(const AccountId& account_id) {
    const user_manager::User* user = user_manager()->AddUser(account_id);
    session_manager_.CreateSession(
        account_id,
        chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
            account_id.GetUserEmail()),
        false);

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

  TestChromeUserManager* user_manager() { return user_manager_; }

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
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                      profile);
    return profile;
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext context_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<AssistantClient> assistant_client_;
  session_manager::SessionManager session_manager_;
  chromeos::SessionTerminationManager session_termination_manager_;

 private:
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  // Owned by |user_manager_enabler_|.
  TestChromeUserManager* user_manager_ = nullptr;

  std::unique_ptr<chromeos::ScopedCrosSettingsTestHelper>
      cros_settings_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(SessionControllerClientImplTest);
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
      prefs::kMultiProfileUserBehavior,
      chromeos::MultiProfileUserController::kBehaviorNotAllowed);
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure MultiProfile disabled by primary user policy certificates.
TEST_F(SessionControllerClientImplTest,
       MultiProfileDisallowedByPolicyCertificates) {
  InitForMultiProfile();
  user_manager()->AddUser(
      AccountId::FromUserEmailGaiaId("bb@b.b", "4444444444"));

  const AccountId account_id(
      AccountId::FromUserEmailGaiaId(kUser, kUserGaiaId));
  user_manager()->LoginUser(account_id);
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
  policy::PolicyCertServiceFactory::SetUsedPolicyCertificates(
      account_id.GetUserEmail());
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure MultiProfile disabled by primary user certificates in memory.
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
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
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
      prefs::kMultiProfileUserBehavior,
      chromeos::MultiProfileUserController::kBehaviorNotAllowed);
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
  session_termination_manager_.SetDeviceLockedToSingleUser();
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
  TestingProfile* user_profile = CreateTestingProfile(user);
  session_manager_.CreateSession(
      account_id,
      chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
          account_id.GetUserEmail()),
      false);
  session_manager_.SetSessionState(SessionState::ACTIVE);

  // User session was sent.
  EXPECT_EQ(1, session_controller.update_user_session_count());
  ASSERT_TRUE(session_controller.last_user_session());
  EXPECT_EQ(content::BrowserContext::GetServiceInstanceGroupFor(user_profile),
            session_controller.last_user_session()
                ->user_info.service_instance_group.value());

  // Simulate a request for an update where nothing changed.
  client.SendUserSession(*user_manager()->GetLoggedInUsers()[0]);

  // Session was not updated because nothing changed.
  EXPECT_EQ(1, session_controller.update_user_session_count());
}

TEST_F(SessionControllerClientImplTest, SupervisedUser) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client;
  TestSessionController session_controller;
  client.Init();

  // Simulate the login screen. No user session yet.
  session_manager_.SetSessionState(SessionState::LOGIN_PRIMARY);
  EXPECT_FALSE(session_controller.last_user_session().has_value());

  // Simulate a supervised user logging in.
  const AccountId account_id(AccountId::FromUserEmail("child@test.com"));
  const user_manager::User* user =
      user_manager()->AddSupervisedUser(account_id);
  ASSERT_TRUE(user);

  // Start session. This logs in the user and sends an active user notification.
  // The hash must match the one used by FakeChromeUserManager.
  session_manager_.CreateSession(
      account_id,
      chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
          "child@test.com"),
      false);

  // Simulate profile creation after login.
  TestingProfile* user_profile = CreateTestingProfile(user);
  user_profile->SetSupervisedUserId("child-id");

  // Simulate supervised user custodians.
  PrefService* prefs = user_profile->GetPrefs();
  prefs->SetString(prefs::kSupervisedUserCustodianEmail, "parent1@test.com");
  prefs->SetString(prefs::kSupervisedUserSecondCustodianEmail,
                   "parent2@test.com");

  // Simulate the notification that the profile is ready.
  session_manager_.NotifyUserProfileLoaded(account_id);

  // User session could only be made active after user profile is loaded.
  session_manager_.SetSessionState(SessionState::ACTIVE);

  // The session controller received session info and user session.
  EXPECT_LT(0u, session_controller.last_user_session()->session_id);
  EXPECT_EQ(user_manager::USER_TYPE_SUPERVISED,
            session_controller.last_user_session()->user_info.type);

  // The custodians were sent over the mojo interface.
  EXPECT_EQ("parent1@test.com",
            session_controller.last_user_session()->custodian_email);
  EXPECT_EQ("parent2@test.com",
            session_controller.last_user_session()->second_custodian_email);

  // Simulate an update to the custodian information.
  prefs->SetString(prefs::kSupervisedUserCustodianEmail, "parent3@test.com");
  client.OnCustodianInfoChanged();

  // The updated custodian was sent over the mojo interface.
  EXPECT_EQ("parent3@test.com",
            session_controller.last_user_session()->custodian_email);
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
  session_manager_.CreateSession(
      account_id,
      chromeos::ProfileHelper::GetUserIdHashByUserIdForTesting(
          account_id.GetUserEmail()),
      false);

  // Simulate the notification that the profile is ready.
  TestingProfile* const user_profile = CreateTestingProfile(user);
  session_manager_.NotifyUserProfileLoaded(account_id);

  // User session could only be made active after user profile is loaded.
  session_manager_.SetSessionState(SessionState::ACTIVE);

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
  const base::TimeDelta length_limit = base::TimeDelta::FromHours(1);
  const base::TimeTicks start_time = base::TimeTicks::Now();
  PrefService* local_state = TestingBrowserProcess::GetGlobal()->local_state();
  local_state->SetInteger(prefs::kSessionLengthLimit,
                          length_limit.InMilliseconds());
  local_state->SetInt64(prefs::kSessionStartTime, start_time.ToInternalValue());
  EXPECT_EQ(length_limit, session_controller.last_session_length_limit());
  EXPECT_EQ(start_time, session_controller.last_session_start_time());
}
