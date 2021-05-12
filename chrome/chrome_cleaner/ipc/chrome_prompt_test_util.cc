// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/chrome_prompt_test_util.h"

#include <windows.h>

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/win/win_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace chrome_cleaner {

using base::win::ScopedHandle;

namespace {

enum class ServerPipeDirection {
  kInbound,
  kOutbound,
};

// This function is copied from
// https://cs.chromium.org/chromium/src/chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_channel_win.cc
// to get the same behavior in tests as in Chrome.
std::pair<ScopedHandle, ScopedHandle> CreateMessagePipe(
    ServerPipeDirection server_direction) {
  SECURITY_ATTRIBUTES security_attributes = {};
  security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  // Use this process's default access token.
  security_attributes.lpSecurityDescriptor = nullptr;
  // Handles to inherit will be added to the LaunchOptions explicitly.
  security_attributes.bInheritHandle = false;

  std::wstring pipe_name = base::UTF8ToWide(
      base::StrCat({"\\\\.\\pipe\\chrome-cleaner-",
                    base::UnguessableToken::Create().ToString()}));

  // Create the server end of the pipe.
  DWORD direction_flag = server_direction == ServerPipeDirection::kInbound
                             ? PIPE_ACCESS_INBOUND
                             : PIPE_ACCESS_OUTBOUND;
  ScopedHandle server_handle(::CreateNamedPipe(
      pipe_name.c_str(), direction_flag | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
          PIPE_REJECT_REMOTE_CLIENTS,
      /*nMaxInstances=*/1, /*nOutBufferSize=*/0, /*nInBufferSize=*/0,
      /*nDefaultTimeOut=*/0, &security_attributes));
  if (!server_handle.IsValid()) {
    PLOG(ERROR) << "Error creating server pipe";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  // The client pipe's read/write permissions are the opposite of the server's.
  DWORD client_mode = server_direction == ServerPipeDirection::kInbound
                          ? GENERIC_WRITE
                          : GENERIC_READ;

  // Create the client end of the pipe.
  ScopedHandle client_handle(::CreateFile(
      pipe_name.c_str(), client_mode, /*dwShareMode=*/0,
      /*lpSecurityAttributes=*/nullptr, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL | SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
      /*hTemplateFile=*/nullptr));
  if (!client_handle.IsValid()) {
    PLOG(ERROR) << "Error creating client pipe";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  // Wait for the client end to connect (this should return
  // ERROR_PIPE_CONNECTED immediately since it's already connected).
  if (::ConnectNamedPipe(server_handle.Get(), /*lpOverlapped=*/nullptr)) {
    LOG(ERROR) << "ConnectNamedPipe got an unexpected connection";
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }
  const auto error = ::GetLastError();
  if (error != ERROR_PIPE_CONNECTED) {
    LOG(ERROR) << "ConnectNamedPipe returned unexpected error: "
               << logging::SystemErrorCodeToString(error);
    return std::make_pair(ScopedHandle(), ScopedHandle());
  }

  return std::make_pair(std::move(server_handle), std::move(client_handle));
}

void AppendHandleToCommandLine(base::CommandLine* command_line,
                               const std::string& switch_string,
                               HANDLE handle) {
  DCHECK(command_line);
  command_line->AppendSwitchASCII(
      switch_string, base::NumberToString(base::win::HandleToUint32(handle)));
}

}  // namespace

MockChromePromptIPC::MockChromePromptIPC() = default;

MockChromePromptIPC::~MockChromePromptIPC() = default;

void MockChromePromptIPC::PostPromptUserTask(
    const std::vector<base::FilePath>& files_to_delete,
    const std::vector<std::wstring>& registry_keys,
    const std::vector<std::wstring>& extension_ids,
    PromptUserCallback callback) {
  MockPostPromptUserTask(files_to_delete, registry_keys, extension_ids,
                         &callback);
}

ChromePromptPipeHandles::ChromePromptPipeHandles() = default;

ChromePromptPipeHandles::~ChromePromptPipeHandles() = default;

ChromePromptPipeHandles::ChromePromptPipeHandles(
    ChromePromptPipeHandles&& other) = default;

ChromePromptPipeHandles& ChromePromptPipeHandles::operator=(
    ChromePromptPipeHandles&& other) = default;

ChromePromptPipeHandles CreateTestChromePromptMessagePipes(
    ChromePromptServerProcess server_process,
    base::CommandLine* command_line,
    base::HandlesToInheritVector* handles_to_inherit) {
  ChromePromptPipeHandles handles;

  // Client handles are passed to the child process.
  HANDLE client_read_handle;
  HANDLE client_write_handle;

  if (server_process == ChromePromptServerProcess::kCleanerIsServer) {
    // Requests flow from the cleaner to Chrome, which is the client.
    std::tie(handles.request_write_handle, handles.request_read_handle) =
        CreateMessagePipe(ServerPipeDirection::kOutbound);
    client_read_handle = handles.request_read_handle.Get();

    // Responses flow from Chrome to the cleaner, which is the server.
    std::tie(handles.response_read_handle, handles.response_write_handle) =
        CreateMessagePipe(ServerPipeDirection::kInbound);
    client_write_handle = handles.response_write_handle.Get();
  } else {
    // Requests flow from the cleaner to Chrome, which is the server.
    std::tie(handles.request_read_handle, handles.request_write_handle) =
        CreateMessagePipe(ServerPipeDirection::kInbound);
    client_write_handle = handles.request_write_handle.Get();

    // Responses flow from Chrome to the cleaner, which is the client.
    std::tie(handles.response_write_handle, handles.response_read_handle) =
        CreateMessagePipe(ServerPipeDirection::kOutbound);
    client_read_handle = handles.response_read_handle.Get();
  }

  if (handles.IsValid()) {
    if (command_line) {
      AppendHandleToCommandLine(command_line,
                                chrome_cleaner::kChromeWriteHandleSwitch,
                                client_write_handle);
      AppendHandleToCommandLine(command_line,
                                chrome_cleaner::kChromeReadHandleSwitch,
                                client_read_handle);
    }
    if (handles_to_inherit) {
      handles_to_inherit->push_back(client_write_handle);
      handles_to_inherit->push_back(client_read_handle);
    }
  }
  return handles;
}

}  // namespace chrome_cleaner
