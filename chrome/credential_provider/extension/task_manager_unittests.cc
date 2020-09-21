// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/run_loop.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/credential_provider/extension/extension_strings.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

namespace testing {

class TaskManagerTest : public ::testing::Test {
 public:
  TaskManagerTest() {}

  void RunTasks() {
    fake_task_manager_.RunTasks(task_environment_.GetMainThreadTaskRunner());
  }

  void SetUp() override {
    registry_override.OverrideRegistry(HKEY_LOCAL_MACHINE);
  }

  void TearDown() override { fake_task_manager_.Quit(); }

  FakeTaskManager* fake_task_manager() { return &fake_task_manager_; }
  base::test::SingleThreadTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  FakeTaskManager fake_task_manager_;
  registry_util::RegistryOverrideManager registry_override;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TaskManagerTest, PeriodicExecution) {
  ASSERT_EQ(
      GetGlobalFlagOrDefault(
          credential_provider::extension::kLastPeriodicSyncTimeRegKey, L""),
      L"");

  RunTasks();

  task_environment()->FastForwardBy(base::TimeDelta::FromHours(5));

  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(), 2);

  ASSERT_NE(
      GetGlobalFlagOrDefault(
          credential_provider::extension::kLastPeriodicSyncTimeRegKey, L""),
      L"");
  task_environment()->FastForwardBy(base::TimeDelta::FromHours(2));

  ASSERT_EQ(fake_task_manager()->NumOfTimesExecuted(), 3);
}

}  // namespace testing
}  // namespace credential_provider
