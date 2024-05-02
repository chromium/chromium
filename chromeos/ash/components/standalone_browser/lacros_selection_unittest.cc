// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/lacros_selection.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// This implementation of RAII for LacrosSelection is to make it easy reset
// the state between runs.
class ScopedLacrosSelectionCache {
 public:
  explicit ScopedLacrosSelectionCache(
      ash::standalone_browser::LacrosSelectionPolicy lacros_selection) {
    SetLacrosSelection(lacros_selection);
  }
  ScopedLacrosSelectionCache(const ScopedLacrosSelectionCache&) = delete;
  ScopedLacrosSelectionCache& operator=(const ScopedLacrosSelectionCache&) =
      delete;
  ~ScopedLacrosSelectionCache() {
    ash::standalone_browser::ClearLacrosSelectionCacheForTest();
  }

 private:
  void SetLacrosSelection(
      ash::standalone_browser::LacrosSelectionPolicy lacros_selection) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosSelection, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosSelectionPolicyName(lacros_selection)),
               /*external_data_fetcher=*/nullptr);
    ash::standalone_browser::CacheLacrosSelection(policy);
  }
};

class LacrosSelectionTest : public testing::Test {
 public:
  LacrosSelectionTest() = default;
  ~LacrosSelectionTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>());
  }

  void TearDown() override {
    if (ash::standalone_browser::BrowserSupport::
            IsInitializedForPrimaryUser()) {
      ash::standalone_browser::BrowserSupport::Shutdown();
    }
    fake_user_manager_.Reset();
    ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(
        std::nullopt);
  }

  const user_manager::User* AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    ash::standalone_browser::BrowserSupport::InitializeForPrimaryUser(
        policy::PolicyMap(), false, false);
    return user;
  }

 private:
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
};

TEST_F(LacrosSelectionTest, LacrosSelection) {
  // Neither policy nor command line have any preference on Lacros selection.
  EXPECT_FALSE(ash::standalone_browser::DetermineLacrosSelection());

  {
    // LacrosSelection policy has precedence over command line.
    ScopedLacrosSelectionCache cache(
        ash::standalone_browser::LacrosSelectionPolicy::kRootfs);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitchASCII(
        ash::standalone_browser::kLacrosSelectionSwitch,
        ash::standalone_browser::kLacrosSelectionStateful);
    EXPECT_EQ(ash::standalone_browser::DetermineLacrosSelection(),
              ash::standalone_browser::LacrosSelection::kRootfs);
  }

  {
    // LacrosSelection allows command line check, but command line is not set.
    ScopedLacrosSelectionCache cache(
        ash::standalone_browser::LacrosSelectionPolicy::kUserChoice);
    EXPECT_FALSE(ash::standalone_browser::DetermineLacrosSelection());
  }

  {
    // LacrosSelection allows command line check.
    ScopedLacrosSelectionCache cache(
        ash::standalone_browser::LacrosSelectionPolicy::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitchASCII(
        ash::standalone_browser::kLacrosSelectionSwitch,
        ash::standalone_browser::kLacrosSelectionRootfs);
    EXPECT_EQ(ash::standalone_browser::DetermineLacrosSelection(),
              ash::standalone_browser::LacrosSelection::kRootfs);
  }

  {
    // LacrosSelection allows command line check.
    ScopedLacrosSelectionCache cache(
        ash::standalone_browser::LacrosSelectionPolicy::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitchASCII(
        ash::standalone_browser::kLacrosSelectionSwitch,
        ash::standalone_browser::kLacrosSelectionStateful);
    EXPECT_EQ(ash::standalone_browser::DetermineLacrosSelection(),
              ash::standalone_browser::LacrosSelection::kStateful);
  }
}

// LacrosSelection has no effect on non-googlers.
TEST_F(LacrosSelectionTest, LacrosSelectionPolicyIgnoreNonGoogle) {
  AddRegularUser("user@random.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosSelectionPolicyIgnore);

  {
    ScopedLacrosSelectionCache cache(
        ash::standalone_browser::LacrosSelectionPolicy::kRootfs);
    EXPECT_EQ(ash::standalone_browser::GetCachedLacrosSelectionPolicy(),
              ash::standalone_browser::LacrosSelectionPolicy::kRootfs);
    EXPECT_EQ(ash::standalone_browser::DetermineLacrosSelection(),
              ash::standalone_browser::LacrosSelection::kRootfs);
  }
}

// LacrosSelection has an effect on googlers.
TEST_F(LacrosSelectionTest,
       LacrosSelectionPolicyIgnoreGoogleDisableToUserChoice) {
  AddRegularUser("user@google.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosSelectionPolicyIgnore);

  {
    ScopedLacrosSelectionCache cache(
        ash::standalone_browser::LacrosSelectionPolicy::kRootfs);
    EXPECT_EQ(ash::standalone_browser::GetCachedLacrosSelectionPolicy(),
              ash::standalone_browser::LacrosSelectionPolicy::kUserChoice);
    EXPECT_FALSE(ash::standalone_browser::DetermineLacrosSelection());
  }
}

}  // namespace
