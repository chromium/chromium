// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/engine_file_requests_impl.h"

#include <map>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/chrome_cleaner/engines/broker/scanner_sandbox_interface.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chrome_cleaner {

namespace {

mojom::FindFileDataPtr FindDataToMojoStruct(LPWIN32_FIND_DATAW data) {
  auto find_data_ptr = mojom::FindFileData::New();
  find_data_ptr->data.resize(sizeof(WIN32_FIND_DATAW));
  memcpy(find_data_ptr->data.data(), data, sizeof(WIN32_FIND_DATAW));

  return find_data_ptr;
}

}  // namespace

// TODO(joenotcharles): Log the parameters of all the calls on this file.
EngineFileRequestsImpl::EngineFileRequestsImpl(
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    InterfaceMetadataObserver* metadata_observer)
    : mojo_task_runner_(mojo_task_runner),
      metadata_observer_(metadata_observer) {}

void EngineFileRequestsImpl::Bind(
    mojo::PendingAssociatedRemote<mojom::EngineFileRequests>* remote) {
  receiver_.reset();

  receiver_.Bind(remote->InitWithNewEndpointAndPassReceiver());
  // There's no need to call set_disconnect_handler on this since it's an
  // associated interface. Any errors will be handled on the main EngineCommands
  // interface.
}

EngineFileRequestsImpl::~EngineFileRequestsImpl() = default;

void EngineFileRequestsImpl::SandboxFindFirstFile(
    const base::FilePath& file_name,
    SandboxFindFirstFileCallback result_callback) {
  // Execute the request off of the Mojo thread to unblock it for other calls.
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&EngineFileRequestsImpl::FindFirstFile,
                                base::Unretained(this), file_name,
                                std::move(result_callback)));
}

void EngineFileRequestsImpl::FindFirstFile(
    const base::FilePath& file_name,
    SandboxFindFirstFileCallback result_callback) {
  WIN32_FIND_DATAW find_file_data;
  HANDLE handle;
  uint32_t result = chrome_cleaner_sandbox::SandboxFindFirstFile(
      file_name, &find_file_data, &handle);
  if (metadata_observer_) {
    std::map<std::string, std::string> arguments = {
        {"file_name", base::UTF16ToUTF8(file_name.value())},
        {"result", base::NumberToString(result)},
        {"handle", base::NumberToString(reinterpret_cast<size_t>(handle))},
    };
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD, arguments);
  }

  auto find_data_ptr = FindDataToMojoStruct(&find_file_data);

  auto find_handle_ptr = mojom::FindHandle::New();
  find_handle_ptr->find_handle =
      reinterpret_cast<int64_t>(HandleToHandle64(handle));

  // Execute the result callback on the Mojo thread.
  mojo_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(result_callback), result,
                     std::move(find_data_ptr), std::move(find_handle_ptr)));
}

void EngineFileRequestsImpl::SandboxFindNextFile(
    mojom::FindHandlePtr handle_ptr,
    SandboxFindNextFileCallback result_callback) {
  base::PostTask(FROM_HERE, {base::ThreadPool(), base::MayBlock()},
                 base::BindOnce(&EngineFileRequestsImpl::FindNextFile,
                                base::Unretained(this), std::move(handle_ptr),
                                std::move(result_callback)));
}

void EngineFileRequestsImpl::FindNextFile(
    mojom::FindHandlePtr handle_ptr,
    SandboxFindNextFileCallback result_callback) {
  HANDLE handle =
      Handle64ToHandle(reinterpret_cast<void*>(handle_ptr->find_handle));
  WIN32_FIND_DATAW find_file_data;
  uint32_t result =
      chrome_cleaner_sandbox::SandboxFindNextFile(handle, &find_file_data);
  if (metadata_observer_) {
    std::map<std::string, std::string> arguments = {
        {"result", base::NumberToString(result)},
        {"handle", base::NumberToString(reinterpret_cast<size_t>(handle))},
    };
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD, arguments);
  }

  auto find_data_ptr = FindDataToMojoStruct(&find_file_data);

  mojo_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(std::move(result_callback), result,
                                             std::move(find_data_ptr)));
}

void EngineFileRequestsImpl::SandboxFindClose(
    mojom::FindHandlePtr handle_ptr,
    SandboxFindCloseCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineFileRequestsImpl::FindClose, base::Unretained(this),
                     std::move(handle_ptr), std::move(result_callback)));
}

void EngineFileRequestsImpl::FindClose(
    mojom::FindHandlePtr handle_ptr,
    SandboxFindCloseCallback result_callback) {
  HANDLE handle =
      Handle64ToHandle(reinterpret_cast<void*>(handle_ptr->find_handle));
  uint32_t result = chrome_cleaner_sandbox::SandboxFindClose(handle);
  if (metadata_observer_) {
    std::map<std::string, std::string> arguments = {
        {"result", base::NumberToString(result)},
        {"handle", base::NumberToString(reinterpret_cast<size_t>(handle))},
    };
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD, arguments);
  }

  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback), result));
}

void EngineFileRequestsImpl::SandboxOpenReadOnlyFile(
    const base::FilePath& file_name,
    uint32_t dwFlagsAndAttribute,
    SandboxOpenReadOnlyFileCallback result_callback) {
  base::PostTask(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&EngineFileRequestsImpl::OpenReadOnlyFile,
                     base::Unretained(this), file_name, dwFlagsAndAttribute,
                     std::move(result_callback)));
}

void EngineFileRequestsImpl::OpenReadOnlyFile(
    const base::FilePath& file_name,
    uint32_t dwFlagsAndAttribute,
    SandboxOpenReadOnlyFileCallback result_callback) {
  if (metadata_observer_)
    metadata_observer_->ObserveCall(CURRENT_FILE_AND_METHOD);
  base::win::ScopedHandle handle =
      chrome_cleaner_sandbox::SandboxOpenReadOnlyFile(file_name,
                                                      dwFlagsAndAttribute);

  mojo_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback),
                                mojo::WrapPlatformFile(handle.Take())));
}

}  // namespace chrome_cleaner
