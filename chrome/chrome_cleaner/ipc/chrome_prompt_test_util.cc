// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/chrome_prompt_test_util.h"

#include <windows.h>

#include <utility>

#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "base/win/win_util.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

using base::win::ScopedHandle;
using ::testing::_;
using ::testing::InvokeWithoutArgs;

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

MockChromePromptResponder::MockChromePromptResponder(
    ChromePromptPipeHandles handles)
    : handles_(std::move(handles)) {
  // Stop reading immediately on receiving an unhandled mock call, since the
  // other end of the pipe may be waiting on a response.
  ON_CALL(*this, CloseConnectionRequest())
      .WillByDefault(
          InvokeWithoutArgs(this, &MockChromePromptResponder::StopReading));
  ON_CALL(*this, QueryCapabilityRequest())
      .WillByDefault(
          InvokeWithoutArgs(this, &MockChromePromptResponder::StopReading));
  ON_CALL(*this, PromptUserRequest(_, _))
      .WillByDefault(
          InvokeWithoutArgs(this, &MockChromePromptResponder::StopReading));
}

MockChromePromptResponder::~MockChromePromptResponder() = default;

// This is basically ServiceChromePromptRequests from
// chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_channel_win.cc
// with assertions instead of error handling.
void MockChromePromptResponder::ReadRequests(
    base::WaitableEvent* done_reading_event) {
  DCHECK(handles_.IsValid());

  // Close our copy of the handles that the child reads and writes. They are
  // unused in this process.
  handles_.request_write_handle.Close();
  handles_.response_read_handle.Close();

  DCHECK(done_reading_event);
  base::ScopedClosureRunner done_reading(
      base::BindLambdaForTesting([this, done_reading_event]() {
        // Close the parent's handles when exiting the read loop. If the child
        // is still running the handles on the other end of the pipe will get
        // errors, causing it to exit. If the child exited normally it will
        // already have closed its handles so this will be harmless.
        handles_.request_read_handle.Close();
        handles_.response_write_handle.Close();
        done_reading_event->Signal();
      }));

  HANDLE read_handle = handles_.request_read_handle.Get();

  // Read the protocol version handshake.
  uint8_t version;
  DWORD bytes_read;
  ASSERT_TRUE(
      ::ReadFile(read_handle, &version, sizeof(version), &bytes_read, nullptr))
      << "errno " << ::GetLastError();
  ASSERT_EQ(version, 1);  // kVersion from proto_chrome_prompt_ipc.cc

  while (!stop_reading_) {
    // Read the length of the next message.
    uint32_t message_length;
    ASSERT_TRUE(::ReadFile(read_handle, &message_length, sizeof(message_length),
                           &bytes_read, nullptr))
        << "errno " << ::GetLastError();

    // Read the next message.
    std::string message;
    ASSERT_TRUE(::ReadFile(read_handle,
                           base::WriteInto(&message, message_length + 1),
                           message_length, &bytes_read, nullptr))
        << "errno " << ::GetLastError();

    // Parse the message into a proto and invoke a mocked function for each
    // message type.
    ChromePromptRequest request;
    ASSERT_TRUE(request.ParseFromString(message));
    switch (request.request_case()) {
      case ChromePromptRequest::kCloseConnection:
        CloseConnectionRequest();
        break;
      case ChromePromptRequest::kQueryCapability:
        QueryCapabilityRequest();
        break;
      case ChromePromptRequest::kPromptUser: {
        ASSERT_TRUE(request.prompt_user().extension_ids().empty())
            << "RemoveExtensions is deprecated and unsupported.";
        std::vector<std::string> files_to_delete(
            request.prompt_user().files_to_delete().begin(),
            request.prompt_user().files_to_delete().end());
        std::vector<std::string> registry_keys(
            request.prompt_user().registry_keys().begin(),
            request.prompt_user().registry_keys().end());
        PromptUserRequest(files_to_delete, registry_keys);
      } break;
      case ChromePromptRequest::kRemoveExtensions:
        FAIL() << "RemoveExtensions is deprecated and unsupported.";
      default:
        FAIL() << "Unhandled ChromePromptRequest " << request.request_case();
    }
  }
}

void MockChromePromptResponder::SendQueryCapabilityResponse() {
  chrome_cleaner::QueryCapabilityResponse response;
  WriteResponseMessage(response);
}

void MockChromePromptResponder::SendPromptUserResponse(
    PromptUserResponse::PromptAcceptance acceptance) {
  chrome_cleaner::PromptUserResponse response;
  response.set_prompt_acceptance(acceptance);
  WriteResponseMessage(response);
}

void MockChromePromptResponder::WriteResponseMessage(
    const google::protobuf::MessageLite& message) {
  std::string response_string;
  ASSERT_TRUE(message.SerializeToString(&response_string));

  HANDLE write_handle = handles_.response_write_handle.Get();
  uint32_t message_size = response_string.size();
  DWORD bytes_written;
  ASSERT_TRUE(::WriteFile(write_handle, &message_size, sizeof(uint32_t),
                          &bytes_written, nullptr));
  ASSERT_TRUE(::WriteFile(write_handle, response_string.data(), message_size,
                          &bytes_written, nullptr));
}

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
