// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_internal.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;

namespace updater {

namespace {

class AppServerTest : public AppServer {
 public:
  AppServerTest() {
    ON_CALL(*this, ActiveDuty)
        .WillByDefault(Invoke(this, &AppServerTest::Shutdown0));
    ON_CALL(*this, ActiveDutyInternal)
        .WillByDefault(Invoke(this, &AppServerTest::Shutdown0));
  }

  MOCK_METHOD(void, ActiveDuty, (scoped_refptr<UpdateService>), (override));
  MOCK_METHOD(void,
              ActiveDutyInternal,
              (scoped_refptr<UpdateServiceInternal>),
              (override));
  MOCK_METHOD(bool, SwapInNewVersion, (), (override));
  MOCK_METHOD(void, RepairUpdater, (UpdaterScope, bool), (override));
  MOCK_METHOD(void, UninstallSelf, (), (override));
  MOCK_METHOD(bool, ShutdownIfIdleAfterTask, (), (override));
  MOCK_METHOD(void, OnDelayedTaskComplete, (), (override));

 protected:
  ~AppServerTest() override = default;

 private:
  UpdaterScope updater_scope() const override {
    return GetUpdaterScopeForTesting();
  }

  void Shutdown0() { Shutdown(0); }
};

void ClearPrefs() {
  const UpdaterScope updater_scope = GetUpdaterScopeForTesting();
  for (const std::optional<base::FilePath>& path :
       {GetInstallDirectory(updater_scope),
        GetVersionedInstallDirectory(updater_scope)}) {
    ASSERT_TRUE(path);
    ASSERT_TRUE(
        base::DeleteFile(path->Append(FILE_PATH_LITERAL("prefs.json"))));
  }
}

class AppServerTestCase : public testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    if (GetUpdaterScopeForTesting() == UpdaterScope::kSystem) {
      GTEST_SKIP();
    }
#endif  // BUILDFLAG(IS_MAC)
    ClearPrefs();
  }

 private:
  base::test::TaskEnvironment environment_;
};

}  // namespace

TEST_F(AppServerTestCase, SelfUninstall) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kServerServiceSwitch, kServerUpdateServiceInternalSwitchValue);
  {
    scoped_refptr<GlobalPrefs> global_prefs =
        CreateGlobalPrefs(GetUpdaterScopeForTesting());
    global_prefs->SetActiveVersion("9999999");
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    scoped_refptr<LocalPrefs> local_prefs =
        CreateLocalPrefs(GetUpdaterScopeForTesting());
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to ActiveDutyInternal then SelfUninstall.
  EXPECT_CALL(*app, ActiveDuty).Times(0);
  EXPECT_CALL(*app, ActiveDutyInternal).Times(1);
  EXPECT_CALL(*app, SwapInNewVersion).Times(0);
  EXPECT_CALL(*app, UninstallSelf).Times(1);
  EXPECT_EQ(app->Run(), 0);
  EXPECT_TRUE(CreateLocalPrefs(GetUpdaterScopeForTesting())->GetQualified());
}

TEST_F(AppServerTestCase, SelfPromote) {
  {
    scoped_refptr<LocalPrefs> local_prefs =
        CreateLocalPrefs(GetUpdaterScopeForTesting());
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  {
    auto app = base::MakeRefCounted<AppServerTest>();

    // Expect the app to SwapInNewVersion and then ActiveDuty then
    // Shutdown(0).
    EXPECT_CALL(*app, ActiveDuty).Times(1);
    EXPECT_CALL(*app, SwapInNewVersion).WillOnce(Return(true));
    EXPECT_CALL(*app, UninstallSelf).Times(0);
    EXPECT_EQ(app->Run(), 0);
  }
  scoped_refptr<GlobalPrefs> global_prefs =
      CreateGlobalPrefs(GetUpdaterScopeForTesting());
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), kUpdaterVersion);
}

TEST_F(AppServerTestCase, InstallAutoPromotes) {
  {
    auto app = base::MakeRefCounted<AppServerTest>();

    // Expect the app to SwapInNewVersion and then ActiveDuty then
    // Shutdown(0). In this case it bypasses qualification.
    EXPECT_CALL(*app, ActiveDuty).Times(1);
    EXPECT_CALL(*app, SwapInNewVersion).WillOnce(Return(true));
    EXPECT_CALL(*app, UninstallSelf).Times(0);
    EXPECT_EQ(app->Run(), 0);
    EXPECT_FALSE(CreateLocalPrefs(GetUpdaterScopeForTesting())->GetQualified());
  }
  scoped_refptr<GlobalPrefs> global_prefs =
      CreateGlobalPrefs(GetUpdaterScopeForTesting());
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), kUpdaterVersion);
}

