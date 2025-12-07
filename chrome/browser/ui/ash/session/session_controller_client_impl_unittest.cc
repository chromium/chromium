// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
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
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy.h"
#include "components/user_manager/multi_user/multi_user_sign_in_policy_controller.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

using session_manager::SessionState;

namespace {

constexpr auto kTestUserId =
    AccountId::Literal::FromUserEmailGaiaId("user@test.com",
                                            GaiaId::Literal("0123456789"));
constexpr auto kOtherTestUserId =
    AccountId::Literal::FromUserEmailGaiaId("other@test.com",
                                            GaiaId::Literal("9876543210"));

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
  ~SessionControllerClientImplTest() override = default;

  void SetUp() override {
    cros_settings_test_helper_ =
        std::make_unique<ash::ScopedCrosSettingsTestHelper>();
    ash::LoginState::Initialize();
    session_manager_ = std::make_unique<session_manager::SessionManager>(
        std::make_unique<session_manager::FakeSessionManagerDelegate>());
    // Initialize the UserManager singleton.
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        g_browser_process->local_state(), ash::CrosSettings::Get()));
    session_manager_->OnUserManagerCreated(user_manager_.Get());
    // Initialize AssistantBrowserDelegate singleton.
    assistant_delegate_ = std::make_unique<AssistantBrowserDelegateImpl>();

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  void TearDown() override {

    for (user_manager::User* user : user_manager_->GetPersistedUsers()) {
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
    session_manager_.reset();
    user_manager_.Reset();

    ash::LoginState::Shutdown();
    cros_settings_test_helper_.reset();
  }

  // Registers a user with the given `account_id` and `user_type`.
  // Currently, only regular and child users are supported.
  [[nodiscard]] user_manager::User* RegisterUser(
      const AccountId& account_id,
      user_manager::UserType user_type = user_manager::UserType::kRegular) {
    user_manager::TestHelper test_helper(user_manager_.Get());
    switch (user_type) {
      case user_manager::UserType::kRegular:
        return test_helper.AddRegularUser(account_id);
      case user_manager::UserType::kChild:
        return test_helper.AddChildUser(account_id);
      default:
        NOTREACHED();
    }
  }

  TestingProfile* LogIn(const AccountId& account_id) {
    session_manager_->CreateSession(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id),
        /*new_user=*/false,
        /*has_active_session=*/false);

    // Simulate that user profile is loaded.
    auto* result = CreateTestingProfile(account_id);
    session_manager_->NotifyUserProfileLoaded(account_id);

    session_manager_->SetSessionState(SessionState::ACTIVE);
    return result;
  }

  // Get the active user.
  const std::string& GetActiveUserEmail() {
    return user_manager::UserManager::Get()
        ->GetActiveUser()
        ->GetAccountId()
        .GetUserEmail();
  }

  // Calls private methods to create a testing profile. The created profile
  // is owned by ProfileManager.
  TestingProfile* CreateTestingProfile(const AccountId& account_id) {
    TestingProfile* profile = nullptr;
    {
      ash::ScopedAccountIdAnnotator account_id_annotator(
          profile_manager_->profile_manager(), account_id);
      profile =
          profile_manager_->CreateTestingProfile(account_id.GetUserEmail());
    }
    user_manager::UserManager::Get()->OnUserProfileCreated(account_id,
                                                           profile->GetPrefs());
    return profile;
  }

  session_manager::SessionManager& session_manager() {
    return *session_manager_;
  }
  ash::SessionTerminationManager& session_termination_manager() {
    return session_termination_manager_;
  }

 private:
  // Sorted in the production initialization order.
  ash::SessionTerminationManager session_termination_manager_;
  content::BrowserTaskEnvironment task_environment_;
  user_manager::ScopedUserManager user_manager_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;
  std::unique_ptr<AssistantBrowserDelegateImpl> assistant_delegate_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<ash::ScopedCrosSettingsTestHelper> cros_settings_test_helper_;
};

