// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/engine_requests_impl.h"

// Windows include must be first for the code to compile.
// clang-format off
#include <windows.h>
#include <sddl.h>
// clang-format on

#include <set>
#include <utility>

#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/chrome_cleaner/engines/broker/scanner_sandbox_interface.h"
#include "chrome/chrome_cleaner/os/registry.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

namespace {

void CloseRegistryHandle(HANDLE handle) {
  if (IsPredefinedRegistryHandle(handle))
    return;
  // Call ::CloseHandle instead of ::RegCloseKey as it's not always possible to
  // distinguish whether the passed handle was returned by RegOpenKey or by
  // NtOpenKey.
  // TODO(veranika): track registry handle types and close them correctly.
  bool success = ::CloseHandle(handle);
  PLOG_IF(ERROR, !success) << "Failed to close handle";
}

void ForwardOpenRegistryResult(
    mojom::EngineRequests::SandboxOpenReadOnlyRegistryCallback result_callback,
    uint32_t return_code,
    HKEY handle) {
  std::move(result_callback).Run(return_code, handle);

  if (return_code == 0) {
    // Close |handle| only on successful operation, as otherwise it's not set.
    LSTATUS status = ::RegCloseKey(handle);
    LOG_IF(ERROR, status != ERROR_SUCCESS)
        << "Failed to close a registry key: "
        << logging::SystemErrorCodeToString(status);
  }
}

void ForwardNtOpenRegistryResult(
    mojom::EngineRequests::SandboxNtOpenReadOnlyRegistryCallback callback,
    uint32_t return_code,
    HANDLE handle) {
  std::move(callback).Run(return_code, handle);
  CloseRegistryHandle(handle);
}

}  // namespace

EngineRequestsImpl::EngineRequestsImpl(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    InterfaceMetadataObserver* metadata_observer)
    : mojo_task_runner_(mojo_task_runner),
      metadata_observer_(metadata_observer) {}

EngineRequestsImpl::~EngineRequestsImpl() = default;

void EngineRequestsImpl::Bind(
    mojo::PendingAssociatedRemote<mojom::EngineRequests>* remote) {
  if (receiver_.is_bound())
    receiver_.reset();

  receiver_.Bind(remote->InitWithNewEndpointAndPassReceiver());
  // There's no need to call set_disconnect_handler on this since it's an
  // associated interface. Any errors will be handled on the main EngineCommands
  // interface.
}

void EngineRequestsImpl::SandboxGetFileAttributes(
    const base::FilePath& file_name,
    SandboxGetFileAttributesCallback result_callback) {
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&EngineRequestsImpl::GetFileAttributes,
                                base::Unretained(this), file_name,
                                std::move(result_callback)));
}

void EngineRequestsImpl::GetFileAttributes(
    const base::FilePath& file_name,
    SandboxGetFileAttributesCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  uint32_t attributes = INVALID_FILE_ATTRIBUTES;
  uint32_t result =
      chrome_cleaner_sandbox::SandboxGetFileAttributes(file_name, &attributes);
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), result, attributes));
}

void EngineRequestsImpl::SandboxGetKnownFolderPath(
    mojom::KnownFolder folder_id,
    SandboxGetKnownFolderPathCallback result_callback) {
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&EngineRequestsImpl::GetKnownFolderPath,
                                base::Unretained(this), folder_id,
                                std::move(result_callback)));
}

void EngineRequestsImpl::GetKnownFolderPath(
    mojom::KnownFolder folder_id,
    SandboxGetKnownFolderPathCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  base::FilePath folder_path;
  bool result = chrome_cleaner_sandbox::SandboxGetKnownFolderPath(folder_id,
                                                                  &folder_path);
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), result, folder_path));
}

void EngineRequestsImpl::SandboxGetProcesses(
    SandboxGetProcessesCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineRequestsImpl::GetProcesses, base::Unretained(this),
                     std::move(result_callback)));
}

void EngineRequestsImpl::GetProcesses(
    SandboxGetProcessesCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  std::vector<base::ProcessId> processes;
  bool result = chrome_cleaner_sandbox::SandboxGetProcesses(&processes);
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), result, std::move(processes)));
}

void EngineRequestsImpl::SandboxGetTasks(
    SandboxGetTasksCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineRequestsImpl::GetTasks, base::Unretained(this),
                     std::move(result_callback)));
}

void EngineRequestsImpl::GetTasks(SandboxGetTasksCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  std::vector<TaskScheduler::TaskInfo> tasks;
  bool result = chrome_cleaner_sandbox::SandboxGetTasks(&tasks);

  std::vector<mojom::ScheduledTaskPtr> mojo_tasks;
  for (TaskScheduler::TaskInfo& task : tasks) {
    std::vector<mojom::ScheduledTaskActionPtr> mojo_actions;
    for (TaskScheduler::TaskExecAction& action : task.exec_actions) {
      auto mojo_action = mojom::ScheduledTaskAction::New(
          std::move(action.application_path), std::move(action.working_dir),
          std::move(action.arguments));
      mojo_actions.push_back(std::move(mojo_action));
    }
    auto mojo_task = mojom::ScheduledTask::New(std::move(task.name),
                                               std::move(task.description),
                                               std::move(mojo_actions));
    mojo_tasks.push_back(std::move(mojo_task));
  }

  mojo_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(result_callback), result,
                                             std::move(mojo_tasks)));
}

