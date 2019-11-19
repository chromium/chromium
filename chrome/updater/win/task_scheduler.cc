// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/task_scheduler.h"

#include <mstask.h>
#include <oleauto.h>
#include <security.h>
#include <taskschd.h>
#include <wrl/client.h>

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/updater/win/util.h"

namespace updater {

namespace {

// Names of the TaskSchedulerV2 libraries so we can pin them below.
const wchar_t kV2Library[] = L"taskschd.dll";

// Text for times used in the V2 API of the Task Scheduler.
const wchar_t kOneHourText[] = L"PT1H";
const wchar_t kFiveHoursText[] = L"PT5H";
const wchar_t kZeroMinuteText[] = L"PT0M";
const wchar_t kFifteenMinutesText[] = L"PT15M";
const wchar_t kTwentyFourHoursText[] = L"PT24H";

// Most of the users with pending logs succeeds within 7 days, so no need to
// try for longer than that, especially for those who keep crashing.
const int kNumDaysBeforeExpiry = 7;
const size_t kNumDeleteTaskRetry = 3;
const size_t kDeleteRetryDelayInMs = 100;

// Return |timestamp| in the following string format YYYY-MM-DDTHH:MM:SS.
base::string16 GetTimestampString(const base::Time& timestamp) {
  base::Time::Exploded exploded_time;
  // The Z timezone info at the end of the string means UTC.
  timestamp.UTCExplode(&exploded_time);
  return base::StringPrintf(L"%04d-%02d-%02dT%02d:%02d:%02dZ",
                            exploded_time.year, exploded_time.month,
                            exploded_time.day_of_month, exploded_time.hour,
                            exploded_time.minute, exploded_time.second);
}

bool LocalSystemTimeToUTCFileTime(const SYSTEMTIME& system_time_local,
                                  FILETIME* file_time_utc) {
  DCHECK(file_time_utc);
  SYSTEMTIME system_time_utc = {};
  if (!::TzSpecificLocalTimeToSystemTime(nullptr, &system_time_local,
                                         &system_time_utc) ||
      !::SystemTimeToFileTime(&system_time_utc, file_time_utc)) {
    PLOG(ERROR) << "Failed to convert local system time to UTC file time.";
    return false;
  }
  return true;
}

bool UTCFileTimeToLocalSystemTime(const FILETIME& file_time_utc,
                                  SYSTEMTIME* system_time_local) {
  DCHECK(system_time_local);
  SYSTEMTIME system_time_utc = {};
  if (!::FileTimeToSystemTime(&file_time_utc, &system_time_utc) ||
      !::SystemTimeToTzSpecificLocalTime(nullptr, &system_time_utc,
                                         system_time_local)) {
    PLOG(ERROR) << "Failed to convert file time to UTC local system.";
    return false;
  }
  return true;
}

bool GetCurrentUser(base::win::ScopedBstr* user_name) {
  DCHECK(user_name);
  ULONG user_name_size = 256;
  // Paranoia... ;-)
  DCHECK_EQ(sizeof(OLECHAR), sizeof(WCHAR));
  if (!::GetUserNameExW(
          NameSamCompatible,
          user_name->AllocateBytes(user_name_size * sizeof(OLECHAR)),
          &user_name_size)) {
    if (::GetLastError() != ERROR_MORE_DATA) {
      PLOG(ERROR) << "GetUserNameEx failed.";
      return false;
    }
    if (!::GetUserNameExW(
            NameSamCompatible,
            user_name->AllocateBytes(user_name_size * sizeof(OLECHAR)),
            &user_name_size)) {
      DCHECK_NE(static_cast<DWORD>(ERROR_MORE_DATA), ::GetLastError());
      PLOG(ERROR) << "GetUserNameEx failed.";
      return false;
    }
  }
  return true;
}

void PinModule(const wchar_t* module_name) {
  // Force the DLL to stay loaded until program termination. We have seen
  // cases where it gets unloaded even though we still have references to
  // the objects we just CoCreated.
  base::NativeLibrary module_handle = nullptr;
  if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, module_name,
                            &module_handle)) {
    PLOG(ERROR) << "Failed to pin '" << module_name << "'.";
  }
}

