// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/task_scheduler.h"

#include <mstask.h>
#include <oleauto.h>
#include <security.h>
#include <taskschd.h>
#include <wrl/client.h>

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/native_library.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"

namespace updater {
namespace {

// Names of the TaskSchedulerV2 libraries so we can pin them below.
const wchar_t kV2Library[] = L"taskschd.dll";

// Text for times used in the V2 API of the Task Scheduler.
const wchar_t kOneHourText[] = L"PT1H";
const wchar_t kFiveHoursText[] = L"PT5H";
const wchar_t kOneDayText[] = L"P1D";

const size_t kNumDeleteTaskRetry = 3;
const size_t kDeleteRetryDelayInMs = 100;

// Returns true if `error` is HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) or
// HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND).
[[nodiscard]] bool IsFileOrPathNotFoundError(HRESULT hr) {
  return hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
         hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
}

// Returns |timestamp| in the format YYYY-MM-DDTHH:MM:SS.
std::wstring GetTimestampString(const base::Time& timestamp) {
  // This intentionally avoids depending on the facilities in
  // base/i18n/time_formatting.h so the updater will not need to depend on the
  // ICU data file.
  base::Time::Exploded exploded_time;
  // The Z timezone info at the end of the string means UTC.
  timestamp.UTCExplode(&exploded_time);
  return base::ASCIIToWide(base::StringPrintf(
      "%04d-%02d-%02dT%02d:%02d:%02dZ", exploded_time.year, exploded_time.month,
      exploded_time.day_of_month, exploded_time.hour, exploded_time.minute,
      exploded_time.second));
}

[[nodiscard]] bool UTCFileTimeToLocalSystemTime(const FILETIME& file_time_utc,
                                                SYSTEMTIME& system_time_local) {
  SYSTEMTIME system_time_utc = {};
  if (!::FileTimeToSystemTime(&file_time_utc, &system_time_utc) ||
      !::SystemTimeToTzSpecificLocalTime(nullptr, &system_time_utc,
                                         &system_time_local)) {
    PLOG(ERROR) << "Failed to convert file time to UTC local system.";
    return false;
  }
  return true;
}

[[nodiscard]] bool GetCurrentUser(base::win::ScopedBstr& user_name) {
  static_assert(sizeof(OLECHAR) == sizeof(WCHAR));
  ULONG user_name_size = 256;
  if (!::GetUserNameExW(
          NameSamCompatible,
          user_name.AllocateBytes(user_name_size * sizeof(OLECHAR)),
          &user_name_size)) {
    if (::GetLastError() != ERROR_MORE_DATA) {
      PLOG(ERROR) << "GetUserNameEx failed.";
      return false;
    }
    if (!::GetUserNameExW(
            NameSamCompatible,
            user_name.AllocateBytes(user_name_size * sizeof(OLECHAR)),
            &user_name_size)) {
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

// Returns the XML serialization of a task definition.
[[nodiscard]] std::wstring GetTaskXml(ITaskDefinition* task) {
  base::win::ScopedBstr task_xml;
  task->get_XmlText(task_xml.Receive());
  return task_xml.Get() ? std::wstring(task_xml.Get()) : std::wstring();
}

// A task scheduler class uses the V2 API of the task scheduler.
class TaskSchedulerV2 final : public TaskScheduler {
 private:
  // Forces creation of instances through the factory function because the
  // constructor of `TaskSchedulerV2` is not accessible.
  struct ConstructorTag {};

 public:
  static scoped_refptr<TaskScheduler> CreateInstance(UpdaterScope scope,
                                                     bool use_task_subfolders) {
    auto instance = base::MakeRefCounted<TaskSchedulerV2>(
        ConstructorTag(), scope, use_task_subfolders);
    return instance->task_service_ ? instance : nullptr;
  }

  TaskSchedulerV2(ConstructorTag /*tag*/,
                  UpdaterScope scope,
                  bool use_task_subfolders)
      : scope_(scope), use_task_subfolders_(use_task_subfolders) {
    task_service_ = GetTaskService();
    if (!task_service_) {
      VLOG(2) << "Can't get the task service.";
      return;
    }
    task_folder_ = GetUpdaterTaskFolder();
    VLOG_IF(2, !task_folder_) << "Can't get the task scheduler folder.";
  }
  TaskSchedulerV2(const TaskSchedulerV2&) = delete;
  TaskSchedulerV2& operator=(const TaskSchedulerV2&) = delete;

  // TaskScheduler overrides.
  bool IsTaskRegistered(const std::wstring& task_name) override {
    if (!task_folder_) {
      return false;
    }
    return GetTask(task_name, nullptr);
  }

  bool GetNextTaskRunTime(const std::wstring& task_name,
                          base::Time& next_run_time) override {
    if (!task_folder_) {
      return false;
    }

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task)) {
      return false;
    }

    // We unfortunately can't use get_NextRunTime because of a known bug which
    // requires hotfix: http://support.microsoft.com/kb/2495489/en-us. So fetch
    // one of the run times in the next day.
    // Also, although it's not obvious from MSDN, IRegisteredTask::GetRunTimes
    // expects local time.
    SYSTEMTIME start_system_time = {};
    GetLocalTime(&start_system_time);

    base::Time tomorrow(base::Time::NowFromSystemTime() + base::Days(1));
    SYSTEMTIME end_system_time = {};
    if (!UTCFileTimeToLocalSystemTime(tomorrow.ToFileTime(), end_system_time)) {
      return false;
    }

    DWORD num_run_times = 1;
    SYSTEMTIME* raw_run_times = nullptr;
    HRESULT hr = registered_task->GetRunTimes(
        &start_system_time, &end_system_time, &num_run_times, &raw_run_times);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to GetRunTimes, " << std::hex << hr;
      return false;
    }

    if (num_run_times == 0) {
      return false;
    }

    base::win::ScopedCoMem<SYSTEMTIME> run_times;
    run_times.Reset(raw_run_times);
    // Again, although unclear from MSDN, IRegisteredTask::GetRunTimes returns
    // local times.
    // The returned local times are already adjusted for DST.
    FILETIME local_file_time = {};
    if (!::SystemTimeToFileTime(&run_times[0], &local_file_time)) {
      return false;
    }
    next_run_time = base::Time::FromFileTime(local_file_time);
    return true;
  }

  bool SetTaskEnabled(const std::wstring& task_name, bool enabled) override {
    if (!task_folder_) {
      return false;
    }

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task)) {
      LOG(ERROR) << "Failed to find the task " << task_name
                 << " to enable/disable";
      return false;
    }

