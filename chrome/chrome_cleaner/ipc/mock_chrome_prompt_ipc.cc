// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/ipc/mock_chrome_prompt_ipc.h"

namespace chrome_cleaner {

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

}  // namespace chrome_cleaner
