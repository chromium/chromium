// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_H_
#define CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"

class Browser;

// Creates and shows a dialog for |browser| displaying the Privacy Sandbox
// notice or consent to the user. Specific implementations are responsible for
// altering the content as appropriate based on |dialog_type|.
void ShowPrivacySandboxDialog(Browser* browser,
                              PrivacySandboxService::DialogType dialog_type);

#endif  // CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_H_