void EngineRequestsImpl::SandboxGetProcessImagePath(
    base::ProcessId pid,
    SandboxGetProcessImagePathCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineRequestsImpl::GetProcessImagePath,
                     base::Unretained(this), pid, std::move(result_callback)));
}

void EngineRequestsImpl::GetProcessImagePath(
    base::ProcessId pid,
    SandboxGetProcessImagePathCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  base::FilePath image_path;
  bool result =
      chrome_cleaner_sandbox::SandboxGetProcessImagePath(pid, &image_path);

  mojo_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(result_callback), result,
                                             std::move(image_path)));
}

void EngineRequestsImpl::SandboxGetLoadedModules(
    base::ProcessId pid,
    SandboxGetLoadedModulesCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineRequestsImpl::GetLoadedModules,
                     base::Unretained(this), pid, std::move(result_callback)));
}

void EngineRequestsImpl::GetLoadedModules(
    base::ProcessId pid,
    SandboxGetLoadedModulesCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  std::set<base::string16> modules;
  bool result = chrome_cleaner_sandbox::SandboxGetLoadedModules(pid, &modules);

  std::vector<base::string16> modules_list(modules.begin(), modules.end());
  mojo_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(result_callback), result,
                                             std::move(modules_list)));
}

void EngineRequestsImpl::SandboxGetProcessCommandLine(
    base::ProcessId pid,
    SandboxGetProcessCommandLineCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineRequestsImpl::GetProcessCommandLine,
                     base::Unretained(this), pid, std::move(result_callback)));
}

void EngineRequestsImpl::GetProcessCommandLine(
    base::ProcessId pid,
    SandboxGetProcessCommandLineCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  base::string16 command_line;
  bool result =
      chrome_cleaner_sandbox::SandboxGetProcessCommandLine(pid, &command_line);

  mojo_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(result_callback), result,
                                             std::move(command_line)));
}

void EngineRequestsImpl::SandboxGetUserInfoFromSID(
    mojom::StringSidPtr string_sid,
    SandboxGetUserInfoFromSIDCallback result_callback) {
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&EngineRequestsImpl::GetUserInfoFromSID,
                                base::Unretained(this), std::move(string_sid),
                                std::move(result_callback)));
}

void EngineRequestsImpl::GetUserInfoFromSID(
    mojom::StringSidPtr string_sid,
    SandboxGetUserInfoFromSIDCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  PSID sid = nullptr;
  if (!::ConvertStringSidToSid(string_sid->value.c_str(), &sid)) {
    PLOG(ERROR) << "Failed to convert string sid to sid";
  }
  mojom::UserInformationPtr user_info = mojom::UserInformation::New();
  bool result = chrome_cleaner_sandbox::SandboxGetUserInfoFromSID(
      static_cast<SID*>(sid), user_info.get());
  ::LocalFree(sid);

  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), result, std::move(user_info)));
}

void EngineRequestsImpl::SandboxOpenReadOnlyRegistry(
    HANDLE root_key_handle,
    const base::string16& sub_key,
    uint32_t dw_access,
    SandboxOpenReadOnlyRegistryCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineRequestsImpl::OpenReadOnlyRegistry,
                     base::Unretained(this), root_key_handle, sub_key,
                     dw_access, std::move(result_callback)));
}

void EngineRequestsImpl::OpenReadOnlyRegistry(
    HANDLE root_key_handle,
    const base::string16& sub_key,
    uint32_t dw_access,
    SandboxOpenReadOnlyRegistryCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  HKEY handle;
  uint32_t return_code = chrome_cleaner_sandbox::SandboxOpenReadOnlyRegistry(
      root_key_handle, sub_key, dw_access, &handle);

  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(ForwardOpenRegistryResult, std::move(result_callback),
                     return_code, handle));

  // Close handles as Mojo doesn't own them. ForwardOpenRegistryResult will
  // close result handle.
  // TODO(veranika): clearly define ownership and find a better fix.
  CloseRegistryHandle(root_key_handle);
}

void EngineRequestsImpl::SandboxNtOpenReadOnlyRegistry(
    HANDLE root_key_handle,
    const String16EmbeddedNulls& sub_key,
    uint32_t dw_access,
    SandboxNtOpenReadOnlyRegistryCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineRequestsImpl::NtOpenReadOnlyRegistry,
                     base::Unretained(this), root_key_handle, sub_key,
                     dw_access, std::move(result_callback)));
}

void EngineRequestsImpl::NtOpenReadOnlyRegistry(
    HANDLE root_key_handle,
    const String16EmbeddedNulls& sub_key,
    uint32_t dw_access,
    SandboxNtOpenReadOnlyRegistryCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);

  HANDLE handle;
  uint32_t return_code = chrome_cleaner_sandbox::SandboxNtOpenReadOnlyRegistry(
      root_key_handle, sub_key, dw_access, &handle);

  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(ForwardNtOpenRegistryResult, std::move(result_callback),
                     return_code, handle));

  // Close handles as Mojo doesn't own them. ForwardOpenRegistryResult will
  // close result handle.
  // TODO(veranika): clearly define ownership and find a better fix.
  CloseRegistryHandle(root_key_handle);
}

}  // namespace chrome_cleaner
