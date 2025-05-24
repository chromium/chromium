// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_CHROME_SIGNOUT_CONFIRMATION_PROMPT_H_
#define CHROME_BROWSER_UI_SIGNIN_CHROME_SIGNOUT_CONFIRMATION_PROMPT_H_

#include "base/functional/callback_forward.h"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ChromeSignoutConfirmationChoice)
enum class ChromeSignoutConfirmationChoice {
  kCancelSignout = 0,
  kSignout = 1,
  kCancelSignoutAndReauth = 2,

  kMaxValue = kCancelSignoutAndReauth,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:ChromeSignoutConfirmationChoice)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AccountExtensionsSignoutChoice)
enum class AccountExtensionsSignoutChoice {
  kCancelSignout = 0,
  kSignoutAccountExtensionsKept = 1,
  kSignoutAccountExtensionsUninstalled = 2,

  kMaxValue = kSignoutAccountExtensionsUninstalled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:AccountExtensionsSignoutChoice)

// Represents a callback made when a user accepts or cancels the signout
// confirmation prompt.
// Contains a `ChromeSignoutConfirmationChoice` and a boolean indicating if
// account extensions should be uninstalled on signout.
using SignoutConfirmationCallback =
    base::OnceCallback<void(ChromeSignoutConfirmationChoice, bool)>;

enum class ChromeSignoutConfirmationPromptVariant {
  // The user does not have unsynced data.
  // Available choices: `kSignout` and `kDismissed`.
  kNoUnsyncedData,
  // The user has unsynced data, and can choose between canceling the signout
  // or proceeding anyway.
  // Available choices: `kSignout` and `kDismissed`.
  kUnsyncedData,
  // The user has unsynced data, and can choose between reauthenticating or
  // proceeding anyway. Dismissing the dialog closes it without any action.
  // Available choices: `kReauth`, `kSignout` and `kDismissed`.
  kUnsyncedDataWithReauthButton,
  // The user is supervised and parental controls apply to their profile.
  // Available choices: `kSignout` and `kDismissed`.
  kProfileWithParentalControls,
};

void RecordChromeSignoutConfirmationPromptMetrics(
    ChromeSignoutConfirmationPromptVariant variant,
    ChromeSignoutConfirmationChoice choice);

void RecordAccountExtensionsSignoutChoice(
    ChromeSignoutConfirmationChoice choice,
    bool account_extensions_kept);

#endif  // CHROME_BROWSER_UI_SIGNIN_CHROME_SIGNOUT_CONFIRMATION_PROMPT_H_
