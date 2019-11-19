// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/proto_chrome_prompt_ipc.h"

#include <windows.h>

#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"

namespace {

// chrome_cleaner <-> chrome protocol version.
constexpr uint8_t kVersion = 1;
}  // namespace

namespace chrome_cleaner {

ProtoChromePromptIPC::ProtoChromePromptIPC(
    base::win::ScopedHandle response_read_handle,
    base::win::ScopedHandle request_write_handle)
    : response_read_handle_(std::move(response_read_handle)),
      request_write_handle_(std::move(request_write_handle)) {
  // All uses of this class, and more specifically its state member need to
  // happen on the same sequence but one that is not the construction
  // sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProtoChromePromptIPC::~ProtoChromePromptIPC() = default;

void ProtoChromePromptIPC::Initialize(ErrorHandler* error_handler) {
  DCHECK(task_runner_);

  error_handler_ = error_handler;

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ProtoChromePromptIPC::InitializeImpl,
                                        base::Unretained(this)));
}

void ProtoChromePromptIPC::PostPromptUserTask(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<base::string16>& registry_keys,
    const std::vector<base::string16>& extension_ids,
    PromptUserCallback callback) {
  DCHECK(task_runner_);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ProtoChromePromptIPC::RunPromptUserTask,
                     base::Unretained(this), files_to_delete, registry_keys,
                     extension_ids, std::move(callback)));
}

void ProtoChromePromptIPC::PostDisableExtensionsTask(
    const std::vector<base::string16>& extension_ids,
    DisableExtensionsCallback callback) {
  NOTIMPLEMENTED();
  OnConnectionError();
}

void ProtoChromePromptIPC::TryDeleteExtensions(
    base::OnceClosure delete_allowed_callback,
    base::OnceClosure delete_not_allowed_callback) {
  NOTIMPLEMENTED();
  OnConnectionError();
}

void ProtoChromePromptIPC::InitializeImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kUninitialized, state_);
  state_ = State::kWaitingForScanResults;

  // Initialize communication with chrome by sending the version.
  WriteByValue(kVersion);
}

void ProtoChromePromptIPC::RunPromptUserTask(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<base::string16>& registry_keys,
    const std::vector<base::string16>& extension_ids,
    PromptUserCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kUninitialized);
  DCHECK_NE(state_, State::kWaitingForResponseFromChrome);

  // This can be true if any connection error occurred already in which case
  // We don't not want to go forward with the prompting.
  if (state_ == State::kDoneInteraction) {
    return;
  }

  state_ = State::kWaitingForResponseFromChrome;

  // If the contents of the message cannot be represented in a sane way avoid
  // sending it on the wire. Returns a denied prompt since Chrome would run
  // similar checks and deny it anyway.

  // Build the prompt message.
  chrome_cleaner::PromptUserRequest prompt_user_message;
  for (const base::FilePath& file_to_delete : files_to_delete) {
    std::string file_path_utf8;
    if (!base::UTF16ToUTF8(file_to_delete.value().c_str(),
                           file_to_delete.value().size(), &file_path_utf8)) {
      std::move(callback).Run(PromptUserResponse::DENIED);
      return;
    } else {
      prompt_user_message.add_files_to_delete(file_path_utf8);
    }
  }

  for (const base::string16& registry_key : registry_keys) {
    std::string registry_key_utf8;
    if (!base::UTF16ToUTF8(registry_key.c_str(), registry_key.size(),
                           &registry_key_utf8)) {
      std::move(callback).Run(PromptUserResponse::DENIED);
      return;
    } else {
      prompt_user_message.add_registry_keys(registry_key_utf8);
    }
  }

  for (const base::string16& extension_id : extension_ids) {
    std::string extension_id_utf8;
    if (!base::UTF16ToUTF8(extension_id.c_str(), extension_id.size(),
                           &extension_id_utf8)) {
      std::move(callback).Run(PromptUserResponse::DENIED);
      return;
    } else {
      prompt_user_message.add_extension_ids(extension_id_utf8);
    }
  }

  // This is the top-level message that Chrome is expecting.
  ChromePromptRequest chrome_prompt_request;
  *chrome_prompt_request.mutable_prompt_user() = prompt_user_message;

  std::string request_content;
  bool serialize_result =
      chrome_prompt_request.SerializeToString(&request_content);
  DCHECK(serialize_result);

  SendBuffer(request_content);

  // Sending the request can cause communication errors. If any happened don't
  // bother waiting for a response.
  if (state_ == State::kDoneInteraction) {
    return;
  }

  // Receive the response from Chrome.
  PromptUserResponse::PromptAcceptance prompt_acceptance =
      WaitForPromptAcceptance();

  if (state_ == State::kDoneInteraction) {
    return;
  }

  // Send a message confirming to Chrome that the communication is over.
  chrome_cleaner::CloseConnectionRequest close_connection_request;
  chrome_prompt_request = ChromePromptRequest();
  *chrome_prompt_request.mutable_close_connection() = close_connection_request;

  std::string response_content;
  serialize_result = chrome_prompt_request.SerializeToString(&response_content);
  DCHECK(serialize_result);

  SendBuffer(response_content);

  if (state_ == State::kDoneInteraction) {
    return;
  }

  // Invoke callback with the result.
  std::move(callback).Run(prompt_acceptance);

  state_ = State::kDoneInteraction;
}

