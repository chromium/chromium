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
#include <string>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/util/unittest_util.h"
#include "chrome/updater/util/util.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/test/test_executables.h"
#include "chrome/updater/win/test/test_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    DeleteUpdaterLog();

    task_scheduler_ = TaskScheduler::CreateInstance();
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
    ASSERT_FALSE(test::IsProcessRunning(kTestProcessExecutableName));
    EXPECT_TRUE(IsServiceRunning(SERVICE_SCHEDULE));
  }

  void TearDown() override {
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
    EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));

    // Make sure every processes launched with scheduled task are completed.
    test::WaitForProcessesToExit(kTestProcessExecutableName,
                                 TestTimeouts::action_max_timeout());
    EXPECT_FALSE(test::IsProcessRunning(kTestProcessExecutableName));

    DeleteUpdaterLog();
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

  void DeleteUpdaterLog() {
    const absl::optional<base::FilePath> log_file =
        GetLogFilePath(GetTestScope());
    if (log_file) {
      base::DeleteFile(*log_file);
    }
  }

 protected:
  std::unique_ptr<TaskScheduler> task_scheduler_;
};

}  // namespace

TEST_F(TaskSchedulerTests, DeleteAndIsRegistered) {
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));

  // Construct the full-path of the test executable.
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  // Validate that the task is properly seen as registered when it is.
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
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

// TODO(crbug.com/1295399) : this test fails on Builder
// win10-updater-tester-dbg-uac, and we do not know why.
TEST_F(TaskSchedulerTests, DISABLED_RunAProgramNow) {
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  // Create a unique name for a shared event to be waited for in this process
  // and signaled in the test process to confirm it was scheduled and ran.
  const std::wstring event_name =
      base::StrCat({kTestProcessExecutableName, L"-",
                    base::NumberToWString(::GetCurrentProcessId())});
  NamedObjectAttributes attr =
      GetNamedObjectAttributes(event_name.c_str(), GetTestScope());

  base::WaitableEvent event(base::win::ScopedHandle(
      ::CreateEvent(&attr.sa, FALSE, FALSE, attr.name.c_str())));
  ASSERT_NE(event.handle(), nullptr);

  command_line.AppendSwitchNative(kTestEventToSignal, attr.name);
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->StartTask(kTaskName1));

  VLOG(0) << [this]() {
    TaskScheduler::TaskInfo info;
    EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));
    return info;
  }();

  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));

  test::PrintLog(GetTestScope());
}

TEST_F(TaskSchedulerTests, Hourly) {
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  base::Time now(base::Time::NowFromSystemTime());
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  base::TimeDelta one_hour(base::Hours(1));
  base::TimeDelta one_minute(base::Minutes(1));

  base::Time next_run_time;
  EXPECT_TRUE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
  EXPECT_LT(next_run_time, UTCTimeToLocalTime(now + one_hour + one_minute));
  EXPECT_GT(next_run_time, UTCTimeToLocalTime(now + one_hour - one_minute));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_FALSE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
}

TEST_F(TaskSchedulerTests, EveryFiveHours) {
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  base::Time now(base::Time::NowFromSystemTime());
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
      TaskScheduler::TRIGGER_TYPE_EVERY_FIVE_HOURS, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  base::TimeDelta five_hours(base::Hours(5));
  base::TimeDelta one_minute(base::Minutes(1));

  base::Time next_run_time;
  EXPECT_TRUE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
  EXPECT_LT(next_run_time, UTCTimeToLocalTime(now + five_hours + one_minute));
  EXPECT_GT(next_run_time, UTCTimeToLocalTime(now + five_hours - one_minute));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_FALSE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_FALSE(task_scheduler_->GetNextTaskRunTime(kTaskName1, &next_run_time));
}

TEST_F(TaskSchedulerTests, SetTaskEnabled) {
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
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
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName2, kTaskDescription2, command_line,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  std::vector<std::wstring> task_names;
  EXPECT_TRUE(task_scheduler_->GetTaskNameList(&task_names));
  EXPECT_TRUE(base::Contains(task_names, kTaskName1));
  EXPECT_TRUE(base::Contains(task_names, kTaskName2));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
}

TEST_F(TaskSchedulerTests, FindFirstTaskName) {
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName2, kTaskDescription2, command_line,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  EXPECT_STREQ(kTaskName1,
               task_scheduler_->FindFirstTaskName(kPrefixTaskName1).c_str());
  EXPECT_STREQ(kTaskName2,
               task_scheduler_->FindFirstTaskName(kPrefixTaskName2).c_str());

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
}

