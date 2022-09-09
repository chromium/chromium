// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_requests_proxy.h"

#include <sddl.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/registry.h"
#include "chrome/chrome_cleaner/strings/wstring_embedded_nulls.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

namespace {

template <typename ResultType>
void SaveBoolAndCopyableDataCallback(bool* out_success,
                                     ResultType* out_result,
                                     base::WaitableEvent* async_call_done_event,
                                     bool success,
                                     const ResultType& result) {
  *out_success = success;
  *out_result = result;
  async_call_done_event->Signal();
}

void SaveGetFileAttributesCallback(uint32_t* out_result,
                                   uint32_t* out_attributes,
                                   base::WaitableEvent* async_call_done_event,
                                   uint32_t result,
                                   uint32_t attributes) {
  *out_result = result;
  *out_attributes = attributes;
  async_call_done_event->Signal();
}

void SaveOpenReadOnlyRegistryCallback(
    uint32_t* out_return_code,
    HANDLE* result_holder,
    base::WaitableEvent* async_call_done_event,
    uint32_t return_code,
    HANDLE handle) {
  *out_return_code = return_code;
  *result_holder = std::move(handle);
  async_call_done_event->Signal();
}

void SaveBoolAndScheduledTasksCallback(
    bool* out_success,
    std::vector<TaskScheduler::TaskInfo>* out_tasks,
    base::WaitableEvent* async_call_done_event,
    bool in_success,
    std::vector<mojom::ScheduledTaskPtr> in_tasks) {
  out_tasks->clear();
  out_tasks->reserve(in_tasks.size());
  for (mojom::ScheduledTaskPtr& in_task : in_tasks) {
    TaskScheduler::TaskInfo out_task;
    out_task.name = std::move(in_task->name);
    out_task.description = std::move(in_task->description);
    out_task.exec_actions.reserve(in_task->actions.size());

    for (mojom::ScheduledTaskActionPtr& in_task_action : in_task->actions) {
      TaskScheduler::TaskExecAction out_action;
      out_action.application_path = std::move(in_task_action->path);
      out_action.working_dir = std::move(in_task_action->working_dir);
      out_action.arguments = std::move(in_task_action->arguments);
      out_task.exec_actions.push_back(std::move(out_action));
    }
    out_tasks->push_back(std::move(out_task));
  }

  *out_success = in_success;
  async_call_done_event->Signal();
}

void SaveUserInformationCallback(bool* out_success,
                                 mojom::UserInformation* out_result,
                                 base::WaitableEvent* async_call_done_event,
                                 bool in_success,
                                 mojom::UserInformationPtr in_result) {
  *out_success = in_success;
  out_result->name = std::move(in_result->name);
  out_result->domain = std::move(in_result->domain);
  out_result->account_type = in_result->account_type;
  async_call_done_event->Signal();
}

}  // namespace

EngineRequestsProxy::EngineRequestsProxy(
    mojo::PendingAssociatedRemote<mojom::EngineRequests> engine_requests,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : engine_requests_(std::move(engine_requests)), task_runner_(task_runner) {}

void EngineRequestsProxy::UnbindRequestsRemote() {
  engine_requests_.reset();
}

uint32_t EngineRequestsProxy::GetFileAttributes(const base::FilePath& file_path,
                                                uint32_t* attributes) {
  if (!attributes) {
    LOG(ERROR) << "GetFileAttributes given a null pointer |attributes|";
    return NULL_DATA_HANDLE;
  }
  uint32_t result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetFileAttributes,
                     base::Unretained(this), file_path),
      base::BindOnce(SaveGetFileAttributesCallback, &result, attributes));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    *attributes = INVALID_FILE_ATTRIBUTES;
    return SandboxErrorCode::INTERNAL_ERROR;
  }
  return result;
}

bool EngineRequestsProxy::GetKnownFolderPath(mojom::KnownFolder folder_id,
                                             base::FilePath* folder_path) {
  if (folder_path == nullptr) {
    LOG(ERROR) << "GetKnownFolderPathCallback given a null folder_path";
    return false;
  }
  bool result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetKnownFolderPath,
                     base::Unretained(this), folder_id),
      base::BindOnce(&SaveBoolAndCopyableDataCallback<base::FilePath>, &result,
                     folder_path));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetProcesses(
    std::vector<base::ProcessId>* processes) {
  if (processes == nullptr) {
    LOG(ERROR) << "GetProcessesCallback received a null processes";
    return false;
  }
  bool success = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetProcesses,
                     base::Unretained(this)),
      base::BindOnce(
          &SaveBoolAndCopyableDataCallback<std::vector<base::ProcessId>>,
          &success, processes));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return success;
}

