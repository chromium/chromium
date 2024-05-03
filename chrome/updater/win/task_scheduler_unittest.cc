// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/task_scheduler.h"

#include <lmsname.h>
#include <mstask.h>
#include <security.h>
#include <shlobj.h>
#include <taskschd.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {
namespace {

// The name of the tasks as will be visible in the scheduler so we know we can
// safely delete them if they get stuck for whatever reason.
const wchar_t kTaskName1[] = L"Chrome Updater Test task 1 (delete me)";
const wchar_t kTaskName2[] = L"Chrome Updater Test task 2 (delete me)";

const wchar_t kPrefixTaskName1[] = L"Chrome Updater Test task 1";
const wchar_t kPrefixTaskName2[] = L"Chrome Updater Test task 2";

// Optional descriptions for the tasks above.
const wchar_t kTaskDescription1[] =
    L"Task 1 used only for Chrome Updater unit testing.";
const wchar_t kTaskDescription2[] =
    L"Task 2 used only for Chrome Updater unit testing.";

// A command-line switch used in testing.
const char kUnitTestSwitch[] = "a_switch";

class TaskSchedulerTests : public ::testing::Test {
 public:
  void SetUp() override {
    task_scheduler_ =
        TaskScheduler::CreateInstance(GetUpdaterScopeForTesting());
    ASSERT_TRUE(task_scheduler_);

    EXPECT_TRUE(IsServiceRunning(SERVICE_SCHEDULE));
    ASSERT_TRUE(test::KillProcesses(kTestProcessExecutableName, 0))
        << test::PrintProcesses(kTestProcessExecutableName);
    ASSERT_FALSE(test::IsProcessRunning(kTestProcessExecutableName));
  }

  void TearDown() override {
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1))
        << GetRegKeyTaskCacheTasksContents();
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2))
        << GetRegKeyTaskCacheTasksContents();
    EXPECT_FALSE(test::IsProcessRunning(kTestProcessExecutableName))
        << test::PrintProcesses(kTestProcessExecutableName);
    EXPECT_TRUE(test::KillProcesses(kTestProcessExecutableName, 0));
  }

  std::wstring GetRegKeyTaskCacheTasksContents() {
    std::optional<std::wstring> contents = GetRegKeyContents(
        L"HKLM\\SOFTWARE\\Microsoft\\Windows "
        L"NT\\CurrentVersion\\Schedule\\TaskCache\\Tasks");
    return contents ? *contents : L"";
  }

  void ExpectRegisterTaskSucceeds(const std::wstring& task_name,
                                  const std::wstring& task_description,
                                  const base::CommandLine& run_command,
                                  int trigger_types,
                                  bool hidden) {
    EXPECT_TRUE(task_scheduler_->RegisterTask(
        task_name, task_description, run_command, trigger_types, hidden))
        << GetRegKeyTaskCacheTasksContents();
  }

  // Converts a base::Time that is in UTC and returns the corresponding local
  // time on the current system.
  base::Time UTCTimeToLocalTime(const base::Time& time_utc) {
    const FILETIME file_time_utc = time_utc.ToFileTime();
    FILETIME file_time_local = {};
    SYSTEMTIME system_time_utc = {};
    SYSTEMTIME system_time_local = {};

    // We do not use ::FileTimeToLocalFileTime, since it uses the current
    // settings for the time zone and daylight saving time, instead of the
    // settings at the time of `time_utc`.
    if (!::FileTimeToSystemTime(&file_time_utc, &system_time_utc) ||
        !::SystemTimeToTzSpecificLocalTime(nullptr, &system_time_utc,
                                           &system_time_local) ||
        !::SystemTimeToFileTime(&system_time_local, &file_time_local)) {
      return base::Time();
    }
    return base::Time::FromFileTime(file_time_local);
  }

  void RunTaskTest(TaskScheduler::TriggerType trigger_type) {
    base::CommandLine command_line = GetTestProcessCommandLine(
        GetUpdaterScopeForTesting(), test::GetTestName());

    // Create a unique name for a shared event to be waited for in this process
    // and signaled in the test process to confirm it was scheduled and ran.
    test::EventHolder event_holder(test::CreateWaitableEventForTest());

    command_line.AppendSwitchNative(kTestEventToSignal, event_holder.name);
    ExpectRegisterTaskSucceeds(kTaskName1, kTaskDescription1, command_line,
                               trigger_type, false);

    // Check that the created task matches the trigger it was created with.
    TaskScheduler::TaskInfo info;
    EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
    EXPECT_EQ(info.trigger_types, trigger_type);

    if (trigger_type != TaskScheduler::TRIGGER_TYPE_NOW) {
      EXPECT_TRUE(task_scheduler_->StartTask(kTaskName1));
    }

    VLOG(0) << [this] {
      TaskScheduler::TaskInfo info;
      EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
      return info;
    }();

    EXPECT_TRUE(
        event_holder.event.TimedWait(TestTimeouts::action_max_timeout()));
    EXPECT_TRUE(test::WaitFor(
        [&] { return !task_scheduler_->IsTaskRunning(kTaskName1); }));

    if (trigger_type == TaskScheduler::TRIGGER_TYPE_NOW) {
      base::Time next_run_time;
      EXPECT_FALSE(
          task_scheduler_->GetNextTaskRunTime(kTaskName1, next_run_time));
    }

    test::PrintLog(GetUpdaterScopeForTesting());
  }

  void RunGetTaskInfoTriggerTypesTest(int expected_trigger_types) {
    base::CommandLine command_line = GetTestProcessCommandLine(
        GetUpdaterScopeForTesting(), test::GetTestName());

    ExpectRegisterTaskSucceeds(kTaskName1, kTaskDescription1, command_line,
                               expected_trigger_types, false);
    TaskScheduler::TaskInfo info;
    EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
    EXPECT_EQ(info.trigger_types, expected_trigger_types);
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  }

 protected:
  scoped_refptr<TaskScheduler> task_scheduler_;
};

}  // namespace

