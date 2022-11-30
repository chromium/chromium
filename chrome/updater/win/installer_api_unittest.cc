// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer_api.h"

#include "base/test/test_reg_util_win.h"
#include "chrome/updater/updater_scope.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {
constexpr char kAppId[] = "{55d6c27c-8b97-4b76-a691-2df8810004ed}";
}  // namespace

class InstallerAPITest : public ::testing::TestWithParam<UpdaterScope> {
 protected:
  const UpdaterScope updater_scope_ = GetParam();
  registry_util::RegistryOverrideManager registry_override_;
};

INSTANTIATE_TEST_SUITE_P(UpdaterScope,
                         InstallerAPITest,
                         testing::Values(UpdaterScope::kUser,
                                         UpdaterScope::kSystem));

TEST_P(InstallerAPITest, InstallerProgress) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));

  ClientStateAppKeyDelete(updater_scope_, kAppId);
  EXPECT_EQ(GetInstallerProgress(updater_scope_, kAppId), -1);
  SetInstallerProgressForTesting(updater_scope_, kAppId, 0);
  EXPECT_EQ(GetInstallerProgress(updater_scope_, kAppId), 0);
  SetInstallerProgressForTesting(updater_scope_, kAppId, 50);
  EXPECT_EQ(GetInstallerProgress(updater_scope_, kAppId), 50);
  SetInstallerProgressForTesting(updater_scope_, kAppId, 100);
  EXPECT_EQ(GetInstallerProgress(updater_scope_, kAppId), 100);
  SetInstallerProgressForTesting(updater_scope_, kAppId, 200);
  EXPECT_EQ(GetInstallerProgress(updater_scope_, kAppId), 100);
  EXPECT_TRUE(ClientStateAppKeyDelete(updater_scope_, kAppId));
}

TEST_P(InstallerAPITest, GetTextForSystemError) {
  EXPECT_FALSE(GetTextForSystemError(2).empty());
}

TEST_P(InstallerAPITest, GetInstallerOutcome) {
  ASSERT_NO_FATAL_FAILURE(
      registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));

  ClientStateAppKeyDelete(updater_scope_, kAppId);

  // No installer outcome if the ClientState for the app it does not exist.
  EXPECT_FALSE(GetInstallerOutcome(updater_scope_, kAppId));

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerResult::kSystemError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    EXPECT_TRUE(SetInstallerOutcomeForTesting(updater_scope_, kAppId,
                                              installer_outcome));
  }

  absl::optional<InstallerOutcome> installer_outcome =
      GetInstallerOutcome(updater_scope_, kAppId);
  ASSERT_TRUE(installer_outcome);
  EXPECT_EQ(installer_outcome->installer_result, InstallerResult::kSystemError);
  EXPECT_EQ(installer_outcome->installer_error, 1);
  EXPECT_EQ(installer_outcome->installer_extracode1, -2);
  EXPECT_STREQ(installer_outcome->installer_text->c_str(), "some text");
  EXPECT_STREQ(installer_outcome->installer_cmd_line->c_str(), "some cmd line");

  // No installer outcome values after clearing the installer outcome.
  EXPECT_TRUE(DeleteInstallerOutput(updater_scope_, kAppId));
  installer_outcome = GetInstallerOutcome(updater_scope_, kAppId);
  ASSERT_TRUE(installer_outcome);
  EXPECT_FALSE(installer_outcome->installer_result);
  EXPECT_FALSE(installer_outcome->installer_error);
  EXPECT_FALSE(installer_outcome->installer_extracode1);
  EXPECT_FALSE(installer_outcome->installer_text);
  EXPECT_FALSE(installer_outcome->installer_cmd_line);

  EXPECT_TRUE(ClientStateAppKeyDelete(updater_scope_, kAppId));
}

TEST_P(InstallerAPITest, MakeInstallerResult) {
  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerResult::kSuccess;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    const auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 0);
    EXPECT_EQ(installer_result.extended_error, 0);
    EXPECT_TRUE(installer_result.installer_text.empty());
    EXPECT_STREQ(installer_result.installer_cmd_line.c_str(), "some cmd line");
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerResult::kCustomError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 1);
    EXPECT_EQ(installer_result.extended_error, -2);
    EXPECT_STREQ(installer_result.installer_text.c_str(), "some text");
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
    installer_outcome.installer_error = absl::nullopt;
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 10);
    EXPECT_EQ(installer_result.extended_error, -2);
    EXPECT_STREQ(installer_result.installer_text.c_str(), "some text");
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerResult::kMsiError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 1);
    EXPECT_EQ(installer_result.extended_error, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
    installer_outcome.installer_error = absl::nullopt;
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 10);
    EXPECT_EQ(installer_result.extended_error, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerResult::kSystemError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 1);
    EXPECT_EQ(installer_result.extended_error, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
    installer_outcome.installer_error = absl::nullopt;
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 10);
    EXPECT_EQ(installer_result.extended_error, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerResult::kExitCode;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 0);
    EXPECT_EQ(installer_result.error, 0);
    EXPECT_EQ(installer_result.extended_error, 0);
    EXPECT_TRUE(installer_result.installer_text.empty());
    EXPECT_STREQ(installer_result.installer_cmd_line.c_str(), "some cmd line");
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.error, 10);
    EXPECT_EQ(installer_result.extended_error, 0);
    EXPECT_TRUE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }
}

}  // namespace updater
