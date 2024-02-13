// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_
#define CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

class Browser;

// Creates and shows a dialog or bubble for |browser| displaying the Privacy
// Sandbox notice or consent to the user.
void ShowPrivacySandboxPrompt(Browser* browser,
                              PrivacySandboxService::PromptType prompt_type);

// Creates and shows a dialog for |browser| displaying the Privacy Sandbox
// notice or consent to the user. Specific implementations are responsible for
// altering the content as appropriate based on |prompt_type|.
void ShowPrivacySandboxDialog(Browser* browser,
                              PrivacySandboxService::PromptType prompt_type);

// Returns whether a Privacy Sandbox prompt can be shown in the |browser|.
// Checks if the maximum dialog height fits the prompt height.
bool CanWindowHeightFitPrivacySandboxPrompt(Browser* browser);

#endif  // CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_