TEST_F(TaskSchedulerTests, DeleteAndIsRegistered) {
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));

  // Construct the full-path of the test executable.
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  // Validate that the task is properly seen as registered when it is.
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
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
  RunTaskTest(TaskScheduler::TRIGGER_TYPE_NOW);
}

TEST_F(TaskSchedulerTests, StartTask) {
  RunTaskTest(TaskScheduler::TRIGGER_TYPE_HOURLY);
}

TEST_F(TaskSchedulerTests, Hourly) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  base::Time now(base::Time::NowFromSystemTime());
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  base::Time next_run_time;
  EXPECT_TRUE(task_scheduler_->GetNextTaskRunTime(kTaskName1, next_run_time));

  // Check that the task starts approximately 5 minutes from the current time.
  EXPECT_GT(next_run_time, UTCTimeToLocalTime(now + base::Minutes(3)));
  EXPECT_LT(next_run_time, UTCTimeToLocalTime(now + base::Minutes(7)));

  // Check that the task has a hourly trigger.
  TaskScheduler::TaskInfo info;
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
  EXPECT_EQ(info.trigger_types, TaskScheduler::TRIGGER_TYPE_HOURLY);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_FALSE(task_scheduler_->GetNextTaskRunTime(kTaskName1, next_run_time));
}

TEST_F(TaskSchedulerTests, EveryFiveHours) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  base::Time now(base::Time::NowFromSystemTime());
  ExpectRegisterTaskSucceeds(kTaskName1, kTaskDescription1, command_line,
                             TaskScheduler::TRIGGER_TYPE_EVERY_FIVE_HOURS,
                             false);
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  base::Time next_run_time;
  EXPECT_TRUE(task_scheduler_->GetNextTaskRunTime(kTaskName1, next_run_time));

  // Check that the task starts approximately 5 minutes from the current time.
  EXPECT_GT(next_run_time, UTCTimeToLocalTime(now + base::Minutes(3)));
  EXPECT_LT(next_run_time, UTCTimeToLocalTime(now + base::Minutes(7)));

  // Check that the task has a five hour trigger.
  TaskScheduler::TaskInfo info;
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
  EXPECT_EQ(info.trigger_types, TaskScheduler::TRIGGER_TYPE_EVERY_FIVE_HOURS);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_FALSE(task_scheduler_->GetNextTaskRunTime(kTaskName1, next_run_time));
}

TEST_F(TaskSchedulerTests, SetTaskEnabled) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

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
}

TEST_F(TaskSchedulerTests, IsTaskRunning) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  // Create a unique name for a shared event to be waited for in the task and
  // signaled in this test.
  test::EventHolder event_holder(test::CreateWaitableEventForTest());

  command_line.AppendSwitchNative(kTestEventToWaitOn, event_holder.name);
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_NOW, false));

  EXPECT_TRUE(test::WaitFor(
      [&] { return task_scheduler_->IsTaskRunning(kTaskName1); }));
  EXPECT_EQ(test::FindProcesses(kTestProcessExecutableName).size(), 1U);

  event_holder.event.Signal();

  EXPECT_TRUE(test::WaitFor(
      [&] { return !task_scheduler_->IsTaskRunning(kTaskName1); }));
  EXPECT_TRUE(test::FindProcesses(kTestProcessExecutableName).empty());
}

