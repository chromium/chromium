// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/core/session_manager.h"

#include <memory>
#include <vector>

#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace session_manager {
namespace {

constexpr AccountId::Literal kAccountId1 =
    AccountId::Literal::FromUserEmailGaiaId("user1@example.com",
                                            GaiaId::Literal("fake-gaia-id-1"));
constexpr AccountId::Literal kAccountId2 =
    AccountId::Literal::FromUserEmailGaiaId("user2@example.com",
                                            GaiaId::Literal("fake-gaia-id-2"));

// Records the AccountIds of the sessions passed to OnActiveSessionSwitched().
class ActiveSessionSwitchedRecorder : public SessionManagerObserver {
 public:
  explicit ActiveSessionSwitchedRecorder(SessionManager* session_manager) {
    observation_.Observe(session_manager);
  }

  // SessionManagerObserver:
  void OnActiveSessionSwitched(const Session& session) override {
    switched_account_ids_.push_back(session.account_id());
  }

  const std::vector<AccountId>& switched_account_ids() const {
    return switched_account_ids_;
  }

 private:
  std::vector<AccountId> switched_account_ids_;
  base::ScopedObservation<SessionManager, SessionManagerObserver> observation_{
      this};
};

class SessionManagerTest : public testing::Test {
 public:
  void SetUp() override {
    ash::test::UserSessionTestEnvironment::RegisterLocalStatePrefs(
        local_state_.registry());
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(&local_state_);
  }

  void TearDown() override { user_session_test_environment_.reset(); }

 protected:
  ash::test::UserSessionTestEnvironment& user_session_test_environment() {
    return *user_session_test_environment_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

TEST_F(SessionManagerTest, OnActiveSessionSwitched) {
  ASSERT_TRUE(user_session_test_environment().AddRegularUser(kAccountId1));
  ASSERT_TRUE(user_session_test_environment().AddRegularUser(kAccountId2));

  auto* session_manager = SessionManager::Get();
  auto* user_manager = user_manager::UserManager::Get();
  ActiveSessionSwitchedRecorder recorder(session_manager);

  // PrefService instances owned by the users' Profile in production.
  TestingPrefServiceSimple prefs1;
  TestingPrefServiceSimple prefs2;

  // Primary user log-in. Creating the primary session makes it active.
  session_manager->CreateSession(
      kAccountId1, user_manager::TestHelper::GetFakeUsernameHash(kAccountId1),
      /*new_user=*/false, /*has_active_session=*/false);
  EXPECT_EQ(std::vector<AccountId>({kAccountId1}),
            recorder.switched_account_ids());
  user_manager->OnUserProfileCreated(kAccountId1, &prefs1);
  session_manager->SessionStarted();
  EXPECT_EQ(std::vector<AccountId>({kAccountId1}),
            recorder.switched_account_ids());

  // Secondary user log-in. The active session switching is pending until
  // the user's Profile is created.
  session_manager->CreateSession(
      kAccountId2, user_manager::TestHelper::GetFakeUsernameHash(kAccountId2),
      /*new_user=*/false, /*has_active_session=*/false);
  EXPECT_EQ(std::vector<AccountId>({kAccountId1}),
            recorder.switched_account_ids());
  user_manager->OnUserProfileCreated(kAccountId2, &prefs2);
  EXPECT_EQ(std::vector<AccountId>({kAccountId1, kAccountId2}),
            recorder.switched_account_ids());
  session_manager->SessionStarted();
  EXPECT_EQ(std::vector<AccountId>({kAccountId1, kAccountId2}),
            recorder.switched_account_ids());

  // Explicit switching.
  session_manager->SwitchActiveSession(kAccountId1);
  EXPECT_EQ(std::vector<AccountId>({kAccountId1, kAccountId2, kAccountId1}),
            recorder.switched_account_ids());

  // Release the PrefService instances from the users before destroying them.
  user_manager->OnUserProfileWillBeDestroyed(kAccountId2);
  user_manager->OnUserProfileWillBeDestroyed(kAccountId1);
}

}  // namespace
}  // namespace session_manager
