// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_TEST_UTIL_H_
#define CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

// Mock for the ChromePromptIPC controller in the cleaner process. This fake
// replaces the Chrome side of the connection in the unit tests, bypassing the
// proto IPC layer entirely.
class MockChromePromptIPC : public ChromePromptIPC {
 public:
  MockChromePromptIPC();
  ~MockChromePromptIPC() override;

  MOCK_METHOD(void, Initialize, (ErrorHandler * error_handler));

  // Workaround for GMock's limitation, in which MOCK_METHOD* doesn't
  // accept base::OnceCallback parameters. Will forward any calls to
  // MockPost*() and pass along a raw pointer for |callback|.
  void PostPromptUserTask(const std::vector<base::FilePath>& files_to_delete,
                          const std::vector<std::wstring>& registry_keys,
                          const std::vector<std::wstring>& extension_ids,
                          PromptUserCallback callback) override;

  MOCK_METHOD(void,
              MockPostPromptUserTask,
              (const std::vector<base::FilePath>& files_to_delete,
               const std::vector<std::wstring>& registry_keys,
               const std::vector<std::wstring>& extension_ids,
               PromptUserCallback* callback));
};

// A collection of handles used to communicate between the cleaner process and
// Chrome.
struct ChromePromptPipeHandles {
  ChromePromptPipeHandles();
  ~ChromePromptPipeHandles();

  // Move-only.
  ChromePromptPipeHandles(const ChromePromptPipeHandles& other) = delete;
  ChromePromptPipeHandles& operator=(const ChromePromptPipeHandles& other) =
      delete;
  ChromePromptPipeHandles(ChromePromptPipeHandles&& other);
  ChromePromptPipeHandles& operator=(ChromePromptPipeHandles&& other);

  bool IsValid() const {
    return request_read_handle.IsValid() && request_write_handle.IsValid() &&
           response_read_handle.IsValid() && response_write_handle.IsValid();
  }

  // Handles for Chrome.
  base::win::ScopedHandle request_read_handle;
  base::win::ScopedHandle response_write_handle;

  // Handles for the cleaner.
  base::win::ScopedHandle request_write_handle;
  base::win::ScopedHandle response_read_handle;
};

// A mock class that can send replies to ChromePrompt requests, faking the
// Chrome side of an IPC connection. This is suitable for high-level tests of
// cleaner and Chrome behaviour.
class MockChromePromptResponder {
 public:
  explicit MockChromePromptResponder(ChromePromptPipeHandles handles);
  ~MockChromePromptResponder();

  MockChromePromptResponder(const MockChromePromptResponder& other) = delete;
  MockChromePromptResponder& operator=(const MockChromePromptResponder& other) =
      delete;

  // Enters a loop reading ChromePrompt requests, calling the appropriate
  // mock method for each request. When the other end of the pipe closes or
  // there is an error this will signal |done_reading_event| and return. It
  // will also log a gtest failure for any error.
  //
  // This must be called after passing the client end of the handles to the
  // child side of the IPC connection, because it will close the parent's copy
  // of those handles.
  void ReadRequests(base::WaitableEvent* done_reading_event);

  // Exits the ReadRequests loop.
  void StopReading() { stop_reading_ = true; }

  void SendQueryCapabilityResponse();
  void SendPromptUserResponse(PromptUserResponse::PromptAcceptance acceptance);

  MOCK_METHOD(void, CloseConnectionRequest, ());
  MOCK_METHOD(void, QueryCapabilityRequest, ());
  MOCK_METHOD(void,
              PromptUserRequest,
              (const std::vector<std::string>& files_to_delete,
               const std::vector<std::string>& registry_keys));

 private:
  void WriteResponseMessage(const google::protobuf::MessageLite& message);

  ChromePromptPipeHandles handles_;
  bool stop_reading_ = false;
};

// The parent process (which is generally the test framework) must always be
// the server end of a named pipe. In some tests the test framework will mock
// the cleaner end of the connection, and in others it will mock the Chrome
// end.
enum class ChromePromptServerProcess {
  kCleanerIsServer,
  kChromeIsServer,
};

// Creates a pair of named pipes for IPC communication and returns the handles
// of all pipe ends. |server_process| specifies which handles will be the
// server side of the connection. Also adds the client side handles to
// |command_line| and |handles_to_inherit| if they are not null.
ChromePromptPipeHandles CreateTestChromePromptMessagePipes(
    ChromePromptServerProcess server_process,
    base::CommandLine* command_line = nullptr,
    base::HandlesToInheritVector* handles_to_inherit = nullptr);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_TEST_UTIL_H_