TEST_F(TaskSchedulerTests, GetTasksIncludesHidden) {
  base::CommandLine command_line = GetTestProcessCommandLine(GetTestScope());

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line,
      TaskScheduler::TRIGGER_TYPE_HOURLY, true));

  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  std::vector<std::wstring> task_names;
  EXPECT_TRUE(task_scheduler_->GetTaskNameList(&task_names));
  EXPECT_TRUE(base::Contains(task_names, kTaskName1));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

TEST_F(TaskSchedulerTests, GetTaskInfoExecActions) {
  base::CommandLine command_line1({L"c:\\test\\process 1.exe"});

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line1,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  EXPECT_EQ(0UL, info.exec_actions.size());
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));
  ASSERT_EQ(1UL, info.exec_actions.size());
  EXPECT_EQ(base::StrCat({L"\"", command_line1.GetProgram().value(), L"\""}),
            info.exec_actions[0].application_path.value());
  EXPECT_EQ(command_line1.GetArgumentsString(), info.exec_actions[0].arguments);

  base::CommandLine command_line2({L"c:\\test\\process2.exe"});
  command_line2.AppendSwitch(kUnitTestSwitch);
  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName2, kTaskDescription2, command_line2,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName2));

  // The |info| struct is re-used to ensure that new task information overwrites
  // the previous contents of the struct.
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  ASSERT_EQ(1UL, info.exec_actions.size());
  EXPECT_EQ(command_line2.GetProgram().value(),
            info.exec_actions[0].application_path.value());
  EXPECT_EQ(command_line2.GetArgumentsString(), info.exec_actions[0].arguments);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName2));
}

TEST_F(TaskSchedulerTests, GetTaskInfoNameAndDescription) {
  base::CommandLine command_line1 = GetTestProcessCommandLine(GetTestScope());

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line1,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  EXPECT_EQ(L"", info.description);
  EXPECT_EQ(L"", info.name);

  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));
  EXPECT_EQ(kTaskDescription1, info.description);
  EXPECT_EQ(kTaskName1, info.name);

  const std::wstring expected_task_folder =
      base::StrCat({L"\\" COMPANY_SHORTNAME_STRING,
                    IsSystemInstall(GetTestScope()) ? L"System" : L"User",
                    L"\\" PRODUCT_FULLNAME_STRING});
  EXPECT_EQ(task_scheduler_->GetTaskSubfolderName(GetTestScope()),
            expected_task_folder);
  EXPECT_TRUE(task_scheduler_->HasTaskFolder(expected_task_folder.c_str()));

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

TEST_F(TaskSchedulerTests, GetTaskInfoLogonType) {
  const bool is_system = IsSystemInstall(GetTestScope());

  base::CommandLine command_line1 = GetTestProcessCommandLine(GetTestScope());

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line1,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  EXPECT_EQ(0U, info.logon_type);
  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));
  EXPECT_EQ(!is_system, !!(info.logon_type & TaskScheduler::LOGON_INTERACTIVE));
  EXPECT_EQ(is_system, !!(info.logon_type & TaskScheduler::LOGON_SERVICE));
  EXPECT_FALSE(info.logon_type & TaskScheduler::LOGON_S4U);

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

TEST_F(TaskSchedulerTests, GetTaskInfoUserId) {
  const bool is_system = IsSystemInstall(GetTestScope());

  base::CommandLine command_line1 = GetTestProcessCommandLine(GetTestScope());

  EXPECT_TRUE(task_scheduler_->RegisterTask(
      GetTestScope(), kTaskName1, kTaskDescription1, command_line1,
      TaskScheduler::TRIGGER_TYPE_HOURLY, false));
  EXPECT_TRUE(task_scheduler_->IsTaskRegistered(kTaskName1));

  TaskScheduler::TaskInfo info;
  EXPECT_FALSE(task_scheduler_->GetTaskInfo(kTaskName2, &info));
  EXPECT_STREQ(L"", info.user_id.c_str());

  EXPECT_TRUE(task_scheduler_->GetTaskInfo(kTaskName1, &info));

  const std::wstring expected_user_id = [&is_system]() -> std::wstring {
    if (is_system)
      return L"SYSTEM";

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

  EXPECT_TRUE(task_scheduler_->DeleteTask(kTaskName1));
}

}  // namespace updater
