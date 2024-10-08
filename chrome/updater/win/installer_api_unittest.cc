// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/installer_api.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/win_util.h"
#include "components/update_client/update_client.h"
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
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(
      UpdaterScopeToHKeyRoot(updater_scope_)));

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

TEST_P(InstallerAPITest, GetInstallerOutcome) {
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(
      UpdaterScopeToHKeyRoot(updater_scope_)));

  ClientStateAppKeyDelete(updater_scope_, kAppId);

  // No installer outcome if the ClientState for the app it does not exist.
  EXPECT_FALSE(GetInstallerOutcome(updater_scope_, kAppId));
  EXPECT_FALSE(GetClientStateKeyLastInstallerOutcome(updater_scope_, kAppId));
  EXPECT_FALSE(GetUpdaterKeyLastInstallerOutcome(updater_scope_));

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerApiResult::kSystemError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    EXPECT_TRUE(SetInstallerOutcomeForTesting(updater_scope_, kAppId,
                                              installer_outcome));
  }

  std::optional<InstallerOutcome> installer_outcome =
      GetInstallerOutcome(updater_scope_, kAppId);
  ASSERT_TRUE(installer_outcome);
  EXPECT_EQ(installer_outcome->installer_result,
            InstallerApiResult::kSystemError);
  EXPECT_EQ(installer_outcome->installer_error, 1);
  EXPECT_EQ(installer_outcome->installer_extracode1, -2);
  EXPECT_STREQ(installer_outcome->installer_text->c_str(), "some text");
  EXPECT_STREQ(installer_outcome->installer_cmd_line->c_str(), "some cmd line");

  // Checks that LastInstallerXXX values match the installer outcome.
  for (std::optional<InstallerOutcome> last_installer_outcome :
       {GetClientStateKeyLastInstallerOutcome(updater_scope_, kAppId),
        GetUpdaterKeyLastInstallerOutcome(updater_scope_)}) {
    ASSERT_TRUE(last_installer_outcome);
    EXPECT_EQ(last_installer_outcome->installer_result,
              installer_outcome->installer_result);
    EXPECT_EQ(last_installer_outcome->installer_error,
              installer_outcome->installer_error);
    EXPECT_EQ(last_installer_outcome->installer_extracode1,
              installer_outcome->installer_extracode1);
    EXPECT_EQ(*last_installer_outcome->installer_text,
              *installer_outcome->installer_text);
    EXPECT_EQ(*last_installer_outcome->installer_cmd_line,
              *installer_outcome->installer_cmd_line);
  }

  // Checks that the previous call to `GetInstallerOutcome` cleared the
  // installer outcome.
  installer_outcome = GetInstallerOutcome(updater_scope_, kAppId);
  ASSERT_TRUE(installer_outcome);
  EXPECT_FALSE(installer_outcome->installer_result);
  EXPECT_FALSE(installer_outcome->installer_error);
  EXPECT_FALSE(installer_outcome->installer_extracode1);
  EXPECT_FALSE(installer_outcome->installer_text);
  EXPECT_FALSE(installer_outcome->installer_cmd_line);

  {
    InstallerOutcome installer_outcome_for_deletion;
    installer_outcome_for_deletion.installer_result =
        InstallerApiResult::kSystemError;
    installer_outcome_for_deletion.installer_error = 1;
    installer_outcome_for_deletion.installer_extracode1 = -2;
    installer_outcome_for_deletion.installer_text = "some text";
    installer_outcome_for_deletion.installer_cmd_line = "some cmd line";
    EXPECT_TRUE(SetInstallerOutcomeForTesting(updater_scope_, kAppId,
                                              installer_outcome_for_deletion));
  }

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
    installer_outcome.installer_result = InstallerApiResult::kSuccess;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    const auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kNone);
    EXPECT_EQ(installer_result.result.code_, 0);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_TRUE(installer_result.installer_text.empty());
    EXPECT_STREQ(installer_result.installer_cmd_line.c_str(), "some cmd line");
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerApiResult::kCustomError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 1);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_STREQ(installer_result.installer_text.c_str(), "some text");
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
    installer_outcome.installer_error = std::nullopt;
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 10);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_STREQ(installer_result.installer_text.c_str(), "some text");
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerApiResult::kMsiError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 1);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
    installer_outcome.installer_error = std::nullopt;
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 10);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerApiResult::kSystemError;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 1);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
    installer_outcome.installer_error = std::nullopt;
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 10);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_FALSE(installer_result.installer_text.empty());
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }

  {
    InstallerOutcome installer_outcome;
    installer_outcome.installer_result = InstallerApiResult::kExitCode;
    installer_outcome.installer_error = 1;
    installer_outcome.installer_extracode1 = -2;
    installer_outcome.installer_text = "some text";
    installer_outcome.installer_cmd_line = "some cmd line";
    auto installer_result = MakeInstallerResult(installer_outcome, 0);

    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 1);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_EQ(installer_result.installer_text, "some text");
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());

    // `installer_outcome` overrides the exit code.
    installer_result = MakeInstallerResult(installer_outcome, 10);
    EXPECT_EQ(installer_result.result.category_,
              update_client::ErrorCategory::kInstaller);
    EXPECT_EQ(installer_result.result.code_, 1);
    EXPECT_EQ(installer_result.result.extra_, -2);
    EXPECT_EQ(installer_result.installer_text, "some text");
    EXPECT_TRUE(installer_result.installer_cmd_line.empty());
  }
}

