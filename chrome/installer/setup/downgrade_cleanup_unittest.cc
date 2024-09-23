// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/downgrade_cleanup.h"

#include "base/command_line.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_reg_util_win.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace installer {

namespace {

constexpr wchar_t kVersion[] = L"80.0.0.0";
constexpr wchar_t kLastBreakingVersion[] = L"81.0.0.0";
constexpr char kDowngradeCleanupSuccessMain[] = "DowngradeCleanupSuccessMain";
constexpr wchar_t kDummyKey1[] = L"dummy1";
constexpr wchar_t kDummyKey2[] = L"dummy2";
constexpr int64_t kDummyValue1 = 2019;
constexpr int64_t kDummyValue2 = 2020;

}  // namespace

// Test fixture to verify that `AddDowngradeCleanupItems` launches the right
// processes with the required placeholder replacements. This mimics the command
// line returned by `GetDowngradeCleanupCommandWithPlaceholders`, but the
// process launched is a test process in which the command line arguments are
// verified
class DowngradeCleanupTest : public base::MultiProcessTest {
 protected:
  DowngradeCleanupTest() = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(reg_root()));
  }

  // Returns a commande line with the placeholders required by
  // `GetDowngradeCleanupCommandWithPlaceholders`.
  base::CommandLine MakeCmdLine(const std::string& procname) override {
    base::CommandLine command_line =
        base::MultiProcessTest::MakeCmdLine(procname);
    command_line.AppendSwitchNative(switches::kCleanupForDowngradeVersion,
                                    L"$1");
    command_line.AppendSwitchNative(switches::kCleanupForDowngradeOperation,
                                    L"$2");
    return command_line;
  }

  const wchar_t* client_state_key() const { return client_state_key_.c_str(); }

  // Adds a dummy work item before and after the cleanup work items. If
  // `expect_cleanup_items_added` is true, `AddDowngradeCleanupItems` must have
  // added some work items in `list`, otherwise it should not have added any
  // work items.
  void AddCleanupWorkItems(WorkItemList* list,
                           bool expect_cleanup_items_added) {
    list->AddSetRegValueWorkItem(reg_root(), client_state_key(),
                                 KEY_WOW64_32KEY, kDummyKey1, kDummyValue1,
                                 false);
    ASSERT_EQ(AddDowngradeCleanupItems(
                  base::Version(base::WideToASCII(kVersion)), list),
              expect_cleanup_items_added);
    list->AddSetRegValueWorkItem(reg_root(), client_state_key(),
                                 KEY_WOW64_32KEY, kDummyKey2, kDummyValue2,
                                 false);
  }

  void ExpectPreCleanupWorkItems(bool applied) {
    int64_t value = 0;
    base::win::RegKey(reg_root(), client_state_key(),
                      KEY_READ | KEY_WOW64_32KEY)
        .ReadInt64(kDummyKey1, &value);
    EXPECT_EQ(value, (applied ? kDummyValue1 : 0));
  }

  void ExpectPostCleanupWorkItems(bool applied) {
    int64_t expected_value = 0;
    base::win::RegKey(reg_root(), client_state_key(),
                      KEY_READ | KEY_WOW64_32KEY)
        .ReadInt64(kDummyKey2, &expected_value);
    EXPECT_EQ(expected_value, applied ? kDummyValue2 : 0);
  }

  static HKEY reg_root() {
    return install_static::IsSystemInstall() ? HKEY_LOCAL_MACHINE
                                             : HKEY_CURRENT_USER;
  }

 private:
  install_static::ScopedInstallDetails install_details_{true};
  registry_util::RegistryOverrideManager registry_override_manager_;
  const std::wstring client_state_key_ =
      install_static::GetClientStateKeyPath();
};

