// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/lacros_availability.h"

#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

namespace ash::standalone_browser {

class LacrosAvailabilityTest : public testing::Test {
 public:
  const User* AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_.AddUser(account_id);
    fake_user_manager_.UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    return user;
  }

  user_manager::FakeUserManager fake_user_manager_;
};

TEST_F(LacrosAvailabilityTest,
       DetermineLacrosAvailabilityFromPolicyValueExternal) {
  const User* const user = AddRegularUser("user@random.com");

  // For non-Googlers, the policy can't be ignored by command line flag.
  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kLacrosAvailabilityIgnore);
    EXPECT_EQ(LacrosAvailability::kLacrosOnly,
              DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_only"));
  }

  // If there's no policy value, the choice is left to the user.
  EXPECT_EQ(LacrosAvailability::kUserChoice,
            DetermineLacrosAvailabilityFromPolicyValue(user, ""));

  // If the policy value is valid and there is no command line flag, the policy
  // should be respected.
  EXPECT_EQ(LacrosAvailability::kLacrosOnly,
            DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_only"));

  // If the policy value is invalid, the choice is left to the user.
  EXPECT_EQ(
      LacrosAvailability::kUserChoice,
      DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_tertiary"));

  // Whether LacrosGooglePolicyRollout is enabled or not makes no difference for
  // normal users.
  {
    // Disable LacrosGooglePolicyRollout.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({}, {kLacrosGooglePolicyRollout});
    EXPECT_EQ(
        LacrosAvailability::kLacrosDisallowed,
        DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_disallowed"));
    EXPECT_EQ(LacrosAvailability::kLacrosOnly,
              DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_only"));
  }
  {
    // Enable LacrosGooglePolicyRollout.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({kLacrosGooglePolicyRollout}, {});
    EXPECT_EQ(
        LacrosAvailability::kLacrosDisallowed,
        DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_disallowed"));
    EXPECT_EQ(LacrosAvailability::kLacrosOnly,
              DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_only"));
  }
}

struct LacrosAvailabilityInternalParams {
  std::string test_name;
  std::string test_account;
};

class LacrosAvailabilityInternalTest
    : public LacrosAvailabilityTest,
      public testing::WithParamInterface<LacrosAvailabilityInternalParams> {};

TEST_P(LacrosAvailabilityInternalTest,
       DetermineLacrosAvailabilityFromPolicyValueInternal) {
  const User* const user = AddRegularUser(GetParam().test_account);

  // For Googlers, the policy can be ignored by command line flag.
  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kLacrosAvailabilityIgnore);
    EXPECT_EQ(LacrosAvailability::kUserChoice,
              DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_only"));
  }

  // If there's no policy value, the choice is left to the user.
  EXPECT_EQ(LacrosAvailability::kUserChoice,
            DetermineLacrosAvailabilityFromPolicyValue(user, ""));

  // If the policy value is invalid, the choice is left to the user.
  EXPECT_EQ(
      LacrosAvailability::kUserChoice,
      DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_tertiary"));

  {
    // Disable LacrosGooglePolicyRollout.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({}, {kLacrosGooglePolicyRollout});

    // For Googlers, if the GooglePolicyRollout feature is disabled, the choice
    // is left to the user...
    EXPECT_EQ(LacrosAvailability::kUserChoice,
              DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_only"));

    // ...unless the policy is that Lacros is explicitly disallowed.
    EXPECT_EQ(
        LacrosAvailability::kLacrosDisallowed,
        DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_disallowed"));
  }

  {
    // Enable LacrosGooglePolicyRollout.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures({kLacrosGooglePolicyRollout}, {});

    // For Googlers, if the GooglePolicyRollout feature is enabled, the policy
    // should be respected.
    EXPECT_EQ(LacrosAvailability::kLacrosOnly,
              DetermineLacrosAvailabilityFromPolicyValue(user, "lacros_only"));
  }
}

INSTANTIATE_TEST_SUITE_P(
    LacrosAvailabilityInternalTests,
    LacrosAvailabilityInternalTest,
    testing::ValuesIn<LacrosAvailabilityInternalParams>(
        {{"Google_Internal", "user@google.com"},
         {"Enterprise", "user@managedchrome.com"},
         {"Robot_Account", "something@blah.iam.gserviceaccount.com"},
         {"Auth", "service@blah.apps.googleusercontent.com"}}),
    [](const testing::TestParamInfo<LacrosAvailabilityInternalTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace ash::standalone_browser