// A task scheduler class uses the V2 API of the task scheduler.
class TaskSchedulerV2 final : public TaskScheduler {
 public:
  static bool Initialize() {
    DCHECK(!task_service_);
    DCHECK(!root_task_folder_);

    HRESULT hr =
        ::CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&task_service_));
    if (FAILED(hr)) {
      PLOG(ERROR) << "CreateInstance failed for CLSID_TaskScheduler. "
                  << std::hex << hr;
      return false;
    }
    hr = task_service_->Connect(base::win::ScopedVariant::kEmptyVariant,
                                base::win::ScopedVariant::kEmptyVariant,
                                base::win::ScopedVariant::kEmptyVariant,
                                base::win::ScopedVariant::kEmptyVariant);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to connect to task service. " << std::hex << hr;
      return false;
    }
    hr = task_service_->GetFolder(base::win::ScopedBstr(L"\\"),
                                  &root_task_folder_);
    if (FAILED(hr)) {
      LOG(ERROR) << "Can't get task service folder. " << std::hex << hr;
      return false;
    }
    PinModule(kV2Library);
    return true;
  }

  static void Terminate() {
    root_task_folder_.Reset();
    task_service_.Reset();
  }

  TaskSchedulerV2() {
    DCHECK(task_service_);
    DCHECK(root_task_folder_);
  }

  // TaskScheduler overrides.
  bool IsTaskRegistered(const wchar_t* task_name) override {
    DCHECK(task_name);
    if (!root_task_folder_)
      return false;

    return GetTask(task_name, nullptr);
  }

  bool GetNextTaskRunTime(const wchar_t* task_name,
                          base::Time* next_run_time) override {
    DCHECK(task_name);
    DCHECK(next_run_time);
    if (!root_task_folder_)
      return false;

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task))
      return false;

    // We unfortunately can't use get_NextRunTime because of a known bug which
    // requires hotfix: http://support.microsoft.com/kb/2495489/en-us. So fetch
    // one of the run times in the next day.
    // Also, although it's not obvious from MSDN, IRegisteredTask::GetRunTimes
    // expects local time.
    SYSTEMTIME start_system_time = {};
    GetLocalTime(&start_system_time);

    base::Time tomorrow(base::Time::NowFromSystemTime() +
                        base::TimeDelta::FromDays(1));
    SYSTEMTIME end_system_time = {};
    if (!UTCFileTimeToLocalSystemTime(tomorrow.ToFileTime(), &end_system_time))
      return false;

    DWORD num_run_times = 1;
    SYSTEMTIME* raw_run_times = nullptr;
    HRESULT hr = registered_task->GetRunTimes(
        &start_system_time, &end_system_time, &num_run_times, &raw_run_times);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to GetRunTimes, " << std::hex << hr;
      return false;
    }

    if (num_run_times == 0)
      return false;

    base::win::ScopedCoMem<SYSTEMTIME> run_times;
    run_times.Reset(raw_run_times);
    // Again, although unclear from MSDN, IRegisteredTask::GetRunTimes returns
    // local times.
    FILETIME file_time = {};
    if (!LocalSystemTimeToUTCFileTime(run_times[0], &file_time))
      return false;
    *next_run_time = base::Time::FromFileTime(file_time);
    return true;
  }

  bool SetTaskEnabled(const wchar_t* task_name, bool enabled) override {
    DCHECK(task_name);
    if (!root_task_folder_)
      return false;

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task)) {
      LOG(ERROR) << "Failed to find the task " << task_name
                 << " to enable/disable";
      return false;
    }

    HRESULT hr;
    hr = registered_task->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to set enabled status of task named " << task_name
                  << ". " << std::hex << hr;
      return false;
    }
    return true;
  }

  bool IsTaskEnabled(const wchar_t* task_name) override {
    DCHECK(task_name);
    if (!root_task_folder_)
      return false;

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task))
      return false;

    HRESULT hr;
    VARIANT_BOOL is_enabled;
    hr = registered_task->get_Enabled(&is_enabled);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get enabled status for task named " << task_name
                 << ". " << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    return is_enabled == VARIANT_TRUE;
  }

  bool GetTaskNameList(std::vector<base::string16>* task_names) override {
    DCHECK(task_names);
    if (!root_task_folder_)
      return false;

    for (TaskIterator it(root_task_folder_.Get()); !it.done(); it.Next())
      task_names->push_back(it.name());
    return true;
  }

  bool GetTaskInfo(const wchar_t* task_name, TaskInfo* info) override {
    DCHECK(task_name);
    DCHECK(info);
    if (!root_task_folder_)
      return false;

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task))
      return false;

    // Collect information into internal storage to ensure that we start with
    // a clean slate and don't return partial results on error.
    TaskInfo info_storage;
    HRESULT hr =
        GetTaskDescription(registered_task.Get(), &info_storage.description);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get description for task '" << task_name << "'. "
                 << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    if (!GetTaskExecActions(registered_task.Get(),
                            &info_storage.exec_actions)) {
      LOG(ERROR) << "Failed to get actions for task '" << task_name << "'";
      return false;
    }

    hr = GetTaskLogonType(registered_task.Get(), &info_storage.logon_type);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get logon type for task '" << task_name << "'. "
                 << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }
    info_storage.name = task_name;
    std::swap(*info, info_storage);
    return true;
  }

  bool DeleteTask(const wchar_t* task_name) override {
    DCHECK(task_name);
    if (!root_task_folder_)
      return false;

    VLOG(1) << "Delete Task '" << task_name << "'.";

    HRESULT hr =
        root_task_folder_->DeleteTask(base::win::ScopedBstr(task_name), 0);
    // This can happen, e.g., while running tests, when the file system stresses
    // quite a lot. Give it a few more chances to succeed.
    size_t num_retries_left = kNumDeleteTaskRetry;

    if (FAILED(hr)) {
      while ((hr == HRESULT_FROM_WIN32(ERROR_TRANSACTION_NOT_ACTIVE) ||
              hr == HRESULT_FROM_WIN32(ERROR_TRANSACTION_ALREADY_ABORTED)) &&
             --num_retries_left && IsTaskRegistered(task_name)) {
        LOG(WARNING) << "Retrying delete task because transaction not active, "
                     << std::hex << hr << ".";
        hr = root_task_folder_->DeleteTask(base::win::ScopedBstr(task_name), 0);
        ::Sleep(kDeleteRetryDelayInMs);
      }
      if (!IsTaskRegistered(task_name))
        hr = S_OK;
    }

    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      PLOG(ERROR) << "Can't delete task. " << std::hex << hr;
      return false;
    }

    DCHECK(!IsTaskRegistered(task_name));
    return true;
  }

  bool RegisterTask(const wchar_t* task_name,
                    const wchar_t* task_description,
                    const base::CommandLine& run_command,
                    TriggerType trigger_type,
                    bool hidden) override {
    DCHECK(task_name);
    DCHECK(task_description);
    if (!DeleteTask(task_name))
      return false;

    // Create the task definition object to create the task.
    Microsoft::WRL::ComPtr<ITaskDefinition> task;
    DCHECK(task_service_);
    HRESULT hr = task_service_->NewTask(0, &task);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't create new task. " << std::hex << hr;
      return false;
    }

    base::win::ScopedBstr user_name;
    if (!GetCurrentUser(&user_name))
      return false;

    if (trigger_type != TRIGGER_TYPE_NOW) {
      // Allow the task to run elevated on startup.
      Microsoft::WRL::ComPtr<IPrincipal> principal;
      hr = task->get_Principal(&principal);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't get principal. " << std::hex << hr;
        return false;
      }

      hr = principal->put_UserId(user_name);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put user id. " << std::hex << hr;
        return false;
      }

      hr = principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put logon type. " << std::hex << hr;
        return false;
      }
    }

    Microsoft::WRL::ComPtr<IRegistrationInfo> registration_info;
    hr = task->get_RegistrationInfo(&registration_info);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get registration info. " << std::hex << hr;
      return false;
    }

    hr = registration_info->put_Author(user_name);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't set registration info author. " << std::hex << hr;
      return false;
    }

    base::win::ScopedBstr description(task_description);
    hr = registration_info->put_Description(description);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't set description. " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<ITaskSettings> task_settings;
    hr = task->get_Settings(&task_settings);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get task settings. " << std::hex << hr;
      return false;
    }

    hr = task_settings->put_StartWhenAvailable(VARIANT_TRUE);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put 'StartWhenAvailable' to true. " << std::hex
                  << hr;
      return false;
    }

    // TODO(csharp): Find a way to only set this for log upload retry.
    hr = task_settings->put_DeleteExpiredTaskAfter(
        base::win::ScopedBstr(kZeroMinuteText));
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put 'DeleteExpiredTaskAfter'. " << std::hex << hr;
      return false;
    }

    hr = task_settings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put 'DisallowStartIfOnBatteries' to false. "
                  << std::hex << hr;
      return false;
    }

    hr = task_settings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put 'StopIfGoingOnBatteries' to false. " << std::hex
                  << hr;
      return false;
    }

    if (hidden) {
      hr = task_settings->put_Hidden(VARIANT_TRUE);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put 'Hidden' to true. " << std::hex << hr;
        return false;
      }
    }

    Microsoft::WRL::ComPtr<ITriggerCollection> trigger_collection;
    hr = task->get_Triggers(&trigger_collection);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get trigger collection. " << std::hex << hr;
      return false;
    }

    TASK_TRIGGER_TYPE2 task_trigger_type = TASK_TRIGGER_EVENT;
    base::win::ScopedBstr repetition_interval;
    switch (trigger_type) {
      case TRIGGER_TYPE_POST_REBOOT:
        task_trigger_type = TASK_TRIGGER_LOGON;
        break;
      case TRIGGER_TYPE_NOW:
        task_trigger_type = TASK_TRIGGER_REGISTRATION;
        break;
      case TRIGGER_TYPE_HOURLY:
      case TRIGGER_TYPE_EVERY_FIVE_HOURS:
        task_trigger_type = TASK_TRIGGER_DAILY;
        if (trigger_type == TRIGGER_TYPE_EVERY_FIVE_HOURS) {
          repetition_interval.Reset(::SysAllocString(kFiveHoursText));
        } else if (trigger_type == TRIGGER_TYPE_HOURLY) {
          repetition_interval.Reset(::SysAllocString(kOneHourText));
        } else {
          NOTREACHED() << "Unknown TriggerType?";
        }
        break;
      default:
        NOTREACHED() << "Unknown TriggerType?";
    }

    Microsoft::WRL::ComPtr<ITrigger> trigger;
    hr = trigger_collection->Create(task_trigger_type, &trigger);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't create trigger of type " << task_trigger_type
                  << ". " << std::hex << hr;
      return false;
    }

    if (trigger_type == TRIGGER_TYPE_HOURLY ||
        trigger_type == TRIGGER_TYPE_EVERY_FIVE_HOURS) {
      Microsoft::WRL::ComPtr<IDailyTrigger> daily_trigger;
      hr = trigger.As(&daily_trigger);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't Query for registration trigger. " << std::hex
                    << hr;
        return false;
      }

      hr = daily_trigger->put_DaysInterval(1);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put 'DaysInterval' to 1, " << std::hex << hr;
        return false;
      }

      Microsoft::WRL::ComPtr<IRepetitionPattern> repetition_pattern;
      hr = trigger->get_Repetition(&repetition_pattern);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't get 'Repetition'. " << std::hex << hr;
        return false;
      }

      // The duration is the time to keep repeating until the next daily
      // trigger.
      hr = repetition_pattern->put_Duration(
          base::win::ScopedBstr(kTwentyFourHoursText));
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put 'Duration' to " << kTwentyFourHoursText
                    << ". " << std::hex << hr;
        return false;
      }

      hr = repetition_pattern->put_Interval(repetition_interval);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put 'Interval' to " << repetition_interval << ". "
                    << std::hex << hr;
        return false;
      }

      // Start now.
      base::Time now(base::Time::NowFromSystemTime());
      base::win::ScopedBstr start_boundary(GetTimestampString(now));
      hr = trigger->put_StartBoundary(start_boundary);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put 'StartBoundary' to " << start_boundary << ". "
                    << std::hex << hr;
        return false;
      }
    }

    if (trigger_type == TRIGGER_TYPE_POST_REBOOT) {
      Microsoft::WRL::ComPtr<ILogonTrigger> logon_trigger;
      hr = trigger.As(&logon_trigger);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't query trigger for 'ILogonTrigger'. " << std::hex
                    << hr;
        return false;
      }

      hr = logon_trigger->put_Delay(base::win::ScopedBstr(kFifteenMinutesText));
      if (FAILED(hr)) {
        PLOG(ERROR) << "Can't put 'Delay'. " << std::hex << hr;
        return false;
      }
    }

    // None of the triggers should go beyond kNumDaysBeforeExpiry.
    base::Time expiry_date(base::Time::NowFromSystemTime() +
                           base::TimeDelta::FromDays(kNumDaysBeforeExpiry));
    base::win::ScopedBstr end_boundary(GetTimestampString(expiry_date));
    hr = trigger->put_EndBoundary(end_boundary);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put 'EndBoundary' to " << end_boundary << ". "
                  << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<IActionCollection> actions;
    hr = task->get_Actions(&actions);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get actions collection. " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<IAction> action;
    hr = actions->Create(TASK_ACTION_EXEC, &action);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't create exec action. " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<IExecAction> exec_action;
    hr = action.As(&exec_action);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't query for exec action. " << std::hex << hr;
      return false;
    }

    base::win::ScopedBstr path(run_command.GetProgram().value());
    hr = exec_action->put_Path(path);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't set path of exec action. " << std::hex << hr;
      return false;
    }

    base::win::ScopedBstr args(run_command.GetArgumentsString());
    hr = exec_action->put_Arguments(args);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't set arguments of exec action. " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    base::win::ScopedVariant user(user_name);

    DCHECK(root_task_folder_);
    hr = root_task_folder_->RegisterTaskDefinition(
        base::win::ScopedBstr(task_name), task.Get(), TASK_CREATE,
        *user.AsInput(),  // Not really input, but API expect non-const.
        base::win::ScopedVariant::kEmptyVariant, TASK_LOGON_NONE,
        base::win::ScopedVariant::kEmptyVariant, &registered_task);
    if (FAILED(hr)) {
      LOG(ERROR) << "RegisterTaskDefinition failed. " << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    DCHECK(IsTaskRegistered(task_name));

    VLOG(1) << "Successfully registered: "
            << run_command.GetCommandLineString();
    return true;
  }

 private:
  // Helper class that lets us iterate over all registered tasks.
  class TaskIterator {
   public:
    explicit TaskIterator(ITaskFolder* task_folder) {
      DCHECK(task_folder);
      HRESULT hr = task_folder->GetTasks(TASK_ENUM_HIDDEN, &tasks_);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get registered tasks from folder. "
                    << std::hex << hr;
        done_ = true;
        return;
      }
      hr = tasks_->get_Count(&num_tasks_);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get registered tasks count. " << std::hex
                    << hr;
        done_ = true;
        return;
      }
      Next();
    }

    // Increment to the next valid item in the task list. Skip entries for
    // which we cannot retrieve a name.
    void Next() {
      DCHECK(!done_);
      task_.Reset();
      name_.clear();
      if (++task_index_ >= num_tasks_) {
        done_ = true;
        return;
      }

      // Note: get_Item uses 1 based indices.
      HRESULT hr =
          tasks_->get_Item(base::win::ScopedVariant(task_index_ + 1), &task_);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get task at index: " << task_index_ << ". "
                    << std::hex << hr;
        Next();
        return;
      }

      base::win::ScopedBstr task_name_bstr;
      hr = task_->get_Name(task_name_bstr.Receive());
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get name at index: " << task_index_ << ". "
                    << std::hex << hr;
        Next();
        return;
      }
      name_ = base::string16(task_name_bstr ? task_name_bstr : L"");
    }

    // Detach the currently active task and pass ownership to the caller.
    // After this method has been called, the -> operator must no longer be
    // used.
    IRegisteredTask* Detach() { return task_.Detach(); }

    // Provide access to the current task.
    IRegisteredTask* operator->() const {
      IRegisteredTask* result = task_.Get();
      DCHECK(result);
      return result;
    }

    const base::string16& name() const { return name_; }
    bool done() const { return done_; }

   private:
    Microsoft::WRL::ComPtr<IRegisteredTaskCollection> tasks_;
    Microsoft::WRL::ComPtr<IRegisteredTask> task_;
    base::string16 name_;
    long task_index_ = -1;  // NOLINT, API requires a long.
    long num_tasks_ = 0;    // NOLINT, API requires a long.
    bool done_ = false;
  };

  // Return the task with |task_name| and false if not found. |task| can be null
  // when only interested in task's existence.
  bool GetTask(const wchar_t* task_name, IRegisteredTask** task) {
    for (TaskIterator it(root_task_folder_.Get()); !it.done(); it.Next()) {
      if (::_wcsicmp(it.name().c_str(), task_name) == 0) {
        if (task)
          *task = it.Detach();
        return true;
      }
    }
    return false;
  }

  // Return the description of the task.
  HRESULT GetTaskDescription(IRegisteredTask* task,
                             base::string16* description) {
    DCHECK(task);
    DCHECK(description);

    base::win::ScopedBstr task_name_bstr;
    HRESULT hr = task->get_Name(task_name_bstr.Receive());
    base::string16 task_name =
        base::string16(task_name_bstr ? task_name_bstr : L"");
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get task name";
    }

    Microsoft::WRL::ComPtr<ITaskDefinition> task_info;
    hr = task->get_Definition(&task_info);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get definition for task, " << task_name << ": "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    Microsoft::WRL::ComPtr<IRegistrationInfo> reg_info;
    hr = task_info->get_RegistrationInfo(&reg_info);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get registration info, " << task_name << ": "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    base::win::ScopedBstr raw_description;
    hr = reg_info->get_Description(raw_description.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get description, " << task_name << ": "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }
    *description = base::string16(raw_description ? raw_description : L"");
    return ERROR_SUCCESS;
  }

  // Return all executable actions associated with the given task. Non-exec
  // actions are silently ignored.
  bool GetTaskExecActions(IRegisteredTask* task,
                          std::vector<TaskExecAction>* actions) {
    DCHECK(task);
    DCHECK(actions);
    Microsoft::WRL::ComPtr<ITaskDefinition> task_definition;
    HRESULT hr = task->get_Definition(&task_definition);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to get definition of task, " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<IActionCollection> action_collection;
    hr = task_definition->get_Actions(&action_collection);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to get action collection, " << std::hex << hr;
      return false;
    }

    long actions_count = 0;  // NOLINT, API requires a long.
    hr = action_collection->get_Count(&actions_count);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to get number of actions, " << std::hex << hr;
      return false;
    }

    // Find and return as many exec actions as possible in |actions| and return
    // false if there were any errors on the way. Note that the indexing of
    // actions is 1-based.
    bool success = true;
    for (long action_index = 1;  // NOLINT
         action_index <= actions_count; ++action_index) {
      Microsoft::WRL::ComPtr<IAction> action;
      hr = action_collection->get_Item(action_index, &action);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get action at index " << action_index << ", "
                    << std::hex << hr;
        success = false;
        continue;
      }

      ::TASK_ACTION_TYPE action_type;
      hr = action->get_Type(&action_type);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get the type of action at index "
                    << action_index << ", " << std::hex << hr;
        success = false;
        continue;
      }

      // We only care about exec actions for now. The other types are
      // TASK_ACTION_COM_HANDLER, TASK_ACTION_SEND_EMAIL,
      // TASK_ACTION_SHOW_MESSAGE. The latter two are marked as deprecated in
      // the Task Scheduler's GUI.
      if (action_type != ::TASK_ACTION_EXEC)
        continue;

      Microsoft::WRL::ComPtr<IExecAction> exec_action;
      hr = action.As(&exec_action);
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to query from action, " << std::hex << hr;
        success = false;
        continue;
      }

      base::win::ScopedBstr application_path;
      hr = exec_action->get_Path(application_path.Receive());
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get path from action, " << std::hex << hr;
        success = false;
        continue;
      }

      base::win::ScopedBstr working_dir;
      hr = exec_action->get_WorkingDirectory(working_dir.Receive());
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get working directory for action, "
                    << std::hex << hr;
        success = false;
        continue;
      }

      base::win::ScopedBstr parameters;
      hr = exec_action->get_Arguments(parameters.Receive());
      if (FAILED(hr)) {
        PLOG(ERROR) << "Failed to get arguments from action of task, "
                    << std::hex << hr;
        success = false;
        continue;
      }

      actions->push_back(
          {base::FilePath(application_path ? application_path : L""),
           base::FilePath(working_dir ? working_dir : L""),
           base::string16(parameters ? parameters : L"")});
    }
    return success;
  }

  // Return the log-on type required for the task's actions to be run.
  HRESULT GetTaskLogonType(IRegisteredTask* task, uint32_t* logon_type) {
    DCHECK(task);
    DCHECK(logon_type);
    Microsoft::WRL::ComPtr<ITaskDefinition> task_info;
    HRESULT hr = task->get_Definition(&task_info);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get definition, " << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    Microsoft::WRL::ComPtr<IPrincipal> principal;
    hr = task_info->get_Principal(&principal);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get principal info, " << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    TASK_LOGON_TYPE raw_logon_type;
    hr = principal->get_LogonType(&raw_logon_type);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get logon type info, " << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    switch (raw_logon_type) {
      case TASK_LOGON_INTERACTIVE_TOKEN:
        *logon_type = LOGON_INTERACTIVE;
        break;
      case TASK_LOGON_GROUP:     // fall-thru
      case TASK_LOGON_PASSWORD:  // fall-thru
      case TASK_LOGON_SERVICE_ACCOUNT:
        *logon_type = LOGON_SERVICE;
        break;
      case TASK_LOGON_S4U:
        *logon_type = LOGON_SERVICE | LOGON_S4U;
        break;
      case TASK_LOGON_INTERACTIVE_TOKEN_OR_PASSWORD:
        *logon_type = LOGON_INTERACTIVE | LOGON_SERVICE;
        break;
      default:
        *logon_type = LOGON_UNKNOWN;
        break;
    }
    return ERROR_SUCCESS;
  }

  static Microsoft::WRL::ComPtr<ITaskService> task_service_;
  static Microsoft::WRL::ComPtr<ITaskFolder> root_task_folder_;

  DISALLOW_COPY_AND_ASSIGN(TaskSchedulerV2);
};