MULTIPROCESS_TEST_MAIN(DowngradeCleanupSuccessMain) {
  install_static::ScopedInstallDetails install_details(/*system_level=*/true);
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();

  auto version =
      command_line->GetSwitchValueNative(switches::kCleanupForDowngradeVersion);
  auto operation = command_line->GetSwitchValueNative(
      switches::kCleanupForDowngradeOperation);
  auto should_fail = command_line->HasSwitch("fail");

  installer::InstallStatus result_code = DOWNGRADE_CLEANUP_UNKNOWN_OPERATION;
  EXPECT_EQ(version, kVersion);
  if (operation == L"cleanup") {
    result_code = should_fail ? installer::DOWNGRADE_CLEANUP_FAILED
                              : installer::DOWNGRADE_CLEANUP_SUCCESS;
  } else if (operation == L"revert") {
    result_code = installer::UNDO_DOWNGRADE_CLEANUP_SUCCESS;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  return ::testing::Test::HasFailure() ? -1 : result_code;
}

TEST_F(DowngradeCleanupTest, SuccessfulCleanup) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());

  ASSERT_EQ(base::win::RegKey(reg_root(), client_state_key(),
                              KEY_SET_VALUE | KEY_WOW64_32KEY)
                .WriteValue(google_update::kRegDowngradeCleanupCommandField,
                            MakeCmdLine(kDowngradeCleanupSuccessMain)
                                .GetCommandLineString()
                                .c_str()),
            ERROR_SUCCESS);
  ASSERT_EQ(base::win::RegKey(reg_root(), client_state_key(),
                              KEY_SET_VALUE | KEY_WOW64_32KEY)
                .WriteValue(
                    google_update::kRegCleanInstallRequiredForVersionBelowField,
                    kLastBreakingVersion),
            ERROR_SUCCESS);
  ASSERT_NO_FATAL_FAILURE(
      AddCleanupWorkItems(list.get(), /*expect_cleanup_items_added=*/true));
  ASSERT_TRUE(list->Do());
  ExpectPreCleanupWorkItems(/*applied=*/true);
  ExpectPostCleanupWorkItems(/*applied=*/true);

  list->Rollback();
  ExpectPreCleanupWorkItems(/*applied=*/false);
  ExpectPostCleanupWorkItems(/*applied=*/false);
}

TEST_F(DowngradeCleanupTest, FailedCleanup) {
  std::unique_ptr<WorkItemList> list(WorkItem::CreateWorkItemList());
  auto cmd = MakeCmdLine(kDowngradeCleanupSuccessMain);
  cmd.AppendSwitch("fail");

  ASSERT_EQ(base::win::RegKey(reg_root(), client_state_key(),
                              KEY_SET_VALUE | KEY_WOW64_32KEY)
                .WriteValue(google_update::kRegDowngradeCleanupCommandField,
                            cmd.GetCommandLineString().c_str()),
            ERROR_SUCCESS);
  ASSERT_EQ(base::win::RegKey(reg_root(), client_state_key(),
                              KEY_SET_VALUE | KEY_WOW64_32KEY)
                .WriteValue(
                    google_update::kRegCleanInstallRequiredForVersionBelowField,
                    kLastBreakingVersion),
            ERROR_SUCCESS);
  ASSERT_NO_FATAL_FAILURE(
      AddCleanupWorkItems(list.get(), /*expect_cleanup_items_added=*/true));
  EXPECT_FALSE(list->Do());
  ExpectPreCleanupWorkItems(/*applied=*/true);
  ExpectPostCleanupWorkItems(/*applied=*/false);

  list->Rollback();
  ExpectPreCleanupWorkItems(/*applied=*/false);
  ExpectPostCleanupWorkItems(/*applied=*/false);
}

TEST_F(DowngradeCleanupTest, MissingArguments) {
  std::unique_ptr<WorkItemList> list;
  list.reset(WorkItem::CreateWorkItemList());
  auto cmd = MakeCmdLine(kDowngradeCleanupSuccessMain);
  // Currently only 2 placeholders are supported.
  cmd.AppendSwitchNative("missing", L"$3");

  ASSERT_EQ(base::win::RegKey(reg_root(), client_state_key(),
                              KEY_SET_VALUE | KEY_WOW64_32KEY)
                .WriteValue(google_update::kRegDowngradeCleanupCommandField,
                            cmd.GetCommandLineString().c_str()),
            ERROR_SUCCESS);
  ASSERT_EQ(base::win::RegKey(reg_root(), client_state_key(),
                              KEY_SET_VALUE | KEY_WOW64_32KEY)
                .WriteValue(
                    google_update::kRegCleanInstallRequiredForVersionBelowField,
                    kLastBreakingVersion),
            ERROR_SUCCESS);
  ASSERT_NO_FATAL_FAILURE(
      AddCleanupWorkItems(list.get(), /*expect_cleanup_items_added=*/false));
}

}  // namespace installer
