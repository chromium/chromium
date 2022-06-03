// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"

namespace chrome_cleaner {

namespace {

void SaveFindFirstFileCallback(uint32_t* out_result,
                               LPWIN32_FIND_DATAW lpFindFileData,
                               FindFileHandle* out_handle,
                               base::WaitableEvent* async_call_done_event,
                               uint32_t result,
                               mojom::FindFileDataPtr data,
                               mojom::FindHandlePtr handle) {
  *out_result = result;
  // The layout for WIN32_FIND_DATAW is the same in the broker and the target
  // processes.
  memcpy(lpFindFileData, data->data.data(), sizeof(WIN32_FIND_DATAW));
  *out_handle = handle->find_handle;
  async_call_done_event->Signal();
}

void SaveFindNextFileCallback(uint32_t* out_result,
                              LPWIN32_FIND_DATAW lpFindFileData,
                              base::WaitableEvent* async_call_done_event,
                              uint32_t result,
                              mojom::FindFileDataPtr data) {
  *out_result = result;
  // The layout for WIN32_FIND_DATAW is the same in the broker and the target
  // processes.
  memcpy(lpFindFileData, data->data.data(), sizeof(WIN32_FIND_DATAW));
  async_call_done_event->Signal();
}

void SaveFindCloseCallback(uint32_t* out_result,
                           base::WaitableEvent* async_call_done_event,
                           uint32_t result) {
  *out_result = result;
  async_call_done_event->Signal();
}

void SaveOpenReadOnlyFileCallback(base::win::ScopedHandle* result_holder,
                                  base::WaitableEvent* async_call_done_event,
                                  mojo::PlatformHandle handle) {
  *result_holder = handle.TakeHandle();
  async_call_done_event->Signal();
}

}  // namespace

EngineFileRequestsProxy::EngineFileRequestsProxy(
    mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : file_requests_(std::move(file_requests)), task_runner_(task_runner) {}

EngineFileRequestsProxy::EngineFileRequestsProxy() = default;

EngineFileRequestsProxy::~EngineFileRequestsProxy() = default;

uint32_t EngineFileRequestsProxy::FindFirstFile(
    const base::FilePath& path,
    LPWIN32_FIND_DATAW lpFindFileData,
    FindFileHandle* handle) {
  if (lpFindFileData == nullptr) {
    LOG(ERROR) << "FindFirstFileCallback got a null lpFindFileData";
    return SandboxErrorCode::NULL_DATA_HANDLE;
  }
  if (handle == nullptr) {
    LOG(ERROR) << "FindFirstFileCallback got a null handle";
    return SandboxErrorCode::NULL_FIND_HANDLE;
  }

  uint32_t result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxFindFirstFile,
                     base::Unretained(this), path),
      base::BindOnce(&SaveFindFirstFileCallback, &result, lpFindFileData,
                     handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return result;
}

uint32_t EngineFileRequestsProxy::FindNextFile(
    FindFileHandle hFindFile,
    LPWIN32_FIND_DATAW lpFindFileData) {
  if (lpFindFileData == nullptr) {
    LOG(ERROR) << "FindNextFileCallback received a null lpFindFileData";
    return SandboxErrorCode::NULL_DATA_HANDLE;
  }

  uint32_t result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxFindNextFile,
                     base::Unretained(this), hFindFile),
      base::BindOnce(&SaveFindNextFileCallback, &result, lpFindFileData));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return result;
}

uint32_t EngineFileRequestsProxy::FindClose(FindFileHandle hFindFile) {
  uint32_t result;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxFindClose,
                     base::Unretained(this), hFindFile),
      base::BindOnce(&SaveFindCloseCallback, &result));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR) {
    return call_status.error_code;
  }
  return result;
}

base::win::ScopedHandle EngineFileRequestsProxy::OpenReadOnlyFile(
    const base::FilePath& path,
    uint32_t dwFlagsAndAttributes) {
  base::win::ScopedHandle handle;
  MojoCallStatus call_status = SyncSandboxRequest(
      this,
      base::BindOnce(&EngineFileRequestsProxy::SandboxOpenReadOnlyFile,
                     base::Unretained(this), path, dwFlagsAndAttributes),
      base::BindOnce(&SaveOpenReadOnlyFileCallback, &handle));
  if (call_status.state == MojoCallStatus::MOJO_CALL_ERROR)
    return base::win::ScopedHandle(INVALID_HANDLE_VALUE);
  return handle;
}

void EngineFileRequestsProxy::UnbindRequestsRemote() {
  file_requests_.reset();
}

MojoCallStatus EngineFileRequestsProxy::SandboxFindFirstFile(
    const base::FilePath& path,
    mojom::EngineFileRequests::SandboxFindFirstFileCallback result_callback) {
  if (!file_requests_.is_bound()) {
    LOG(ERROR) << "SandboxFindFirstFile called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  file_requests_->SandboxFindFirstFile(path, std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineFileRequestsProxy::SandboxFindNextFile(
    FindFileHandle handle,
    mojom::EngineFileRequests::SandboxFindNextFileCallback result_callback) {
  if (!file_requests_.is_bound()) {
    LOG(ERROR) << "SandboxFindNextFile called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  auto find_handle = mojom::FindHandle::New();
  find_handle->find_handle = handle;
  file_requests_->SandboxFindNextFile(std::move(find_handle),
                                      std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineFileRequestsProxy::SandboxFindClose(
    FindFileHandle handle,
    mojom::EngineFileRequests::SandboxFindCloseCallback result_callback) {
  if (!file_requests_.is_bound()) {
    LOG(ERROR) << "SandboxFindClose called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  auto find_handle = mojom::FindHandle::New();
  find_handle->find_handle = handle;
  file_requests_->SandboxFindClose(std::move(find_handle),
                                   std::move(result_callback));

  return MojoCallStatus::Success();
}

MojoCallStatus EngineFileRequestsProxy::SandboxOpenReadOnlyFile(
    const base::FilePath& path,
    uint32_t dwFlagsAndAttributes,
    mojom::EngineFileRequests::SandboxOpenReadOnlyFileCallback
        result_callback) {
  if (!file_requests_.is_bound()) {
    LOG(ERROR) << "SandboxOpenReadOnlyFile called without bound pointer";
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  file_requests_->SandboxOpenReadOnlyFile(path, dwFlagsAndAttributes,
                                          std::move(result_callback));

  return MojoCallStatus::Success();
}

}  // namespace chrome_cleaner
