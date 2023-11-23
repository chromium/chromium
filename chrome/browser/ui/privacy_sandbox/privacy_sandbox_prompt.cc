// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/privacy_sandbox/privacy_sandbox_prompt.h"

#include "components/privacy_sandbox/privacy_sandbox_features.h"

void ShowPrivacySandboxPrompt(Browser* browser,
                              PrivacySandboxService::PromptType prompt_type) {
    ShowPrivacySandboxDialog(browser, prompt_type);
}