// Make sure that cycling one user does not cause any harm.
TEST_F(SessionControllerClientImplTest, CyclingOneUser) {
  const auto account_id = AccountId::FromUserEmailGaiaId("firstuser@test.com",
                                                         GaiaId("1111111111"));
  ASSERT_TRUE(RegisterUser(account_id));
  LogIn(account_id);

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
  SessionControllerClientImpl client(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  TestSessionController session_controller;
  client.Init();

  const AccountId first_user = AccountId::FromUserEmailGaiaId(
      "firstuser@test.com", GaiaId("1111111111"));
  const AccountId second_user = AccountId::FromUserEmailGaiaId(
      "seconduser@test.com", GaiaId("2222222222"));
  const AccountId third_user = AccountId::FromUserEmailGaiaId(
      "thirduser@test.com", GaiaId("3333333333"));
  ASSERT_TRUE(RegisterUser(first_user));
  ASSERT_TRUE(RegisterUser(second_user));
  ASSERT_TRUE(RegisterUser(third_user));

  LogIn(first_user);
  LogIn(second_user);
  LogIn(third_user);

  session_manager().SwitchActiveSession(first_user);

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

TEST_F(SessionControllerClientImplTest, MultiUserNoLogin) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

TEST_F(SessionControllerClientImplTest, MultiUserSingleUser) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  LogIn(kTestUserId);

  // Because the only registered user already logged in.
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

TEST_F(SessionControllerClientImplTest, MultiUserMultiUsers) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  ASSERT_TRUE(RegisterUser(kOtherTestUserId));

  LogIn(kTestUserId);
  // kOtherTestUserId can still log in.
  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure MultiUser disabled by primary user policy.
TEST_F(SessionControllerClientImplTest, MultiUserDisallowedByUserPolicy) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  ASSERT_TRUE(RegisterUser(kOtherTestUserId));
  TestingProfile* user_profile = LogIn(kTestUserId);

  user_profile->GetPrefs()->SetString(
      user_manager::prefs::kMultiProfileUserBehaviorPref,
      user_manager::MultiUserSignInPolicyToPrefValue(
          user_manager::MultiUserSignInPolicy::kNotAllowed));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure MultiUser is disabled for Family Link users.
TEST_F(SessionControllerClientImplTest, MultiUserDisallowedForFamilyLinkUsers) {
  constexpr auto kChildUserId = AccountId::Literal::FromUserEmailGaiaId(
      "child@gmail.com", GaiaId::Literal("12345678"));
  ASSERT_TRUE(RegisterUser(kTestUserId));
  ASSERT_TRUE(RegisterUser(kChildUserId, user_manager::UserType::kChild));

  LogIn(kChildUserId);

  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure MultiUser is allowed if the primary user has used
// policy-provided trust anchors.
TEST_F(SessionControllerClientImplTest,
       MultiUserAllowedWithPolicyCertificates) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  ASSERT_TRUE(RegisterUser(kOtherTestUserId));

  TestingProfile* user_profile = LogIn(kTestUserId);

  ASSERT_TRUE(
      policy::PolicyCertServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          user_profile, base::BindRepeating(&CreateTestPolicyCertService)));

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  // Flush tasks posted to IO.
  base::RunLoop().RunUntilIdle();
}

// Make sure MultiUser is allowed if the primary user has policy-provided
// trust anchors in memory.
TEST_F(SessionControllerClientImplTest,
       MultiUserDisallowedByPrimaryUserCertificatesInMemory) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  ASSERT_TRUE(RegisterUser(kOtherTestUserId));

  TestingProfile* user_profile = LogIn(kTestUserId);

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
       AddUserToMultiUserDisallowedByMaximumUsers) {
  ASSERT_TRUE(RegisterUser(kTestUserId));

  // Register "kMaximumNumberOfUserSessions" additional users.
  // This is enough large to run the following check.
  std::vector<AccountId> account_ids;
  for (int i = 0; i < session_manager::kMaximumNumberOfUserSessions; ++i) {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId(base::StringPrintf("%d@gmail.com", i),
                                       GaiaId(base::StringPrintf("%d", i)));
    ASSERT_TRUE(RegisterUser(account_id));
    account_ids.push_back(account_id);
  }

  LogIn(kTestUserId);
  auto* user_manager = user_manager::UserManager::Get();
  size_t index = 0;
  while (user_manager->GetLoggedInUsers().size() <
         session_manager::kMaximumNumberOfUserSessions) {
    EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
              SessionControllerClientImpl::GetAddUserSessionPolicy());
    ASSERT_LT(index, account_ids.size());
    LogIn(account_ids[index++]);
  }

  // Now hit the limit of the multi users.
  EXPECT_EQ(user_manager->GetLoggedInUsers().size(),
            size_t{session_manager::kMaximumNumberOfUserSessions});
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled by primary user policy.
TEST_F(SessionControllerClientImplTest,
       AddUserToMultiUserDisallowedByPrimaryUserPolicy) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  ASSERT_TRUE(RegisterUser(kOtherTestUserId));

  TestingProfile* user_profile = LogIn(kTestUserId);

  EXPECT_EQ(ash::AddUserSessionPolicy::ALLOWED,
            SessionControllerClientImpl::GetAddUserSessionPolicy());

  user_profile->GetPrefs()->SetString(
      user_manager::prefs::kMultiProfileUserBehaviorPref,
      user_manager::MultiUserSignInPolicyToPrefValue(
          user_manager::MultiUserSignInPolicy::kNotAllowed));
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

