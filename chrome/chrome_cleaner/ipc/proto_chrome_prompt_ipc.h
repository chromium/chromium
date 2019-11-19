// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_PROTO_CHROME_PROMPT_IPC_H_
#define CHROME_CHROME_CLEANER_IPC_PROTO_CHROME_PROMPT_IPC_H_

#include <windows.h>

#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"

namespace chrome_cleaner {

class ProtoChromePromptIPC : public ChromePromptIPC {
 public:
  static constexpr uint32_t kMaxMessageLength = 1 * 1024 * 1024;  // 1M bytes

  ProtoChromePromptIPC(base::win::ScopedHandle response_read_handle,
                       base::win::ScopedHandle request_write_handle);
  ~ProtoChromePromptIPC() override;

  void Initialize(ErrorHandler* error_handler) override;

  void PostPromptUserTask(const std::vector<base::FilePath>& files_to_delete,
                          const std::vector<base::string16>& registry_keys,
                          const std::vector<base::string16>& extension_ids,
                          PromptUserCallback callback) override;

  void PostDisableExtensionsTask(
      const std::vector<base::string16>& extension_ids,
      DisableExtensionsCallback callback) override;

  void TryDeleteExtensions(
      base::OnceClosure delete_allowed_callback,
      base::OnceClosure delete_not_allowed_callback) override;

 private:
  // Implements the initialization that needs to happen on the task_runner
  // sequence.
  void InitializeImpl();

  void RunPromptUserTask(const std::vector<base::FilePath>& files_to_delete,
                         const std::vector<base::string16>& registry_keys,
                         const std::vector<base::string16>& extension_ids,
                         PromptUserCallback callback);

  // Invokes error_handler_->OnConnectionClosed() and updates state_. This
  // should not be called more than once.
  void OnConnectionError();

  void SendBuffer(const std::string& request_content);

  PromptUserResponse::PromptAcceptance WaitForPromptAcceptance();

  template <typename T>
  void WriteByValue(T value) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DWORD bytes_written = 0;
    if (!::WriteFile(request_write_handle_.Get(), &value, sizeof(value),
                     &bytes_written, nullptr)) {
      PLOG(ERROR) << "Writing a message to the pipe failed.";
      OnConnectionError();
      return;
    }

    if (bytes_written != sizeof(value)) {
      LOG(ERROR) << "Incorrect number of bytes written to the pipe. Should be: "
                 << sizeof(value) << " but is :" << bytes_written;
      OnConnectionError();
    }
  }

  template <typename T>
  void WriteByPointer(const T* ptr, uint32_t size) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DWORD bytes_written = 0;
    if (!::WriteFile(request_write_handle_.Get(), ptr, size, &bytes_written,
                     nullptr)) {
      PLOG(ERROR) << "Writing a message to the pipe failed.";
      OnConnectionError();
      return;
    }

    if (bytes_written != size) {
      LOG(ERROR) << "Incorrect number of bytes written to the pipe. Should be: "
                 << size << " but is :" << bytes_written;
      OnConnectionError();
    }
  }

  base::win::ScopedHandle response_read_handle_;
  base::win::ScopedHandle request_write_handle_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()});
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_PROTO_CHROME_PROMPT_IPC_H_
