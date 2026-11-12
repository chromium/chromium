// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/test/fake_user_session_test_environment_delegate.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::test {
namespace {

constexpr AccountId::Literal kAccountId1 =
    AccountId::Literal::FromUserEmailGaiaId("user1@example.com",
                                            GaiaId::Literal("fake-gaia-id-1"));
constexpr AccountId::Literal kAccountId2 =
    AccountId::Literal::FromUserEmailGaiaId("user2@example.com",
                                            GaiaId::Literal("fake-gaia-id-2"));

constexpr char kTestPref[] = "test.pref";

class FakeUserSessionTestEnvironmentDelegateTest : public testing::Test {
 public:
  void SetUp() override {
    UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
  }

  void TearDown() override { user_session_test_environment_.reset(); }

 protected:
  void CreateUserSessionTestEnvironment(
      FakeUserSessionTestEnvironmentDelegate::RegisterProfilePrefsCallback
          register_profile_prefs) {
    user_session_test_environment_ =
        std::make_unique<UserSessionTestEnvironment>(
            &local_state_,
            std::make_unique<FakeUserSessionTestEnvironmentDelegate>(
                std::move(register_profile_prefs)));
  }

  UserSessionTestEnvironment& user_session_test_environment() {
    return *user_session_test_environment_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<UserSessionTestEnvironment> user_session_test_environment_;
};

TEST_F(FakeUserSessionTestEnvironmentDelegateTest, LogIn) {
  CreateUserSessionTestEnvironment(
      base::BindRepeating([](PrefRegistrySimple* registry) {
        user_manager::UserManager::RegisterProfilePrefs(registry);
        registry->RegisterIntegerPref(kTestPref, 0);
      }));

  user_manager::User* user =
      user_session_test_environment().AddRegularUser(kAccountId1);
  ASSERT_TRUE(user);
  EXPECT_FALSE(user->is_profile_created());
  EXPECT_FALSE(user->GetProfilePrefs());

  user_session_test_environment().LogIn(kAccountId1);
  EXPECT_TRUE(user->is_profile_created());
  PrefService* prefs = user->GetProfilePrefs();
  ASSERT_TRUE(prefs);

  // Prefs registered via the callback are available.
  EXPECT_TRUE(prefs->FindPreference(
      user_manager::prefs::kMultiProfileUserBehaviorPref));
  EXPECT_EQ(0, prefs->GetInteger(kTestPref));
  prefs->SetInteger(kTestPref, 1);
  EXPECT_EQ(1, prefs->GetInteger(kTestPref));
}

TEST_F(FakeUserSessionTestEnvironmentDelegateTest, NoRegisterProfilePrefs) {
  CreateUserSessionTestEnvironment({});

  user_manager::User* user =
      user_session_test_environment().AddRegularUser(kAccountId1);
  ASSERT_TRUE(user);

  user_session_test_environment().LogIn(kAccountId1);
  EXPECT_TRUE(user->is_profile_created());
  PrefService* prefs = user->GetProfilePrefs();
  ASSERT_TRUE(prefs);

  // No prefs are registered implicitly.
  EXPECT_FALSE(prefs->FindPreference(
      user_manager::prefs::kMultiProfileUserBehaviorPref));
}

TEST_F(FakeUserSessionTestEnvironmentDelegateTest, MultiUserLogIn) {
  CreateUserSessionTestEnvironment({});

  user_manager::User* user1 =
      user_session_test_environment().AddRegularUser(kAccountId1);
  ASSERT_TRUE(user1);
  user_manager::User* user2 =
      user_session_test_environment().AddRegularUser(kAccountId2);
  ASSERT_TRUE(user2);

  auto* session_manager = session_manager::SessionManager::Get();
  user_session_test_environment().LogIn(kAccountId1);
  EXPECT_EQ(AccountId(kAccountId1),
            session_manager->GetActiveSession()->account_id());

  // For the secondary user, the active session is switched on the user's
  // "Profile" creation, as same as production.
  user_session_test_environment().LogIn(kAccountId2);
  EXPECT_EQ(AccountId(kAccountId2),
            session_manager->GetActiveSession()->account_id());

  // Each user has its own PrefService.
  ASSERT_TRUE(user1->GetProfilePrefs());
  ASSERT_TRUE(user2->GetProfilePrefs());
  EXPECT_NE(user1->GetProfilePrefs(), user2->GetProfilePrefs());
}

}  // namespace
}  // namespace ash::test
