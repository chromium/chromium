// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_TEST_UTIL_H_
#define CHROME_CHROME_CLEANER_IPC_CHROME_PROMPT_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
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