bool EngineRequestsProxy::GetTasks(
    std::vector<TaskScheduler::TaskInfo>* task_list) {
  if (task_list == nullptr) {
    LOG(ERROR) << "GetTasksCallback received a null task_list";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetTasks,
                     base::Unretained(this)),
      base::BindOnce(&SaveBoolAndScheduledTasksCallback, &result, task_list));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetProcessImagePath(base::ProcessId pid,
                                              base::FilePath* image_path) {
  if (image_path == nullptr) {
    LOG(ERROR) << "GetProcessImagePathCallback received a null file_info";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetProcessImagePath,
                     base::Unretained(this), pid),
      base::BindOnce(&SaveBoolAndCopyableDataCallback<base::FilePath>, &result,
                     image_path));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetLoadedModules(base::ProcessId pid,
                                           std::vector<std::wstring>* modules) {
  if (modules == nullptr) {
    LOG(ERROR) << "GetLoadedModulesCallback received a null modules";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetLoadedModules,
                     base::Unretained(this), pid),
      base::BindOnce(
          &SaveBoolAndCopyableDataCallback<std::vector<std::wstring>>, &result,
          modules));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetProcessCommandLine(base::ProcessId pid,
                                                std::wstring* command_line) {
  if (command_line == nullptr) {
    LOG(ERROR) << "GetProcessCommandLineCallback received a null command_line";
    return false;
  }
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetProcessCommandLine,
                     base::Unretained(this), pid),
      base::BindOnce(&SaveBoolAndCopyableDataCallback<std::wstring>, &result,
                     command_line));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool EngineRequestsProxy::GetUserInfoFromSID(
    const SID* const sid,
    mojom::UserInformation* user_info) {
  if (user_info == nullptr) {
    LOG(ERROR) << "GetUserInfoFromSIDCallback given a null buffer";
    return false;
  }

  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxGetUserInfoFromSID,
                     base::Unretained(this), sid),
      base::BindOnce(&SaveUserInformationCallback, &result, user_info));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

uint32_t EngineRequestsProxy::OpenReadOnlyRegistry(HANDLE root_key,
                                                   const std::wstring& sub_key,
                                                   uint32_t dw_access,
                                                   HANDLE* registry_handle) {
  uint32_t return_code;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxOpenReadOnlyRegistry,
                     base::Unretained(this), root_key, sub_key, dw_access),
      base::BindOnce(&SaveOpenReadOnlyRegistryCallback, &return_code,
                     registry_handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return return_code;
}

uint32_t EngineRequestsProxy::NtOpenReadOnlyRegistry(
    HANDLE root_key,
    const WStringEmbeddedNulls& sub_key,
    uint32_t dw_access,
    HANDLE* registry_handle) {
  uint32_t return_code;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineRequestsProxy::SandboxNtOpenReadOnlyRegistry,
                     base::Unretained(this), root_key, sub_key, dw_access),
      base::BindOnce(&SaveOpenReadOnlyRegistryCallback, &return_code,
                     registry_handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    if (registry_handle)
      *registry_handle = INVALID_HANDLE_VALUE;
    return call_status.error_code;
  }
  return return_code;
}

EngineRequestsProxy::EngineRequestsProxy() = default;

EngineRequestsProxy::~EngineRequestsProxy() = default;

MojoCallStatus EngineRequestsProxy::SandboxGetFileAttributes(
    const base::FilePath& file_path,
    mojom::EngineRequests::SandboxGetFileAttributesCallback result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetFileAttributes called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxGetFileAttributes(file_path,
                                             std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetKnownFolderPath(
    mojom::KnownFolder folder_id,
    mojom::EngineRequests::SandboxGetKnownFolderPathCallback result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetKnownFolderPath called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxGetKnownFolderPath(folder_id,
                                              std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetProcesses(
    mojom::EngineRequests::SandboxGetProcessesCallback result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetProcesses called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxGetProcesses(std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetTasks(
    mojom::EngineRequests::SandboxGetTasksCallback result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetTasks called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxGetTasks(std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetProcessImagePath(
    base::ProcessId pid,
    mojom::EngineRequests::SandboxGetProcessImagePathCallback result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetProcessImagePath called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxGetProcessImagePath(pid, std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetLoadedModules(
    base::ProcessId pid,
    mojom::EngineRequests::SandboxGetLoadedModulesCallback result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetLoadedModules called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxGetLoadedModules(pid, std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetProcessCommandLine(
    base::ProcessId pid,
    mojom::EngineRequestsProxy::SandboxGetProcessCommandLineCallback
        result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetProcessCommandLine called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxGetProcessCommandLine(pid,
                                                 std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxGetUserInfoFromSID(
    const SID* const sid,
    mojom::EngineRequests::SandboxGetUserInfoFromSIDCallback result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxGetUserInfoFromSID called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  wchar_t* sid_buffer = nullptr;
  if (!::ConvertSidToStringSidW(const_cast<SID*>(sid), &sid_buffer)) {
    PLOG(ERROR) << "Unable to convert |sid| to a string.";
    return MojoCallStatus::Failure(SandboxErrorCode::BAD_SID);
  }

  auto mojom_string_sid = mojom::StringSid::New(sid_buffer);
  LocalFree(sid_buffer);

  engine_requests_->SandboxGetUserInfoFromSID(std::move(mojom_string_sid),
                                              std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxOpenReadOnlyRegistry(
    HANDLE root_key,
    const std::wstring& sub_key,
    uint32_t dw_access,
    mojom::EngineRequests::SandboxOpenReadOnlyRegistryCallback
        result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxOpenReadOnlyRegistry called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxOpenReadOnlyRegistry(root_key, sub_key, dw_access,
                                                std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus EngineRequestsProxy::SandboxNtOpenReadOnlyRegistry(
    HANDLE root_key,
    const WStringEmbeddedNulls& sub_key,
    uint32_t dw_access,
    mojom::EngineRequests::SandboxNtOpenReadOnlyRegistryCallback
        result_callback) {
  if (!engine_requests_.is_bound()) {
    LOG(ERROR) << "SandboxNtOpenReadOnlyRegistry called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  engine_requests_->SandboxNtOpenReadOnlyRegistry(root_key, sub_key, dw_access,
                                                  std::move(result_callback));

  return MojoCallStatus::Success();
}

}  // namespace chrome_cleaner