// Make sure adding users to multiprofiles disabled because device is locked
// to single user.
TEST_F(SessionControllerClientImplTest,
       AddUserToMultiUserDisallowedByLockToSingleUser) {
  ASSERT_TRUE(RegisterUser(kTestUserId));
  ASSERT_TRUE(RegisterUser(kOtherTestUserId));

  LogIn(kTestUserId);

  session_termination_manager().SetDeviceLockedToSingleUser();
  EXPECT_EQ(ash::AddUserSessionPolicy::ERROR_LOCKED_TO_SINGLE_USER,
            SessionControllerClientImpl::GetAddUserSessionPolicy());
}

TEST_F(SessionControllerClientImplTest, SendUserSession) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  TestSessionController session_controller;
  client.Init();

  // No user session sent yet.
  EXPECT_EQ(0, session_controller.update_user_session_count());

  // Simulate login.
  ASSERT_TRUE(RegisterUser(kTestUserId));
  LogIn(kTestUserId);

  // User session was sent.
  EXPECT_EQ(1, session_controller.update_user_session_count());
  ASSERT_TRUE(session_controller.last_user_session());

  // Simulate a request for an update where nothing changed.
  client.SendUserSession(
      *user_manager::UserManager::Get()->GetLoggedInUsers()[0]);

  // Session was not updated because nothing changed.
  EXPECT_EQ(1, session_controller.update_user_session_count());
}

TEST_F(SessionControllerClientImplTest, SetUserSessionOrder) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  TestSessionController session_controller;
  client.Init();

  // User session order is not sent.
  EXPECT_EQ(0, session_controller.set_user_session_order_count());

  ASSERT_TRUE(RegisterUser(kTestUserId));

  // Simulate a not-signed-in user has the user image changed.
  const AccountId not_signed_in(AccountId::FromUserEmailGaiaId(
      "not_signed_in@test.com", GaiaId("12345")));
  user_manager::User* not_signed_in_user = RegisterUser(not_signed_in);
  user_manager::UserManager::Get()->NotifyUserImageChanged(*not_signed_in_user);

  // User session order should not be sent.
  EXPECT_EQ(0, session_controller.set_user_session_order_count());

  // Simulate login.
  LogIn(kTestUserId);

  // User session order is sent after the sign-in.
  EXPECT_EQ(1, session_controller.set_user_session_order_count());
}

TEST_F(SessionControllerClientImplTest, UserPrefsChange) {
  // Create an object to test and connect it to our test interface.
  SessionControllerClientImpl client(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  TestSessionController session_controller;
  client.Init();

  // Simulate login.
  ASSERT_TRUE(RegisterUser(kTestUserId));
  TestingProfile* user_profile = LogIn(kTestUserId);

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
  SessionControllerClientImpl client(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
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
  SessionControllerClientImpl client(
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()));
  TestSessionController session_controller;
  client.Init();

  ASSERT_EQ(0, session_controller.first_session_ready_count());

  // Simulate post login tasks finish.
  session_manager().HandleUserSessionStartUpTaskCompleted();

  EXPECT_EQ(1, session_controller.first_session_ready_count());
}
