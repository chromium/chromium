// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/auto_run_on_os_upgrade_task.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/test/unit_test_util_win.h"
#include "chrome/updater/util/win_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

constexpr wchar_t kAppId[] = L"{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}";
constexpr wchar_t kCmdId1[] = L"CreateOSVersionsFileOnOSUpgrade";
constexpr wchar_t kCmdLineCreateOSVersionsFile[] =
    L"/c \"echo %1 > %1 && exit 0\"";
constexpr wchar_t kCmdId2[] = L"CreateHardcodedFileOnOSUpgrade";
constexpr wchar_t kCmdLineCreateHardcodedFile[] =
    L"/c \"echo HardcodedFile > HardcodedFile && exit 0\"";
constexpr char kLastOSVersion[] = "last_os_version";

}  // namespace

class AutoRunOnOsUpgradeTaskTest : public testing::Test {
 protected:
  AutoRunOnOsUpgradeTaskTest() = default;
  ~AutoRunOnOsUpgradeTaskTest() override = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    update_client::RegisterPrefs(pref_service_->registry());
    RegisterPersistedDataPrefs(pref_service_->registry());
    persisted_data_ = base::MakeRefCounted<PersistedData>(
        GetUpdaterScopeForTesting(), pref_service_.get(), nullptr);
    test::SetupCmdExe(GetUpdaterScopeForTesting(), cmd_exe_command_line_,
                      temp_programfiles_dir_);
  }

  void TearDown() override {
    test::DeleteAppClientKey(GetUpdaterScopeForTesting(), kAppId);
  }

  void SetLastOSVersion(const OSVERSIONINFOEX& os_version) {
    EXPECT_TRUE(pref_service_);

    std::string encoded_os_version =
        base::Base64Encode(base::byte_span_from_ref(os_version));

    pref_service_->SetString(kLastOSVersion, encoded_os_version);
  }

  scoped_refptr<PersistedData> persisted_data_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::CommandLine cmd_exe_command_line_{base::CommandLine::NO_PROGRAM};
  base::ScopedTempDir temp_programfiles_dir_;
};

TEST_F(AutoRunOnOsUpgradeTaskTest, RunOnOsUpgradeForApp) {
  const std::optional<OSVERSIONINFOEX> current_os_version = GetOSVersion();
  ASSERT_NE(current_os_version, std::nullopt);
  OSVERSIONINFOEX last_os_version = current_os_version.value();
  --last_os_version.dwMajorVersion;

  // Sets a lower major version in the persisted data prefs to look like an OS
  // upgrade.
  SetLastOSVersion(last_os_version);

  auto os_upgrade_task = base::MakeRefCounted<AutoRunOnOsUpgradeTask>(
      GetUpdaterScopeForTesting(), persisted_data_);
  ASSERT_TRUE(os_upgrade_task->HasOSUpgraded());

  test::CreateAppCommandOSUpgradeRegistry(
      GetUpdaterScopeForTesting(), kAppId, kCmdId1,
      base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                    kCmdLineCreateOSVersionsFile}));
  test::CreateAppCommandOSUpgradeRegistry(
      GetUpdaterScopeForTesting(), kAppId, kCmdId2,
      base::StrCat({cmd_exe_command_line_.GetCommandLineString(), L" ",
                    kCmdLineCreateHardcodedFile}));

  ASSERT_EQ(os_upgrade_task->RunOnOsUpgradeForApp(base::WideToASCII(kAppId)),
            2U);

  const std::wstring os_upgrade_string = [&] {
    std::string versions;
    for (const auto& version : {last_os_version, current_os_version.value()}) {
      versions += base::StringPrintf(
          "%lu.%lu.%lu.%u.%u%s", version.dwMajorVersion, version.dwMinorVersion,
          version.dwBuildNumber, version.wServicePackMajor,
          version.wServicePackMinor, versions.empty() ? "-" : "");
    }
    return base::ASCIIToWide(versions);
  }();

  base::FilePath current_directory;
  base::PathService::Get(base::DIR_CURRENT, &current_directory);
  base::FilePath os_upgrade_file = current_directory.Append(os_upgrade_string);
  base::FilePath hardcoded_file = current_directory.Append(L"HardcodedFile");

  EXPECT_TRUE(test::WaitFor([&] {
    return base::PathExists(os_upgrade_file) &&
           base::PathExists(hardcoded_file);
  }));
  EXPECT_TRUE(base::DeleteFile(os_upgrade_file));
  EXPECT_TRUE(base::DeleteFile(hardcoded_file));
}

}  // namespace updater
