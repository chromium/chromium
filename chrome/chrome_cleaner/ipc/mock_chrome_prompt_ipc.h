// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_IPC_MOCK_CHROME_PROMPT_IPC_H_
#define CHROME_CHROME_CLEANER_IPC_MOCK_CHROME_PROMPT_IPC_H_

#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

class MockChromePromptIPC : public ChromePromptIPC {
 public:
  MockChromePromptIPC();
  ~MockChromePromptIPC() override;

  MOCK_METHOD1(Initialize, void(ErrorHandler* error_handler));

  // Workaround for GMock's limitation, in which MOCK_METHOD* doesn't
  // accept base::OnceCallback parameters. Will forward any calls to
  // MockPost*() and pass along a raw pointer for |callback|.
  void PostPromptUserTask(const std::vector<base::FilePath>& files_to_delete,
                          const std::vector<std::wstring>& registry_keys,
                          const std::vector<std::wstring>& extension_ids,
                          PromptUserCallback callback) override;

  MOCK_METHOD4(MockPostPromptUserTask,
               void(const std::vector<base::FilePath>& files_to_delete,
                    const std::vector<std::wstring>& registry_keys,
                    const std::vector<std::wstring>& extension_ids,
                    PromptUserCallback* callback));
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_IPC_MOCK_CHROME_PROMPT_IPC_H_
