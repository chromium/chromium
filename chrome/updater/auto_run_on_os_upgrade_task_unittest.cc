// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/auto_run_on_os_upgrade_task.h"

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/unittest_util_win.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/update_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {
constexpr wchar_t kAppId[] = L"{3B1A3CCA-0525-4418-93E6-A0DB3398EC9B}";
constexpr wchar_t kCmdId[] = L"CreateHardcodedFile1OnOSUpgrade";
constexpr wchar_t kCmdLineEcho[] = L" /c \"echo Hello World && exit 0\"";
};  // namespace

class AutoRunOnOsUpgradeTaskTest : public testing::Test {
 protected:
  AutoRunOnOsUpgradeTaskTest() = default;
  ~AutoRunOnOsUpgradeTaskTest() override = default;

  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    update_client::RegisterPrefs(pref_service_->registry());
    RegisterPersistedDataPrefs(pref_service_->registry());
    persisted_data_ = base::MakeRefCounted<PersistedData>(pref_service_.get());
    SetupCmdExe(GetTestScope(), cmd_exe_command_line_, temp_programfiles_dir_);
  }

  void TearDown() override { DeleteAppClientKey(GetTestScope(), kAppId); }

  scoped_refptr<PersistedData> persisted_data_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::CommandLine cmd_exe_command_line_{base::CommandLine::NO_PROGRAM};
  base::ScopedTempDir temp_programfiles_dir_;
};

TEST_F(AutoRunOnOsUpgradeTaskTest, RunOnOsUpgradeForApp) {
  CreateAppCommandOSUpgradeRegistry(
      GetTestScope(), kAppId, kCmdId,
      base::StrCat(
          {cmd_exe_command_line_.GetCommandLineString(), kCmdLineEcho}));

  ASSERT_EQ(base::MakeRefCounted<AutoRunOnOsUpgradeTask>(GetTestScope(),
                                                         persisted_data_)
                ->RunOnOsUpgradeForApp(base::WideToASCII(kAppId)),
            1U);
}

}  // namespace updater
