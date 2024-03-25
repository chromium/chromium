// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome_signout_confirmation_prompt.h"
#include "content/public/test/browser_test.h"

class ChromeSignoutConfirmationPromptPixelTest : public DialogBrowserTest {
 public:
  void ShowUi(const std::string& name) override {
    ShowChromeSignoutConfirmationPrompt(
        *browser(),
        ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton,
        base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(ChromeSignoutConfirmationPromptPixelTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}
