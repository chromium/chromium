// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_METRICS_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_METRICS_H_

#include "base/component_export.h"
#include "components/account_manager_core/account_upsertion_result.h"

namespace account_manager {

inline constexpr char kAccountAdditionSourceHistogramName[] =
    "AccountManager.AccountAdditionSource";
inline constexpr char kAccountUpsertionResultStatusHistogramName[] =
    "AccountManager.AccountUpsertionResultStatus";

// The source UI surface used for launching the account addition /
// re-authentication dialog. This should be as specific as possible.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Note: Please update |AccountManagerAccountAdditionSource| in enums.xml
// after adding new values.
enum class AccountAdditionSource : int {
  // OS Settings > Add account button.
  kSettingsAddAccountButton = 0,
  // OS Settings > Sign in again button.
  kSettingsReauthAccountButton = 1,
  // Launched from an ARC application.
  kArc = 2,
  // Launched automatically from Chrome content area. As of now, this is
  // possible only when an account requires re-authentication.
  kContentAreaReauth = 3,
  // Print Preview dialog.
  kPrintPreviewDialogUnused = 4,
  // Account Manager migration welcome screen.
  kAccountManagerMigrationWelcomeScreen = 5,
  // Onboarding.
  kOnboarding = 6,
  // At profile creation, main account of secondary profile.
  kChromeProfileCreation = 7,
  // Account addition flow launched by the user from One Google Bar.
  kOgbAddAccount = 8,
  // Avatar bubble -> Sign in again button.
  kAvatarBubbleReauthAccountButton = 9,
  // A Chrome extension required account re-authentication.
  kChromeExtensionReauth = 10,
  // Sync promo with an account that requires re-authentication.
  kChromeSyncPromoReauth = 11,
  // Chrome Settings > Sign in again button.
  kChromeSettingsReauthAccountButton = 12,
  // Avatar bubble -> Turn on sync button.
  kAvatarBubbleTurnOnSyncAddAccount = 13,
  // A Chrome extension required a new account.
  kChromeExtensionAddAccount = 14,
  // Sync promo with a new account.
  kChromeSyncPromoAddAccount = 15,
  // Chrome Settings > Turn on Sync.
  kChromeSettingsTurnOnSyncButton = 16,
  // Launched from ChromeOS Projector App for re-authentication.
  kChromeOSProjectorAppReauth = 17,
  // Chrome Menu -> Turn on Sync.
  kChromeMenuTurnOnSync = 18,
  // Sign-in promo with a new account.
  kChromeSigninPromoAddAccount = 19,
  // Gemini in Chrome re-authentication.
  kGeminiInChromeReauth = 20,

  kMaxValue = kGeminiInChromeReauth
};

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
void RecordAccountAdditionSource(AccountAdditionSource source);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
void RecordAccountUpsertionResultStatus(AccountUpsertionResult::Status status);

}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_METRICS_H_
