// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_capabilities.h"

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {
constexpr char kEmail[] = "name@gmail.com";

class MockSupervisedUserCapabilitiesObserver
    : SupervisedUserCapabilitiesObserver {
 public:
  explicit MockSupervisedUserCapabilitiesObserver(
      signin::IdentityManager* identity_manager)
      : SupervisedUserCapabilitiesObserver(identity_manager) {}

  ~MockSupervisedUserCapabilitiesObserver() override = default;

  MOCK_METHOD1(OnIsSubjectToParentalControlsCapabilityChanged,
               void(CapabilityUpdateState));
};

}  // namespace

class SupervisedUserCapabilitiesTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(SupervisedUserCapabilitiesTest,
       SignedOutUserNotSubjectToParentalControls) {
  // Expect no change to account capabilities state.
  MockSupervisedUserCapabilitiesObserver observer(
      identity_test_env_.identity_manager());
  EXPECT_CALL(observer, OnIsSubjectToParentalControlsCapabilityChanged)
      .Times(0);

  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kFalse);
}

TEST_F(SupervisedUserCapabilitiesTest,
       SignedInAdultNotSubjectToParentalControls) {
  // Expect account capabilities to notify of parental controls change.
  base::RunLoop run_loop;
  MockSupervisedUserCapabilitiesObserver observer(
      identity_test_env_.identity_manager());
  EXPECT_CALL(observer, OnIsSubjectToParentalControlsCapabilityChanged)
      .WillOnce([&](supervised_user::CapabilityUpdateState state) {
        ASSERT_EQ(supervised_user::CapabilityUpdateState::kSetToFalse, state);
        run_loop.Quit();
      });

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kFalse);
}

TEST_F(SupervisedUserCapabilitiesTest, SignedInChildSubjectToParentalControls) {
  // Expect account capabilities to notify of parental controls change.
  base::RunLoop run_loop;
  MockSupervisedUserCapabilitiesObserver observer(
      identity_test_env_.identity_manager());
  EXPECT_CALL(observer, OnIsSubjectToParentalControlsCapabilityChanged)
      .WillOnce([&](supervised_user::CapabilityUpdateState state) {
        ASSERT_EQ(supervised_user::CapabilityUpdateState::kSetToTrue, state);
        run_loop.Quit();
      });

  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kTrue);
}

TEST_F(SupervisedUserCapabilitiesTest,
       SignedInUnknownIsSubjectToParentalControls) {
  // Expect no change to account capabilities state.
  MockSupervisedUserCapabilitiesObserver observer(
      identity_test_env_.identity_manager());
  EXPECT_CALL(observer, OnIsSubjectToParentalControlsCapabilityChanged)
      .Times(0);
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kUnknown);
}

// ChromeOS does not support sign-out in tests.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(SupervisedUserCapabilitiesTest,
       SignOutTriggersIsSubjectToParentalControlsUpdate) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kEmail, signin::ConsentLevel::kSignin);

  // Expect detached account capabilities update.
  base::RunLoop run_loop;
  MockSupervisedUserCapabilitiesObserver observer(
      identity_test_env_.identity_manager());
  EXPECT_CALL(observer, OnIsSubjectToParentalControlsCapabilityChanged)
      .WillOnce([&](supervised_user::CapabilityUpdateState state) {
        ASSERT_EQ(supervised_user::CapabilityUpdateState::kDetached, state);
        run_loop.Quit();
      });

  identity_test_env_.ClearPrimaryAccount();
}
#endif

}  // namespace supervised_user