TEST_F(TaskSchedulerTests, GetTaskNameList) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName2, kTaskDescription2, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  std::vector<std::wstring> task_names;
  EXPECT_TRUE(task_scheduler_->GetTaskNameList(task_names));
  EXPECT_TRUE(base::Contains(task_names, kTaskName1));
  EXPECT_TRUE(base::Contains(task_names, kTaskName2));
}

TEST_F(TaskSchedulerTests, FindFirstTaskName) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName2, kTaskDescription2, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  EXPECT_EQ(kTaskName1, task_scheduler_->FindFirstTaskName(kPrefixTaskName1));
  EXPECT_EQ(kTaskName2, task_scheduler_->FindFirstTaskName(kPrefixTaskName2));
}

TEST_F(TaskSchedulerTests, GetTasksIncludesHidden) {
  base::CommandLine command_line = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  EXPECT_TRUE(
      task_scheduler_->RegisterTask(kTaskName1, kTaskDescription1, command_line,
                                    TaskScheduler::TRIGGER_TYPE_HOURLY, true));

  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  std::vector<std::wstring> task_names;
  EXPECT_TRUE(task_scheduler_->GetTaskNameList(task_names));
  EXPECT_TRUE(base::Contains(task_names, kTaskName1));
}

TEST_F(TaskSchedulerTests, GetTaskInfoExecActions) {
  base::CommandLine command_line1({L"c:\\test\\process 1.exe"});

  ExpectRegisterTaskSucceeds(kTaskName1, kTaskDescription1, command_line1,
                             TaskScheduler::TRIGGER_TYPE_HOURLY, false);
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, info));
  EXPECT_EQ(0UL, info.exec_actions.size());
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
  ASSERT_EQ(1UL, info.exec_actions.size());
  EXPECT_EQ(base::CommandLine::QuoteForCommandLineToArgvW(
                command_line1.GetProgram().value()),
            info.exec_actions[0].application_path.value());
  EXPECT_EQ(command_line1.GetArgumentsString(), info.exec_actions[0].arguments);

  base::CommandLine command_line2({L"c:\\test\\process2.exe"});
  command_line2.AppendSwitch(kUnitTestSwitch);
  ExpectRegisterTaskSucceeds(kTaskName2, kTaskDescription2, command_line2,
                             TaskScheduler::TRIGGER_TYPE_HOURLY, false);
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  // The |info| struct is re-used to ensure that new task information overwrites
  // the previous contents of the struct.
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName2, info));
  ASSERT_EQ(1UL, info.exec_actions.size());
  EXPECT_EQ(base::CommandLine::QuoteForCommandLineToArgvW(
                command_line2.GetProgram().value()),
            info.exec_actions[0].application_path.value());
  EXPECT_EQ(command_line2.GetArgumentsString(), info.exec_actions[0].arguments);
}

TEST_F(TaskSchedulerTests, GetTaskInfoNameAndDescription) {
  base::CommandLine command_line1 = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  ExpectRegisterTaskSucceeds(kTaskName1, kTaskDescription1, command_line1,
                             TaskScheduler::TRIGGER_TYPE_HOURLY, false);
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, info));
  EXPECT_EQ(L"", info.description);
  EXPECT_EQ(L"", info.name);

  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
  EXPECT_EQ(kTaskDescription1, info.description);
  EXPECT_EQ(kTaskName1, info.name);

  const std::wstring expected_task_folder = base::StrCat(
      {L"\\" COMPANY_SHORTNAME_STRING,
       IsSystemInstall(GetUpdaterScopeForTesting()) ? L"System" : L"User",
       L"\\" PRODUCT_FULLNAME_STRING});
  EXPECT_EQ(task_scheduler_->GetTaskSubfolderName(), expected_task_folder);
  EXPECT_TRUE(task_scheduler_->HasTaskFolder(expected_task_folder));
}

TEST_F(TaskSchedulerTests, GetTaskInfoLogonType) {
  const bool is_system = IsSystemInstall(GetUpdaterScopeForTesting());

  base::CommandLine command_line1 = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  ExpectRegisterTaskSucceeds(kTaskName1, kTaskDescription1, command_line1,
                             TaskScheduler::TRIGGER_TYPE_HOURLY, false);
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, info));
  EXPECT_EQ(0U, info.logon_type);
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));
  EXPECT_EQ(!is_system, !!(info.logon_type & TaskScheduler::LOGON_INTERACTIVE));
  EXPECT_EQ(is_system, !!(info.logon_type & TaskScheduler::LOGON_SERVICE));
  EXPECT_FALSE(info.logon_type & TaskScheduler::LOGON_S4U);
}