Microsoft::WRL::ComPtr<ITaskService> TaskSchedulerV2::task_service_;
Microsoft::WRL::ComPtr<ITaskFolder> TaskSchedulerV2::root_task_folder_;

}  // namespace

TaskScheduler::TaskInfo::TaskInfo() = default;

TaskScheduler::TaskInfo::TaskInfo(const TaskScheduler::TaskInfo&) = default;

TaskScheduler::TaskInfo::TaskInfo(TaskScheduler::TaskInfo&&) = default;

TaskScheduler::TaskInfo& TaskScheduler::TaskInfo::operator=(
    const TaskScheduler::TaskInfo&) = default;

TaskScheduler::TaskInfo& TaskScheduler::TaskInfo::operator=(
    TaskScheduler::TaskInfo&&) = default;

TaskScheduler::TaskInfo::~TaskInfo() = default;

// static.
bool TaskScheduler::Initialize() {
  return TaskSchedulerV2::Initialize();
}

// static.
void TaskScheduler::Terminate() {
  TaskSchedulerV2::Terminate();
}

// static.
std::unique_ptr<TaskScheduler> TaskScheduler::CreateInstance() {
  return std::make_unique<TaskSchedulerV2>();
}

TaskScheduler::TaskScheduler() = default;

}  // namespace updater