void ProtoChromePromptIPC::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(State::kUninitialized, state_);
  DCHECK_NE(State::kDoneInteraction, state_);

  if (error_handler_) {
    error_handler_->OnConnectionClosed();
  }

  state_ = State::kDoneInteraction;
}

void ProtoChromePromptIPC::SendBuffer(const std::string& request_content) {
  DCHECK_NE(State::kDoneInteraction, state_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Write the message size.
  const uint32_t kMessageLength = request_content.size();
  WriteByValue(kMessageLength);

  // Writing the message length failed. Do not send body.
  if (state_ == State::kDoneInteraction) {
    return;
  }

  // Write the message content.
  WriteByPointer(request_content.data(), kMessageLength);
}

PromptUserResponse::PromptAcceptance
ProtoChromePromptIPC::WaitForPromptAcceptance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kWaitingForResponseFromChrome, state_);

  // On any error condition, invoke the error handler.
  base::ScopedClosureRunner call_connection_closed(base::BindOnce(
      &ProtoChromePromptIPC::OnConnectionError, base::Unretained(this)));

  // Read the response length.
  DWORD bytes_read = 0;
  uint32_t response_length = 0;
  if (!::ReadFile(response_read_handle_.Get(), &response_length,
                  sizeof(response_length), &bytes_read, nullptr)) {
    PLOG(ERROR) << "Reading the prompt acceptance message length failed.";
    return PromptUserResponse::DENIED;
  }
  if (bytes_read != sizeof(response_length)) {
    PLOG(ERROR) << "Short read on the prompt acceptance message length.";
    return PromptUserResponse::DENIED;
  }

  if (response_length == 0 || response_length > kMaxMessageLength) {
    PLOG(ERROR) << "Invalid message length received: " << response_length;
    return PromptUserResponse::DENIED;
  }

  // Read the response.
  std::string response_content;
  if (!::ReadFile(response_read_handle_.Get(),
                  base::WriteInto(&response_content, response_length + 1),
                  response_length, &bytes_read, nullptr)) {
    PLOG(ERROR) << "Reading the prompt acceptance message failed";
    return PromptUserResponse::DENIED;
  }
  if (bytes_read != response_length) {
    PLOG(ERROR) << "Short read on the prompt acceptance message.";
    return PromptUserResponse::DENIED;
  }

  chrome_cleaner::PromptUserResponse response;
  if (!response.ParseFromString(response_content)) {
    LOG(ERROR) << "Parsing of prompt acceptance failed.";
    return PromptUserResponse::DENIED;
  }

  // Successful execution.
  call_connection_closed.ReplaceClosure(base::DoNothing());

  return response.prompt_acceptance();
}

}  // namespace chrome_cleaner