TEST_P(InstallerAPITest, ClientStateAppKeyOpen) {
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(
      UpdaterScopeToHKeyRoot(updater_scope_)));
  EXPECT_FALSE(
      ClientStateAppKeyOpen(updater_scope_, "invalid-app-id", KEY_READ));
  SetInstallerProgressForTesting(updater_scope_, kAppId, 0);
  EXPECT_TRUE(ClientStateAppKeyOpen(updater_scope_, kAppId, KEY_READ));
}

TEST_P(InstallerAPITest, LookupVersionMissing) {
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(
      UpdaterScopeToHKeyRoot(updater_scope_)));
  base::Version default_version = base::Version("1.1.1.1");
  base::Version version =
      LookupVersion(updater_scope_, "{4e346bdc-c3d1-460e-83d7-31555eef96c7}",
                    base::FilePath(), "", default_version);
  EXPECT_EQ(default_version, version);
}

TEST_P(InstallerAPITest, LookupVersionInvalid) {
  HKEY root = UpdaterScopeToHKeyRoot(updater_scope_);
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(root));

  base::win::RegKey key;
  ASSERT_EQ(
      key.Create(root,
                 UPDATER_KEY L"Clients\\{4e346bdc-c3d1-460e-83d7-31555eef96c7}",
                 Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValuePV, L"invalid"), ERROR_SUCCESS);

  base::Version default_version = base::Version("1.1.1.1");
  base::Version version =
      LookupVersion(updater_scope_, "{4e346bdc-c3d1-460e-83d7-31555eef96c7}",
                    base::FilePath(), "", default_version);
  EXPECT_EQ(default_version, version);
}

TEST_P(InstallerAPITest, LookupVersionValid) {
  HKEY root = UpdaterScopeToHKeyRoot(updater_scope_);
  ASSERT_NO_FATAL_FAILURE(registry_override_.OverrideRegistry(root));

  base::win::RegKey key;
  ASSERT_EQ(
      key.Create(root,
                 UPDATER_KEY L"Clients\\{4e346bdc-c3d1-460e-83d7-31555eef96c7}",
                 Wow6432(KEY_WRITE)),
      ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kRegValuePV, L"1.1.1.2"), ERROR_SUCCESS);

  base::Version default_version = base::Version("1.1.1.1");
  base::Version version =
      LookupVersion(updater_scope_, "{4e346bdc-c3d1-460e-83d7-31555eef96c7}",
                    base::FilePath(), "", default_version);
  EXPECT_EQ(version, base::Version("1.1.1.2"));
}

}  // namespace updater