TEST_F(TaskSchedulerTests, GetTaskInfoUserId) {
  const bool is_system = IsSystemInstall(GetUpdaterScopeForTesting());

  base::CommandLine command_line1 = GetTestProcessCommandLine(
      GetUpdaterScopeForTesting(), test::GetTestName());

  ExpectRegisterTaskSucceeds(kTaskName1, kTaskDescription1, command_line1,
                             TaskScheduler::TRIGGER_TYPE_HOURLY, false);
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, info));
  EXPECT_EQ(L"", info.user_id);

  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, info));

  const std::wstring expected_user_id = [&is_system]() -> std::wstring {
    if (is_system) {
      return L"SYSTEM";
    }

    base::win::ScopedBstr user_name_bstr;
    ULONG user_name_size = 256;
    EXPECT_TRUE(::GetUserNameExW(
        NameSamCompatible,
        user_name_bstr.AllocateBytes(user_name_size * sizeof(OLECHAR)),
        &user_name_size));
    return user_name_bstr.Get();
  }();

  EXPECT_TRUE(base::EndsWith(info.user_id, expected_user_id,
                             base::CompareCase::INSENSITIVE_ASCII) ||
              base::EndsWith(expected_user_id, info.user_id,
                             base::CompareCase::INSENSITIVE_ASCII));
}

TEST_F(TaskSchedulerTests, GetTaskInfoTriggerTypes) {
  for (const TaskScheduler::TriggerType expected_trigger_type : {
           TaskScheduler::TRIGGER_TYPE_LOGON,
           TaskScheduler::TRIGGER_TYPE_HOURLY,
           TaskScheduler::TRIGGER_TYPE_EVERY_FIVE_HOURS,
       }) {
    RunGetTaskInfoTriggerTypesTest(expected_trigger_type);
  }

  RunGetTaskInfoTriggerTypesTest(TaskScheduler::TRIGGER_TYPE_LOGON |
                                 TaskScheduler::TRIGGER_TYPE_HOURLY |
                                 TaskScheduler::TRIGGER_TYPE_EVERY_FIVE_HOURS);
}

TEST(TaskSchedulerTest, NoSubfolders) {
  scoped_refptr<TaskScheduler> task_scheduler = TaskScheduler::CreateInstance(
      GetUpdaterScopeForTesting(), /*use_task_subfolders=*/false);
  ASSERT_TRUE(task_scheduler);

  constexpr int kNumTasks = 6;
  const std::wstring kTaskNamePrefix(base::ASCIIToWide(test::GetTestName()));

  for (int count = 0; count < kNumTasks; ++count) {
    std::wstring task_name(kTaskNamePrefix);
    task_name.push_back(L'0' + count);

    ASSERT_TRUE(task_scheduler->RegisterTask(
        task_name, task_name,
        base::CommandLine::FromString(L"C:\\temp\\temp.exe"),
        TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, false));

    ASSERT_TRUE(task_scheduler->DeleteTask(task_name));
  }
}

TEST(TaskSchedulerTest, ForEachTaskWithPrefix) {
  for (bool use_task_subfolders : {true, false}) {
    scoped_refptr<TaskScheduler> task_scheduler = TaskScheduler::CreateInstance(
        GetUpdaterScopeForTesting(), use_task_subfolders);
    ASSERT_TRUE(task_scheduler);

    constexpr int kNumTasks = 6;
    const std::wstring kTaskNamePrefix(base::ASCIIToWide(test::GetTestName()));

    for (int count = 0; count < kNumTasks; ++count) {
      std::wstring task_name(kTaskNamePrefix);
      task_name.push_back(L'0' + count);

      EXPECT_TRUE(task_scheduler->RegisterTask(
          task_name, task_name,
          base::CommandLine::FromString(L"C:\\temp\\temp.exe"),
          TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, false));
    }

    scoped_refptr<TaskScheduler> task_scheduler_different_namespace =
        TaskScheduler::CreateInstance(GetUpdaterScopeForTesting(),
                                      !use_task_subfolders);
    ASSERT_TRUE(task_scheduler_different_namespace);

    int count_entries = 0;

    task_scheduler_different_namespace->ForEachTaskWithPrefix(
        kTaskNamePrefix, [&count_entries](const std::wstring& /*task_name*/) {
          ++count_entries;
        });

    EXPECT_EQ(count_entries, 0);

    count_entries = 0;

    task_scheduler->ForEachTaskWithPrefix(
        kTaskNamePrefix, [&count_entries, &task_scheduler,
                          kTaskNamePrefix](const std::wstring& task_name) {
          EXPECT_TRUE(base::StartsWith(task_name, kTaskNamePrefix));
          ++count_entries;
          EXPECT_TRUE(task_scheduler->DeleteTask(task_name));
        });

    EXPECT_EQ(count_entries, kNumTasks);
  }
}

}  // namespace updater
