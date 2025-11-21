// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_
#define CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_

#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

class Browser;
class BrowserWindowInterface;

class PrivacySandboxDialog {
 public:
  // Creates and shows a dialog for |browser| displaying the Privacy Sandbox
  // notice or consent to the user. Specific implementations are responsible for
  // altering the content as appropriate based on |prompt_type|.
  // TODO(crbug.com/408016824): To be deprecated once V2 is migrated to.
  static void Show(Browser* browser,
                   PrivacySandboxService::PromptType prompt_type);
};

#endif  // CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_H_
