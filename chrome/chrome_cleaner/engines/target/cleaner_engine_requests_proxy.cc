// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/cleaner_engine_requests_proxy.h"

#include <utility>
#include <vector>

#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/chrome_cleaner/engines/target/engine_commands_impl.h"
#include "chrome/chrome_cleaner/strings/wstring_embedded_nulls.h"

namespace chrome_cleaner {

namespace {

void SaveBoolCallback(bool* out_result,
                      base::WaitableEvent* async_call_done_event,
                      bool result) {
  *out_result = result;
  async_call_done_event->Signal();
}

}  // namespace

CleanerEngineRequestsProxy::CleanerEngineRequestsProxy(
    mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests> requests,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : requests_(std::move(requests)), task_runner_(task_runner) {}

void CleanerEngineRequestsProxy::UnbindRequestsRemote() {
  requests_.reset();
}

bool CleanerEngineRequestsProxy::DeleteFile(const base::FilePath& file_name) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxDeleteFile,
                     base::Unretained(this), file_name),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool CleanerEngineRequestsProxy::DeleteFilePostReboot(
    const base::FilePath& file_name) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxDeleteFilePostReboot,
                     base::Unretained(this), file_name),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool CleanerEngineRequestsProxy::NtDeleteRegistryKey(
    const WStringEmbeddedNulls& key) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxNtDeleteRegistryKey,
                     base::Unretained(this), key),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool CleanerEngineRequestsProxy::NtDeleteRegistryValue(
    const WStringEmbeddedNulls& key,
    const WStringEmbeddedNulls& value_name) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxNtDeleteRegistryValue,
                     base::Unretained(this), key, value_name),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool CleanerEngineRequestsProxy::NtChangeRegistryValue(
    const WStringEmbeddedNulls& key,
    const WStringEmbeddedNulls& value_name,
    const WStringEmbeddedNulls& new_value) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxNtChangeRegistryValue,
                     base::Unretained(this), key, value_name, new_value),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool CleanerEngineRequestsProxy::DeleteService(const std::wstring& name) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxDeleteService,
                     base::Unretained(this), name),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool CleanerEngineRequestsProxy::DeleteTask(const std::wstring& name) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxDeleteTask,
                     base::Unretained(this), name),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

bool CleanerEngineRequestsProxy::TerminateProcess(base::ProcessId process_id) {
  bool result = false;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&CleanerEngineRequestsProxy::SandboxTerminateProcess,
                     base::Unretained(this), process_id),
      base::BindOnce(&SaveBoolCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return false;
  }
  return result;
}

CleanerEngineRequestsProxy::CleanerEngineRequestsProxy() = default;

CleanerEngineRequestsProxy::~CleanerEngineRequestsProxy() = default;

MojoCallStatus CleanerEngineRequestsProxy::SandboxDeleteFile(
    const base::FilePath& path,
    mojom::CleanerEngineRequests::SandboxDeleteFileCallback result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxDeleteFile called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxDeleteFile(path, std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus CleanerEngineRequestsProxy::SandboxDeleteFilePostReboot(
    const base::FilePath& path,
    mojom::CleanerEngineRequests::SandboxDeleteFilePostRebootCallback
        result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxDeleteFilePostReboot called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxDeleteFilePostReboot(path, std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus CleanerEngineRequestsProxy::SandboxNtDeleteRegistryKey(
    const WStringEmbeddedNulls& key,
    mojom::CleanerEngineRequests::SandboxNtDeleteRegistryKeyCallback
        result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxNtDeleteRegistryKey called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxNtDeleteRegistryKey(key, std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus CleanerEngineRequestsProxy::SandboxNtDeleteRegistryValue(
    const WStringEmbeddedNulls& key,
    const WStringEmbeddedNulls& value_name,
    mojom::CleanerEngineRequests::SandboxNtDeleteRegistryValueCallback
        result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxNtDeleteRegistryValue called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxNtDeleteRegistryValue(key, value_name,
                                          std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus CleanerEngineRequestsProxy::SandboxNtChangeRegistryValue(
    const WStringEmbeddedNulls& key,
    const WStringEmbeddedNulls& value_name,
    const WStringEmbeddedNulls& new_value,
    mojom::CleanerEngineRequests::SandboxNtChangeRegistryValueCallback
        result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxNtChangeRegistryValue called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxNtChangeRegistryValue(key, value_name, new_value,
                                          std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus CleanerEngineRequestsProxy::SandboxDeleteService(
    const std::wstring& name,
    mojom::CleanerEngineRequests::SandboxDeleteServiceCallback
        result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxDeleteService called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxDeleteService(name, std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus CleanerEngineRequestsProxy::SandboxDeleteTask(
    const std::wstring& name,
    mojom::CleanerEngineRequests::SandboxDeleteTaskCallback result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxDeleteTask called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxDeleteTask(name, std::move(result_callback));
  return MojoCallStatus::Success();
}

MojoCallStatus CleanerEngineRequestsProxy::SandboxTerminateProcess(
    uint32_t process_id,
    mojom::CleanerEngineRequests::SandboxTerminateProcessCallback
        result_callback) {
  if (!requests_.is_bound()) {
    LOG(ERROR) << "SandboxTerminateProcess called without bound remote";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  requests_->SandboxTerminateProcess(process_id, std::move(result_callback));

  return MojoCallStatus::Success();
}

}  // namespace chrome_cleaner
