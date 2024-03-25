// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_CHROME_SIGNOUT_CONFIRMATION_PROMPT_H_
#define CHROME_BROWSER_UI_SIGNIN_CHROME_SIGNOUT_CONFIRMATION_PROMPT_H_

#include "base/functional/callback_forward.h"

class Browser;

enum class ChromeSignoutConfirmationChoice {
  kDismissed,
  kSignout,
  kReauth,
};

enum class ChromeSignoutConfirmationPromptVariant {
  // The user has unsynced data, and can choose between canceling the signout
  // or proceeding anyway.
  // Available choices: `kSignout` and `kDismissed`.
  kUnsyncedData,
  // The user has unsynced data, and can choose between canceling the signout
  // or proceeding anyway.
  // Available choices: `kReauth`, `kSignout` and `kDismissed`.
  kUnsyncedDataWithReauthButton,
};

// Factory function to create and show the Chrome signout confirmation prompt.
void ShowChromeSignoutConfirmationPrompt(
    Browser& browser,
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback);

#endif  // CHROME_BROWSER_UI_SIGNIN_CHROME_SIGNOUT_CONFIRMATION_PROMPT_H_