    HRESULT hr =
        registered_task->put_Enabled(enabled ? VARIANT_TRUE : VARIANT_FALSE);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to set enabled status of task named " << task_name
                  << ". " << std::hex << hr;
      return false;
    }
    return true;
  }

  bool IsTaskEnabled(const std::wstring& task_name) override {
    if (!task_folder_) {
      return false;
    }

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task)) {
      return false;
    }

    VARIANT_BOOL is_enabled = VARIANT_FALSE;
    HRESULT hr = registered_task->get_Enabled(&is_enabled);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get enabled status for task named " << task_name
                 << ". " << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    return is_enabled == VARIANT_TRUE;
  }

  bool IsTaskRunning(const std::wstring& task_name) override {
    if (!task_folder_) {
      return false;
    }

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task)) {
      return false;
    }

    Microsoft::WRL::ComPtr<IRunningTaskCollection> running_task_collection;
    HRESULT hr = registered_task->GetInstances(0, &running_task_collection);
    if (FAILED(hr)) {
      LOG(ERROR) << "IRegisteredTask.GetInstances failed. " << std::hex << hr;
      return false;
    }

    LONG count = 0;
    hr = running_task_collection->get_Count(&count);
    if (FAILED(hr)) {
      LOG(ERROR) << "IRunningTaskCollection.get_Count failed. " << std::hex
                 << hr;
      return false;
    }

    return count > 0;
  }

  bool GetTaskNameList(std::vector<std::wstring>& task_names) override {
    if (!task_folder_) {
      return false;
    }
    for (TaskIterator it(task_folder_.Get()); !it.done(); it.Next()) {
      task_names.push_back(it.name());
    }
    return true;
  }

  std::wstring FindFirstTaskName(const std::wstring& task_prefix) override {
    CHECK(!task_prefix.empty());

    std::vector<std::wstring> task_names;
    if (!GetTaskNameList(task_names)) {
      return std::wstring();
    }

    for (const std::wstring& task_name : task_names) {
      if (base::StartsWith(task_name, task_prefix)) {
        return task_name;
      }
    }

    return std::wstring();
  }

  bool GetTaskInfo(const std::wstring& task_name, TaskInfo& info) override {
    if (!task_folder_) {
      return false;
    }

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task)) {
      return false;
    }

    // Collect information into internal storage to ensure that we start with
    // a clean slate and don't return partial results on error.
    TaskInfo info_storage;
    HRESULT hr =
        GetTaskDescription(registered_task.Get(), info_storage.description);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get description for task '" << task_name << "'. "
                 << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    if (!GetTaskExecActions(registered_task.Get(), info_storage.exec_actions)) {
      LOG(ERROR) << "Failed to get actions for task '" << task_name << "'";
      return false;
    }

    hr = GetTaskLogonType(registered_task.Get(), info_storage.logon_type);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get logon type for task '" << task_name << "'. "
                 << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    hr = GetTaskUserId(registered_task.Get(), info_storage.user_id);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get UserId for task '" << task_name << "'. "
                 << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    hr = GetTaskTriggerTypes(registered_task.Get(), info_storage.trigger_types);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get trigger types for task '" << task_name
                 << "'. " << std::hex << hr << ": "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    info_storage.name = task_name;
    std::swap(info, info_storage);
    return true;
  }

  bool HasTaskFolder(const std::wstring& folder_name) override {
    Microsoft::WRL::ComPtr<ITaskFolder> task_folder;
    const HRESULT hr = task_service_->GetFolder(
        base::win::ScopedBstr(folder_name).Get(), &task_folder);
    LOG_IF(ERROR, FAILED(hr))
        << "task_service_->GetFolder failed: " << folder_name << ": "
        << std::hex << hr;
    return SUCCEEDED(hr);
  }

  bool DeleteTask(const std::wstring& task_name) override {
    if (!task_folder_) {
      return false;
    }

    VLOG(1) << "Delete Task '" << task_name << "'.";
    HRESULT hr =
        task_folder_->DeleteTask(base::win::ScopedBstr(task_name).Get(), 0);
    VLOG_IF(1, SUCCEEDED(hr)) << "Task deleted.";

    // This can happen, e.g., while running tests, when the file system stresses
    // quite a lot. Give it a few more chances to succeed.
    size_t num_retries_left = kNumDeleteTaskRetry;
    if (FAILED(hr)) {
      while ((hr == HRESULT_FROM_WIN32(ERROR_TRANSACTION_NOT_ACTIVE) ||
              hr == HRESULT_FROM_WIN32(ERROR_TRANSACTION_ALREADY_ABORTED)) &&
             --num_retries_left && IsTaskRegistered(task_name)) {
        LOG(WARNING) << "Retrying delete task because transaction not active, "
                     << std::hex << hr << ".";
        hr =
            task_folder_->DeleteTask(base::win::ScopedBstr(task_name).Get(), 0);
        ::Sleep(kDeleteRetryDelayInMs);
      }
      if (!IsTaskRegistered(task_name)) {
        hr = S_OK;
      }
    }

    if (FAILED(hr) && hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
      PLOG(ERROR) << "Can't delete task. " << std::hex << hr;
      return false;
    }

    if (use_task_subfolders_) {
      // Try to delete \\Company\Product first and \\Company second.
      if (DeleteFolderIfEmpty(GetTaskSubfolderName())) {
        std::ignore = DeleteFolderIfEmpty(GetTaskCompanyFolder());
      }
    }

    return !IsTaskRegistered(task_name);
  }

  bool RegisterTask(const std::wstring& task_name,
                    const std::wstring& task_description,
                    const base::CommandLine& run_command,
                    int trigger_types,
                    bool hidden) override {
    // Create the task definition object to create the task.
    Microsoft::WRL::ComPtr<ITaskDefinition> task;
    HRESULT hr = task_service_->NewTask(0, &task);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't create new task. " << std::hex << hr;
      return false;
    }

    const bool is_system = IsSystemInstall(scope_);
    base::win::ScopedBstr user_name(L"NT AUTHORITY\\SYSTEM");
    if (!is_system && !GetCurrentUser(user_name)) {
      return false;
    }

    Microsoft::WRL::ComPtr<IPrincipal> principal;
    hr = task->get_Principal(&principal);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get principal. " << std::hex << hr;
      return false;
    }

    hr = principal->put_UserId(user_name.Get());
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put user id. " << std::hex << hr;
      return false;
    }

    hr = is_system ? principal->put_RunLevel(TASK_RUNLEVEL_HIGHEST)
                   : principal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put run level or logon type. " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<IRegistrationInfo> registration_info;
    hr = task->get_RegistrationInfo(&registration_info);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get registration info. " << std::hex << hr;
      return false;
    }

    hr = registration_info->put_Author(user_name.Get());
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't set registration info author. " << std::hex << hr;
      return false;
    }

    base::win::ScopedBstr description(task_description);
    hr = registration_info->put_Description(description.Get());
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

    Microsoft::WRL::ComPtr<IIdleSettings> idle_settings;
    hr = task_settings->get_IdleSettings(&idle_settings);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get 'IdleSettings'. " << std::hex << hr;
      return false;
    }

    hr = idle_settings->put_StopOnIdleEnd(VARIANT_FALSE);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't put 'StopOnIdleEnd' to false. " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<ITriggerCollection> trigger_collection;
    hr = task->get_Triggers(&trigger_collection);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't get trigger collection. " << std::hex << hr;
      return false;
    }

    for (const TriggerType trigger_type :
         {TRIGGER_TYPE_LOGON, TRIGGER_TYPE_NOW, TRIGGER_TYPE_HOURLY,
          TRIGGER_TYPE_EVERY_FIVE_HOURS}) {
      if (!(trigger_types & trigger_type)) {
        continue;
      }

      TASK_TRIGGER_TYPE2 task_trigger_type = TASK_TRIGGER_EVENT;
      base::win::ScopedBstr repetition_interval;
      switch (trigger_type) {
        case TRIGGER_TYPE_LOGON:
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
            NOTREACHED_IN_MIGRATION() << "Unknown TriggerType?";
          }
          break;
        default:
          NOTREACHED_IN_MIGRATION() << "Unknown TriggerType?";
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

        hr = repetition_pattern->put_Interval(repetition_interval.Get());
        if (FAILED(hr)) {
          PLOG(ERROR) << "Can't put 'Interval' to " << repetition_interval.Get()
                      << ". " << std::hex << hr;
          return false;
        }

        // The duration is the time to keep repeating intervals until the next
        // daily trigger.
        hr = repetition_pattern->put_Duration(
            base::win::ScopedBstr(kOneDayText).Get());
        if (FAILED(hr)) {
          PLOG(ERROR) << "Can't put 'Duration' to " << kOneDayText << ". "
                      << std::hex << hr;
          return false;
        }

        // Start 5 minutes from the current time.
        base::Time five_minutes_from_now(base::Time::NowFromSystemTime() +
                                         base::Minutes(5));
        base::win::ScopedBstr start_boundary(
            GetTimestampString(five_minutes_from_now));
        hr = trigger->put_StartBoundary(start_boundary.Get());
        if (FAILED(hr)) {
          PLOG(ERROR) << "Can't put 'StartBoundary' to " << start_boundary.Get()
                      << ". " << std::hex << hr;
          return false;
        }
      }

      if (trigger_type == TRIGGER_TYPE_LOGON) {
        Microsoft::WRL::ComPtr<ILogonTrigger> logon_trigger;
        hr = trigger.As(&logon_trigger);
        if (FAILED(hr)) {
          PLOG(ERROR) << "Can't query trigger for 'ILogonTrigger'. " << std::hex
                      << hr;
          return false;
        }

        if (!is_system) {
          hr = logon_trigger->put_UserId(user_name.Get());
          if (FAILED(hr)) {
            PLOG(ERROR) << "Can't put 'UserId'. " << std::hex << hr;
            return false;
          }
        }
      }
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

    // Quotes the command line before `put_Path`.
    hr = exec_action->put_Path(
        base::win::ScopedBstr(base::CommandLine::QuoteForCommandLineToArgvW(
                                  run_command.GetProgram().value()))
            .Get());
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't set path of exec action. " << std::hex << hr;
      return false;
    }

    base::win::ScopedBstr args(run_command.GetArgumentsString());
    hr = exec_action->put_Arguments(args.Get());
    if (FAILED(hr)) {
      PLOG(ERROR) << "Can't set arguments of exec action. " << std::hex << hr;
      return false;
    }

    DVLOG(2) << "Registering Task with XML: " << GetTaskXml(task.Get());

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    base::win::ScopedVariant user(user_name.Get());

    if (task_folder_) {
      hr = task_folder_->RegisterTaskDefinition(
          base::win::ScopedBstr(task_name).Get(), task.Get(),
          TASK_CREATE_OR_UPDATE,
          *user.AsInput(),  // Not really input, but API expect non-const.
          base::win::ScopedVariant::kEmptyVariant,
          is_system ? TASK_LOGON_SERVICE_ACCOUNT : TASK_LOGON_INTERACTIVE_TOKEN,
          base::win::ScopedVariant::kEmptyVariant, &registered_task);
      if (FAILED(hr)) {
        LOG(ERROR) << "RegisterTaskDefinition failed: " << std::hex << hr;
        LOG(ERROR) << "Task XML: " << GetTaskXml(task.Get());
        return false;
      }
    }

    VLOG(1) << __func__ << ":" << task_name << ": "
            << run_command.GetCommandLineString();
    return IsTaskRegistered(task_name);
  }

  bool StartTask(const std::wstring& task_name) override {
    if (!task_folder_) {
      return false;
    }

    if (IsTaskRunning(task_name)) {
      return true;
    }

    Microsoft::WRL::ComPtr<IRegisteredTask> registered_task;
    if (!GetTask(task_name, &registered_task)) {
      LOG(ERROR) << "GetTask failed.";
      return false;
    }

    HRESULT hr =
        registered_task->Run(base::win::ScopedVariant::kEmptyVariant, nullptr);
    if (FAILED(hr)) {
      LOG(ERROR) << "IRegisteredTask.Run failed. " << std::hex << hr;
      return false;
    }

    return true;
  }

  std::wstring GetTaskSubfolderName() override {
    return use_task_subfolders_ ? base::StrCat({GetTaskCompanyFolder(),
                                                L"\\" PRODUCT_FULLNAME_STRING})
                                : std::wstring();
  }

  void ForEachTaskWithPrefix(
      const std::wstring& prefix,
      base::FunctionRef<void(const std::wstring&)> callback) override {
    if (!task_folder_) {
      return;
    }

    std::vector<std::wstring> task_names;
    if (!GetTaskNameList(task_names)) {
      return;
    }

    for (const std::wstring& task_name : task_names) {
      if (base::StartsWith(task_name, prefix)) {
        callback(task_name);
      }
    }
  }

 private:
  // Helper class that lets us iterate over all registered tasks.
  class TaskIterator {
   public:
    explicit TaskIterator(ITaskFolder* task_folder) {
      CHECK(task_folder);
      HRESULT hr = task_folder->GetTasks(TASK_ENUM_HIDDEN, &tasks_);
      if (FAILED(hr)) {
        if (!IsFileOrPathNotFoundError(hr)) {
          LOG(ERROR) << "Failed to get tasks from folder." << std::hex << hr;
        }
        done_ = true;
        return;
      }
      hr = tasks_->get_Count(&num_tasks_);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to get the tasks count." << std::hex << hr;
        done_ = true;
        return;
      }
      Next();
    }

    // Increment to the next valid item in the task list. Skip entries for
    // which we cannot retrieve a name.
    void Next() {
      CHECK(!done_);
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
      name_ = std::wstring(task_name_bstr.Get() ? task_name_bstr.Get() : L"");
    }

    // Detach the currently active task and pass ownership to the caller.
    // After this method has been called, the -> operator must no longer be
    // used.
    IRegisteredTask* Detach() { return task_.Detach(); }

    // Provide access to the current task.
    IRegisteredTask* operator->() const {
      IRegisteredTask* result = task_.Get();
      CHECK(result);
      return result;
    }

    const std::wstring& name() const { return name_; }
    bool done() const { return done_; }

   private:
    Microsoft::WRL::ComPtr<IRegisteredTaskCollection> tasks_;
    Microsoft::WRL::ComPtr<IRegisteredTask> task_;
    std::wstring name_;
    LONG task_index_ = -1;
    LONG num_tasks_ = 0;
    bool done_ = false;
  };

  [[nodiscard]] Microsoft::WRL::ComPtr<ITaskService> GetTaskService() const {
    Microsoft::WRL::ComPtr<ITaskService> task_service;
    HRESULT hr =
        ::CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                           IID_PPV_ARGS(&task_service));
    if (FAILED(hr)) {
      PLOG(ERROR) << "CreateInstance failed for CLSID_TaskScheduler. "
                  << std::hex << hr;
      return nullptr;
    }
    hr = task_service->Connect(base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant,
                               base::win::ScopedVariant::kEmptyVariant);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to connect to task service. " << std::hex << hr;
      return nullptr;
    }

    PinModule(kV2Library);
    return task_service;
  }

  // Name of the company folder used to group the scheduled tasks in. System
  // task folders have a "System" suffix, and User task folders have a "User"
  // suffix.
  std::wstring GetTaskCompanyFolder() const {
    return base::StrCat({L"\\" COMPANY_SHORTNAME_STRING,
                         IsSystemInstall(scope_) ? L"System" : L"User"});
  }

  // Return the task with |task_name| and false if not found. |task| can be null
  // when only interested in task's existence.
  [[nodiscard]] bool GetTask(const std::wstring& task_name,
                             IRegisteredTask** task) {
    if (!task_folder_) {
      return false;
    }
    for (TaskIterator it(task_folder_.Get()); !it.done(); it.Next()) {
      if (::_wcsicmp(it.name().c_str(), task_name.c_str()) == 0) {
        if (task) {
          *task = it.Detach();
        }
        return true;
      }
    }
    return false;
  }

  // Return the description of the task.
  [[nodiscard]] HRESULT GetTaskDescription(IRegisteredTask* task,
                                           std::wstring& description) {
    if (!task) {
      return E_INVALIDARG;
    }

    base::win::ScopedBstr task_name_bstr;
    HRESULT hr = task->get_Name(task_name_bstr.Receive());
    std::wstring task_name =
        std::wstring(task_name_bstr.Get() ? task_name_bstr.Get() : L"");
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
    std::wstring(raw_description.Get() ? raw_description.Get() : L"")
        .swap(description);
    return ERROR_SUCCESS;
  }

  // Return all executable actions associated with the given task. Non-exec
  // actions are silently ignored.
  [[nodiscard]] bool GetTaskExecActions(IRegisteredTask* task,
                                        std::vector<TaskExecAction>& actions) {
    if (!task) {
      return false;
    }
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

    LONG actions_count = 0;
    hr = action_collection->get_Count(&actions_count);
    if (FAILED(hr)) {
      PLOG(ERROR) << "Failed to get number of actions, " << std::hex << hr;
      return false;
    }

    // Find and return as many exec actions as possible in |actions| and return
    // false if there were any errors on the way. Note that the indexing of
    // actions is 1-based.
    bool success = true;
    for (LONG action_index = 1;  // NOLINT
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
      if (action_type != ::TASK_ACTION_EXEC) {
        continue;
      }

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

      actions.push_back(
          {base::FilePath(application_path.Get() ? application_path.Get()
                                                 : L""),
           base::FilePath(working_dir.Get() ? working_dir.Get() : L""),
           std::wstring(parameters.Get() ? parameters.Get() : L"")});
    }
    return success;
  }

  // Return the log-on type required for the task's actions to be run.
  [[nodiscard]] HRESULT GetTaskLogonType(IRegisteredTask* task,
                                         uint32_t& logon_type) {
    if (!task) {
      return E_INVALIDARG;
    }

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
        logon_type = LOGON_INTERACTIVE;
        break;
      case TASK_LOGON_GROUP:     // fall-thru
      case TASK_LOGON_PASSWORD:  // fall-thru
      case TASK_LOGON_SERVICE_ACCOUNT:
        logon_type = LOGON_SERVICE;
        break;
      case TASK_LOGON_S4U:
        logon_type = LOGON_SERVICE | LOGON_S4U;
        break;
      case TASK_LOGON_INTERACTIVE_TOKEN_OR_PASSWORD:
        logon_type = LOGON_INTERACTIVE | LOGON_SERVICE;
        break;
      default:
        logon_type = LOGON_UNKNOWN;
        break;
    }
    return ERROR_SUCCESS;
  }

  // Return the branded task folder (e.g. \\Google\Updater).
  [[nodiscard]] Microsoft::WRL::ComPtr<ITaskFolder> GetUpdaterTaskFolder() {
    Microsoft::WRL::ComPtr<ITaskFolder> root_task_folder;
    HRESULT hr = task_service_->GetFolder(base::win::ScopedBstr(L"\\").Get(),
                                          &root_task_folder);
    if (FAILED(hr)) {
      LOG(ERROR) << "Can't get task service folder. " << std::hex << hr;
      return nullptr;
    }

    if (!use_task_subfolders_) {
      return root_task_folder;
    }

    // Try to find the folder first.
    Microsoft::WRL::ComPtr<ITaskFolder> folder;
    base::win::ScopedBstr task_subfolder_name(GetTaskSubfolderName());
    hr = root_task_folder->GetFolder(task_subfolder_name.Get(), &folder);

    // Try creating the folder if it wasn't there.
    if (IsFileOrPathNotFoundError(hr)) {
      // Use default SDDL.
      hr = root_task_folder->CreateFolder(
          task_subfolder_name.Get(), base::win::ScopedVariant::kEmptyVariant,
          &folder);

      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to create the folder: "
                   << task_subfolder_name.Get() << ", error: " << std::hex
                   << hr;
        return nullptr;
      }
    } else if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get the folder." << std::hex << hr;
      return nullptr;
    }

    return folder;
  }

  // Return the UserId of the task.
  [[nodiscard]] HRESULT GetTaskUserId(IRegisteredTask* task,
                                      std::wstring& user_id) {
    if (!task) {
      return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<ITaskDefinition> task_info;
    HRESULT hr = task->get_Definition(&task_info);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get definition: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    Microsoft::WRL::ComPtr<IPrincipal> iprincipal;
    hr = task_info->get_Principal(&iprincipal);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get principal: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    base::win::ScopedBstr raw_user_id;
    hr = iprincipal->get_UserId(raw_user_id.Receive());
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get UserId: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }
    std::wstring(raw_user_id.Get() ? raw_user_id.Get() : L"").swap(user_id);
    return ERROR_SUCCESS;
  }

  [[nodiscard]] HRESULT GetTaskTriggerTypes(IRegisteredTask* task,
                                            int& trigger_types) {
    if (!task) {
      return E_INVALIDARG;
    }

    Microsoft::WRL::ComPtr<ITaskDefinition> task_info;
    HRESULT hr = task->get_Definition(&task_info);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get definition: "
                 << logging::SystemErrorCodeToString(hr);
      return hr;
    }

    Microsoft::WRL::ComPtr<ITriggerCollection> trigger_collection;
    hr = task_info->get_Triggers(&trigger_collection);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get trigger collection: "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    LONG trigger_count = 0;
    hr = trigger_collection->get_Count(&trigger_count);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to get trigger collection count: "
                 << logging::SystemErrorCodeToString(hr);
      return false;
    }

    trigger_types = 0;
    for (LONG t = 1; t <= trigger_count; ++t) {
      Microsoft::WRL::ComPtr<ITrigger> trigger;
      hr = trigger_collection->get_Item(t, &trigger);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to get trigger: "
                   << logging::SystemErrorCodeToString(hr);
        return false;
      }

      TASK_TRIGGER_TYPE2 task_trigger_type = {};
      hr = trigger->get_Type(&task_trigger_type);
      if (FAILED(hr)) {
        LOG(ERROR) << "Failed to get trigger type: "
                   << logging::SystemErrorCodeToString(hr);
        return false;
      }

      switch (task_trigger_type) {
        case TASK_TRIGGER_LOGON:
          trigger_types |= TRIGGER_TYPE_LOGON;
          break;
        case TASK_TRIGGER_REGISTRATION:
          trigger_types |= TRIGGER_TYPE_NOW;
          break;
        case TASK_TRIGGER_DAILY: {
          Microsoft::WRL::ComPtr<IRepetitionPattern> repetition_pattern;
          hr = trigger->get_Repetition(&repetition_pattern);
          if (FAILED(hr)) {
            LOG(ERROR) << "Failed to get 'Repetition'. "
                       << logging::SystemErrorCodeToString(hr);
            return false;
          }

          base::win::ScopedBstr repetition_interval;
          hr = repetition_pattern->get_Interval(repetition_interval.Receive());
          if (FAILED(hr)) {
            LOG(ERROR) << "Failed to get 'Interval': "
                       << logging::SystemErrorCodeToString(hr);
            return false;
          }

          if (base::EqualsCaseInsensitiveASCII(repetition_interval.Get(),
                                               kFiveHoursText)) {
            trigger_types |= TRIGGER_TYPE_EVERY_FIVE_HOURS;
          } else if (base::EqualsCaseInsensitiveASCII(repetition_interval.Get(),
                                                      kOneHourText)) {
            trigger_types |= TRIGGER_TYPE_HOURLY;
          } else {
            NOTREACHED_IN_MIGRATION() << "Unknown TriggerType for interval: "
                                      << repetition_interval.Get();
          }
          break;
        }
        default:
          NOTREACHED_IN_MIGRATION()
              << "Unknown task trigger type: " << task_trigger_type;
      }
    }

    return S_OK;
  }

  // If the task folder specified by |folder_name| is empty, try to delete it.
  // Ignore failures. Returns true if the folder is successfully deleted.
  [[nodiscard]] bool DeleteFolderIfEmpty(const std::wstring& folder_name) {
    // Try deleting if empty. Race conditions here should be handled by the API.
    Microsoft::WRL::ComPtr<ITaskFolder> root_task_folder;
    HRESULT hr = task_service_->GetFolder(base::win::ScopedBstr(L"\\").Get(),
                                          &root_task_folder);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed get root folder. " << std::hex << hr;
      return false;
    }

    Microsoft::WRL::ComPtr<ITaskFolder> task_folder;
    hr = root_task_folder->GetFolder(base::win::ScopedBstr(folder_name).Get(),
                                     &task_folder);
    if (FAILED(hr)) {
      // If we failed because the task folder is not present our job is done.
      if (IsFileOrPathNotFoundError(hr)) {
        return true;
      }
      LOG(ERROR) << "Failed to get sub-folder. " << std::hex << hr;
      return false;
    }

    // Check tasks and subfolders first to see non empty
    Microsoft::WRL::ComPtr<ITaskFolderCollection> subfolders;
    hr = task_folder->GetFolders(0, &subfolders);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to enumerate sub-folders. " << std::hex << hr;
      return false;
    }

    LONG item_count = 0;
    subfolders->get_Count(&item_count);
    if (FAILED(hr) || item_count > 0) {
      return false;
    }

    Microsoft::WRL::ComPtr<IRegisteredTaskCollection> tasks;
    hr = task_folder->GetTasks(TASK_ENUM_HIDDEN, &tasks);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to enumerate tasks. " << std::hex << hr;
      return false;
    }

    item_count = 0;
    tasks->get_Count(&item_count);
    if (FAILED(hr) || item_count > 0) {
      return false;
    }

    hr = root_task_folder->DeleteFolder(
        base::win::ScopedBstr(folder_name).Get(), 0);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed get delete the sub folder. " << std::hex << hr;
      return false;
    }

    return true;
  }

  ~TaskSchedulerV2() override = default;

  const UpdaterScope scope_;
  const bool use_task_subfolders_;

  Microsoft::WRL::ComPtr<ITaskService> task_service_;

  // Folder in which all updater scheduled tasks are grouped.
  Microsoft::WRL::ComPtr<ITaskFolder> task_folder_;
};

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
[[nodiscard]] scoped_refptr<TaskScheduler> TaskScheduler::CreateInstance(
    UpdaterScope scope,
    bool use_task_subfolders) {
  return TaskSchedulerV2::CreateInstance(scope, use_task_subfolders);
}

TaskScheduler::TaskScheduler() = default;

std::ostream& operator<<(std::ostream& stream,
                         const TaskScheduler::TaskExecAction& t) {
  return stream << "TaskExecAction: application_path: "
                << t.application_path.value()
                << ", working_dir: " << t.working_dir.value()
                << ", arguments: " << t.arguments;
}

std::ostream& operator<<(std::ostream& stream,
                         const TaskScheduler::TaskInfo& t) {
  stream << "TaskInfo: name: " << t.name << ", description: " << t.description
         << ", exec_actions: ";

  for (const auto& exec_action : t.exec_actions) {
    stream << ", exec_action: " << exec_action;
  }

  return stream << ", logon_type: " << base::StringPrintf("0x%x", t.logon_type)
                << ", user_id: " << t.user_id;
}

}  // namespace updater
