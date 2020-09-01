// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_server.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util.h"
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
    ON_CALL(*this, UninstallSelf)
        .WillByDefault(Invoke(this, &AppServerTest::Shutdown0));
  }

  MOCK_METHOD(void, ActiveDuty, (), (override));
  MOCK_METHOD(bool, SwapRPCInterfaces, (), (override));
  MOCK_METHOD(void, UninstallSelf, (), (override));

 protected:
  ~AppServerTest() override = default;

 private:
  void InitializeThreadPool() override {
    // Do nothing, the test has already created the thread pool.
  }

  void Shutdown0() { Shutdown(0); }
};

void ClearPrefs() {
  base::FilePath prefs_dir;
  ASSERT_TRUE(GetBaseDirectory(&prefs_dir));
  ASSERT_TRUE(
      base::DeleteFile(prefs_dir.Append(FILE_PATH_LITERAL("prefs.json"))));
  ASSERT_TRUE(GetVersionedDirectory(&prefs_dir));
  ASSERT_TRUE(
      base::DeleteFile(prefs_dir.Append(FILE_PATH_LITERAL("prefs.json"))));
}

class AppServerTestCase : public testing::Test {
 public:
  AppServerTestCase() : main_task_executor_(base::MessagePumpType::UI) {}
  ~AppServerTestCase() override = default;

  void SetUp() override {
    base::ThreadPoolInstance::CreateAndStartWithDefaultParams("test");
    ClearPrefs();
  }

  void TearDown() override {
    base::ThreadPoolInstance::Get()->JoinForTesting();
    base::ThreadPoolInstance::Set(nullptr);
  }

 private:
  base::SingleThreadTaskExecutor main_task_executor_;
};

}  // namespace

TEST_F(AppServerTestCase, SimpleQualify) {
  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    global_prefs->SetActiveVersion("0.0.0.1");
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to qualify and then shutdown.
  EXPECT_CALL(*app, ActiveDuty).Times(0);
  EXPECT_CALL(*app, SwapRPCInterfaces).Times(0);
  EXPECT_CALL(*app, UninstallSelf).Times(0);
  EXPECT_EQ(app->Run(), kErrorQualificationExit);
  EXPECT_TRUE(CreateLocalPrefs()->GetQualified());
}

TEST_F(AppServerTestCase, SelfUninstall) {
  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    global_prefs->SetActiveVersion("9999999");
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    std::unique_ptr<LocalPrefs> local_prefs = CreateLocalPrefs();
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to SelfUninstall and then Shutdown(0).
  EXPECT_CALL(*app, ActiveDuty).Times(0);
  EXPECT_CALL(*app, SwapRPCInterfaces).Times(0);
  EXPECT_CALL(*app, UninstallSelf).Times(1);
  EXPECT_EQ(app->Run(), 0);
  EXPECT_TRUE(CreateLocalPrefs()->GetQualified());
}

TEST_F(AppServerTestCase, SelfPromote) {
  {
    std::unique_ptr<LocalPrefs> local_prefs = CreateLocalPrefs();
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to SwapRpcInterfaces and then ActiveDuty then Shutdown(0).
  EXPECT_CALL(*app, ActiveDuty).Times(1);
  EXPECT_CALL(*app, SwapRPCInterfaces).WillOnce(Return(true));
  EXPECT_CALL(*app, UninstallSelf).Times(0);
  EXPECT_EQ(app->Run(), 0);
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), UPDATER_VERSION_STRING);
}

TEST_F(AppServerTestCase, InstallAutoPromotes) {
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to SwapRpcInterfaces and then ActiveDuty then Shutdown(0).
  // In this case it bypasses qualification.
  EXPECT_CALL(*app, ActiveDuty).Times(1);
  EXPECT_CALL(*app, SwapRPCInterfaces).WillOnce(Return(true));
  EXPECT_CALL(*app, UninstallSelf).Times(0);
  EXPECT_EQ(app->Run(), 0);
  EXPECT_FALSE(CreateLocalPrefs()->GetQualified());
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), UPDATER_VERSION_STRING);
}

TEST_F(AppServerTestCase, SelfPromoteFails) {
  {
    std::unique_ptr<LocalPrefs> local_prefs = CreateLocalPrefs();
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to SwapRpcInterfaces and then Shutdown(2).
  EXPECT_CALL(*app, ActiveDuty).Times(0);
  EXPECT_CALL(*app, SwapRPCInterfaces).WillOnce(Return(false));
  EXPECT_CALL(*app, UninstallSelf).Times(0);
  EXPECT_EQ(app->Run(), 2);
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  EXPECT_TRUE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), "0");
}

TEST_F(AppServerTestCase, ActiveDutyAlready) {
  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    global_prefs->SetActiveVersion(UPDATER_VERSION_STRING);
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    std::unique_ptr<LocalPrefs> local_prefs = CreateLocalPrefs();
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to ActiveDuty and then Shutdown(0).
  EXPECT_CALL(*app, ActiveDuty).Times(1);
  EXPECT_CALL(*app, SwapRPCInterfaces).Times(0);
  EXPECT_CALL(*app, UninstallSelf).Times(0);
  EXPECT_EQ(app->Run(), 0);
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), UPDATER_VERSION_STRING);
}

TEST_F(AppServerTestCase, StateDirty) {
  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    global_prefs->SetActiveVersion(UPDATER_VERSION_STRING);
    global_prefs->SetSwapping(true);
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    std::unique_ptr<LocalPrefs> local_prefs = CreateLocalPrefs();
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to SwapRpcInterfaces and then ActiveDuty and then
  // Shutdown(0).
  EXPECT_CALL(*app, ActiveDuty).Times(1);
  EXPECT_CALL(*app, SwapRPCInterfaces).WillOnce(Return(true));
  EXPECT_CALL(*app, UninstallSelf).Times(0);
  EXPECT_EQ(app->Run(), 0);
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  EXPECT_FALSE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), UPDATER_VERSION_STRING);
}

TEST_F(AppServerTestCase, StateDirtySwapFails) {
  {
    std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
    global_prefs->SetActiveVersion(UPDATER_VERSION_STRING);
    global_prefs->SetSwapping(true);
    PrefsCommitPendingWrites(global_prefs->GetPrefService());
    std::unique_ptr<LocalPrefs> local_prefs = CreateLocalPrefs();
    local_prefs->SetQualified(true);
    PrefsCommitPendingWrites(local_prefs->GetPrefService());
  }
  auto app = base::MakeRefCounted<AppServerTest>();

  // Expect the app to SwapRpcInterfaces and Shutdown(2).
  EXPECT_CALL(*app, ActiveDuty).Times(0);
  EXPECT_CALL(*app, SwapRPCInterfaces).WillOnce(Return(false));
  EXPECT_CALL(*app, UninstallSelf).Times(0);
  EXPECT_EQ(app->Run(), 2);
  std::unique_ptr<GlobalPrefs> global_prefs = CreateGlobalPrefs();
  EXPECT_TRUE(global_prefs->GetSwapping());
  EXPECT_EQ(global_prefs->GetActiveVersion(), UPDATER_VERSION_STRING);
}

}  // namespace updater