TEST_F(AppServerTestCase, SelfPromoteFails) {
  {
    scoped_refptr<LocalPrefs> local_prefs =
        CreateLocalPrefs(GetUpdaterScopeForTesting());
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  {
    auto app = base::MakeRefCounted<AppServerTest>();

    // Expect the app to SwapInNewVersion and then Shutdown(2).
    EXPECT_CALL(*app, ActiveDuty).Times(0);
    EXPECT_CALL(*app, SwapInNewVersion).WillOnce(Return(false));
    EXPECT_CALL(*app, UninstallSelf).Times(0);
    EXPECT_EQ(app->Run(), kErrorFailedToSwap);
  }
  scoped_refptr<GlobalPrefs> global_prefs =
      CreateGlobalPrefs(GetUpdaterScopeForTesting());
  EXPECT_TRUE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), "0");
}

TEST_F(AppServerTestCase, ActiveDutyAlready) {
  {
    scoped_refptr<GlobalPrefs> global_prefs =
        CreateGlobalPrefs(GetUpdaterScopeForTesting());
    global_prefs->SetActiveVersion(kUpdaterVersion);
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    scoped_refptr<LocalPrefs> local_prefs =
        CreateLocalPrefs(GetUpdaterScopeForTesting());
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  {
    auto app = base::MakeRefCounted<AppServerTest>();

    // Expect the app to ActiveDuty and then Shutdown(0).
    EXPECT_CALL(*app, ActiveDuty).Times(1);
    EXPECT_CALL(*app, SwapInNewVersion).Times(0);
    EXPECT_CALL(*app, UninstallSelf).Times(0);
    EXPECT_EQ(app->Run(), 0);
  }
  scoped_refptr<GlobalPrefs> global_prefs =
      CreateGlobalPrefs(GetUpdaterScopeForTesting());
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), kUpdaterVersion);
}

TEST_F(AppServerTestCase, StateDirty) {
  {
    scoped_refptr<GlobalPrefs> global_prefs =
        CreateGlobalPrefs(GetUpdaterScopeForTesting());
    global_prefs->SetActiveVersion(kUpdaterVersion);
    global_prefs->SetSwapping(true);
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    scoped_refptr<LocalPrefs> local_prefs =
        CreateLocalPrefs(GetUpdaterScopeForTesting());
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  {
    auto app = base::MakeRefCounted<AppServerTest>();

    // Expect the app to SwapInNewVersion and then ActiveDuty and then
    // Shutdown(0).
    EXPECT_CALL(*app, ActiveDuty).Times(1);
    EXPECT_CALL(*app, SwapInNewVersion).WillOnce(Return(true));
    EXPECT_CALL(*app, UninstallSelf).Times(0);
    EXPECT_EQ(app->Run(), 0);
  }
  scoped_refptr<GlobalPrefs> global_prefs =
      CreateGlobalPrefs(GetUpdaterScopeForTesting());
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), kUpdaterVersion);
}

TEST_F(AppServerTestCase, StateDirtySwapFails) {
  {
    scoped_refptr<GlobalPrefs> global_prefs =
        CreateGlobalPrefs(GetUpdaterScopeForTesting());
    global_prefs->SetActiveVersion(kUpdaterVersion);
    global_prefs->SetSwapping(true);
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    scoped_refptr<LocalPrefs> local_prefs =
        CreateLocalPrefs(GetUpdaterScopeForTesting());
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  {
    auto app = base::MakeRefCounted<AppServerTest>();

    // Expect the app to SwapInNewVersion and Shutdown(2).
    EXPECT_CALL(*app, ActiveDuty).Times(0);
    EXPECT_CALL(*app, SwapInNewVersion).WillOnce(Return(false));
    EXPECT_CALL(*app, UninstallSelf).Times(0);
    EXPECT_EQ(app->Run(), kErrorFailedToSwap);
  }
  scoped_refptr<GlobalPrefs> global_prefs =
      CreateGlobalPrefs(GetUpdaterScopeForTesting());
  EXPECT_TRUE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), kUpdaterVersion);
}

}  // namespace updater
