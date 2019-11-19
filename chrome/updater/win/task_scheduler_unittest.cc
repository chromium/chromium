// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/task_scheduler.h"

#include <taskschd.h>

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "chrome/updater/win/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

namespace {

// The name of the tasks as will be visible in the scheduler so we know we can
// safely delete them if they get stuck for whatever reason.
const wchar_t kTaskName1[] = L"Chrome Updater Test task 1 (delete me)";
const wchar_t kTaskName2[] = L"Chrome Updater Test task 2 (delete me)";
// Optional descriptions for the above tasks.
const wchar_t kTaskDescription1[] =
    L"Task 1 used only for Chrome Updater unit testing.";
const wchar_t kTaskDescription2[] =
    L"Task 2 used only for Chrome Updater unit testing.";
// A command-line switch used in testing.
const char kTestSwitch[] = "a_switch";

class TaskSchedulerTests : public testing::Test {
 public:
  void SetUp() override {
    task_scheduler_ = TaskScheduler::CreateInstance();
    // In case previous tests failed and left these tasks in the scheduler.
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
    ASSERT_FALSE(IsProcessRunning(kTestProcessExecutableName));
  }

  void TearDown() override {
    // Make sure to not leave tasks behind.
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
    // Make sure every processes launched with scheduled task are completed.
    ASSERT_TRUE(WaitForProcessesStopped(kTestProcessExecutableName));
  }

 protected:
  std::unique_ptr<TaskScheduler> task_scheduler_;
};

}  // namespace

TEST_F(TaskSchedulerTests, DeleteAndIsRegistered) {
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));

  // Construct the full-path of the test executable.
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));

  // Validate that the task is properly seen as registered when it is.
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_NOW, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  // Validate that a task with a similar name is not seen as registered.
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName2));

  // While the first one is still seen as registered, until it gets deleted.
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));
  // The other task should still not be registered.
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName2));
}

TEST_F(TaskSchedulerTests, RunAProgramNow) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));

  // Create a unique name for a shared event to be waited for in this process
  // and signaled in the test process to confirm it was scheduled and ran.
  const base::string16 event_name =
      base::StrCat({kTestProcessExecutableName, L"-",
                    base::NumberToString16(::GetCurrentProcessId())});
  base::WaitableEvent event(base::win::ScopedHandle(
      ::CreateEvent(nullptr, FALSE, FALSE, event_name.c_str())));
  ASSERT_NE(event.handle(), nullptr);

  command_line.AppendSwitchNative(kTestEventToSignal, event_name);
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_NOW, false));
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
  base::Time next_run_time;
  EXPECT_FALSE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

TEST_F(TaskSchedulerTests, Hourly) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));

  base::Time now(base::Time::NowFromSystemTime());
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  base::TimeDelta one_hour(base::TimeDelta::FromHours(1));
  base::TimeDelta one_minute(base::TimeDelta::FromMinutes(1));

  base::Time next_run_time;
  EXPECT_TRUE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
  EXPECT_LT(next_run_time, now + one_hour + one_minute);
  EXPECT_GT(next_run_time, now + one_hour - one_minute);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_FALSE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
}

TEST_F(TaskSchedulerTests, EveryFiveHours) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));

  base::Time now(base::Time::NowFromSystemTime());
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      kTaskName1, kTaskDescription1, command_line,
      TaskScheduler::TRIGGER_TYPE_EVERY_FIVE_HOURS, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  base::TimeDelta six_hours(base::TimeDelta::FromHours(5));
  base::TimeDelta one_minute(base::TimeDelta::FromMinutes(1));

  base::Time next_run_time;
  EXPECT_TRUE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
  EXPECT_LT(next_run_time, now + six_hours + one_minute);
  EXPECT_GT(next_run_time, now + six_hours - one_minute);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_FALSE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
}

TEST_F(TaskSchedulerTests, SetTaskEnabled) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));

  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_TRUE(task_scheduler_->IsTaskEnabled(kTaskName1));

  EXPECT_TRUE(task_scheduler_->SetTaskEnabled(kTaskName1, true));
  EXPECT_TRUE(task_scheduler_->IsTaskEnabled(kTaskName1));
  EXPECT_TRUE(task_scheduler_->SetTaskEnabled(kTaskName1, false));
  EXPECT_FALSE(task_scheduler_->IsTaskEnabled(kTaskName1));
  EXPECT_TRUE(task_scheduler_->SetTaskEnabled(kTaskName1, true));
  EXPECT_TRUE(task_scheduler_->IsTaskEnabled(kTaskName1));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

TEST_F(TaskSchedulerTests, GetTaskNameList) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));

  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName2, kTaskDescription2, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  std::vector<base::string16> task_names;
  EXPECT_TRUE(task_scheduler_->GetTaskNameList(&task_names));
  EXPECT_TRUE(base::Contains(task_names, kTaskName1));
  EXPECT_TRUE(base::Contains(task_names, kTaskName2));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
}

TEST_F(TaskSchedulerTests, GetTasksIncludesHidden) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line(
      executable_path.Append(kTestProcessExecutableName));

  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, true));

  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  std::vector<base::string16> task_names;
  EXPECT_TRUE(task_scheduler_->GetTaskNameList(&task_names));
  EXPECT_TRUE(base::Contains(task_names, kTaskName1));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

TEST_F(TaskSchedulerTests, GetTaskInfoExecActions) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line1(
      executable_path.Append(kTestProcessExecutableName));

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      kTaskName1, kTaskDescription1, command_line1,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  EXPECT_EQ(0UL, info.exec_actions.size());
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));
  ASSERT_EQ(1UL, info.exec_actions.size());
  EXPECT_EQ(command_line1.GetProgram(), info.exec_actions[0].application_path);
  EXPECT_EQ(command_line1.GetArgumentsString(), info.exec_actions[0].arguments);

  base::CommandLine command_line2(
      executable_path.Append(kTestProcessExecutableName));
  command_line2.AppendSwitch(kTestSwitch);
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      kTaskName2, kTaskDescription2, command_line2,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  // The |info| struct is re-used to ensure that new task information overwrites
  // the previous contents of the struct.
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  ASSERT_EQ(1UL, info.exec_actions.size());
  EXPECT_EQ(command_line2.GetProgram(), info.exec_actions[0].application_path);
  EXPECT_EQ(command_line2.GetArgumentsString(), info.exec_actions[0].arguments);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
}

TEST_F(TaskSchedulerTests, GetTaskInfoNameAndDescription) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line1(
      executable_path.Append(kTestProcessExecutableName));

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      kTaskName1, kTaskDescription1, command_line1,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  EXPECT_EQ(L"", info.description);
  EXPECT_EQ(L"", info.name);

  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));
  EXPECT_EQ(kTaskDescription1, info.description);
  EXPECT_EQ(kTaskName1, info.name);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

TEST_F(TaskSchedulerTests, GetTaskInfoLogonType) {
  base::FilePath executable_path;
  ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &executable_path));
  base::CommandLine command_line1(
      executable_path.Append(kTestProcessExecutableName));

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      kTaskName1, kTaskDescription1, command_line1,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  EXPECT_EQ(0U, info.logon_type);
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));
  EXPECT_TRUE(info.logon_type & TaskScheduler::LOGON_INTERACTIVE);
  EXPECT_FALSE(info.logon_type & TaskScheduler::LOGON_SERVICE);
  EXPECT_FALSE(info.logon_type & TaskScheduler::LOGON_S4U);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

}  // namespace updater
