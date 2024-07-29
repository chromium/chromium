// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_capabilities.h"

#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace supervised_user {
namespace {
constexpr char kChildEmail[] = "name@gmail.com";
}  // namespace

class SupervisedUserCapabilitiesTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(SupervisedUserCapabilitiesTest,
       SignedOutUserNotSubjectToParentalControls) {
  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kFalse);
}

TEST_F(SupervisedUserCapabilitiesTest,
       SignedInAdultNotSubjectToParentalControls) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(false);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kFalse);
}

TEST_F(SupervisedUserCapabilitiesTest, SignedInChildSubjectToParentalControls) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);
  AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
  mutator.set_is_subject_to_parental_controls(true);
  identity_test_env_.UpdateAccountInfoForAccount(account_info);

  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kTrue);
}

TEST_F(SupervisedUserCapabilitiesTest,
       SignedInUnknownIsSubjectToParentalControls) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kChildEmail, signin::ConsentLevel::kSignin);

  EXPECT_EQ(IsPrimaryAccountSubjectToParentalControls(
                identity_test_env_.identity_manager()),
            signin::Tribool::kUnknown);
}

}  // namespace supervised_user
