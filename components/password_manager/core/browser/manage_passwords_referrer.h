// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_

namespace password_manager {

// Enumerates referrers that can trigger a navigation to the manage passwords
// page.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Needs to stay in sync with
// ManagePasswordsReferrer in enums.xml.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.password_manager
enum class ManagePasswordsReferrer {
  // Corresponds to Chrome's settings page.
  kChromeSettings = 0,
  // Corresponds to the manage passwords bubble when clicking the key icon.
  kManagePasswordsBubble = 1,
  // Corresponds to the context menu following a right click into a password
  // field.
  // Only used on desktop.
  kPasswordContextMenu = 2,
  // Corresponds to the password dropdown shown when clicking into a password
  // field.
  kPasswordDropdown = 3,
  // Corresponds to the bubble shown when clicking the key icon after a password
  // was generated.
  kPasswordGenerationConfirmation = 4,
  // Corresponds to the profile chooser next to the omnibar ("Autofill Home").
  // Only used on desktop.
  kProfileChooser = 5,
  // Corresponds to the passwords accessory sheet on Android, triggered by
  // tapping on the key icon above in the keyboard accessory bar.
  kPasswordsAccessorySheet = 6,
  // Corresponds to the touch to fill bottom sheet that replaces the dropdown.
  // Only used on Android.
  kTouchToFill = 7,
  // The bubble notifying the user that the last compromised password was
  // updated.
  kSafeStateBubble = 8,
  // The dialog notifying a user about compromised credentials on sign in. Only
  // used on iOS.
  kPasswordBreachDialog = 9,
  // On Android, the Safety Check UI in settings opens the passwords page if no
  // check was performed.
  kSafetyCheck = 10,
  // On Desktop, the Google Password Manager link was clicked in the footer of
  // Save/Update bubble.
  kSaveUpdateBubble = 11,
  // On Desktop, the Google Password Manager link was clicked in the password
  // generation prompt in the Autofill dropdown.
  kPasswordGenerationPrompt = 12,
  // Corresponds to the situation when Chrome opens native Password Manager UI
  // when navigating to specified website.
  kPasswordsGoogleWebsite = 13,

  // Deprecated as part of APC removal.
  // kAutomatedPasswordChangeSuccessLink = 14,

  // On Mac, Win and ChromeOS after enabling Biometric authentication before
  // filling a confirmation dialog is shown with an instructions on how to
  // control the feature from settings.
  kBiometricAuthenticationBeforeFillingDialog = 15,

  // The Password Manager item was clicked in the Chrome menu.
  kChromeMenuItem = 16,

  // On Desktop, the bubble that notifies the user that some of the password
  // stored for the current site have been received via the password sharing
  // feature from other users.
  kSharedPasswordsNotificationBubble = 17,

  // On iOS, the Search Passwords homescreen widget that opens the Password
  // manager in search mode.
  kSearchPasswordsWidget = 18,

  // On Desktop, the Google Password Manager link was clicked in the footer of
  // AddUsername bubble.
  kAddUsernameBubble = 19,

  // On iOS, the "Manage Passwords" omnibox pedal suggestion was tapped.
  kOmniboxPedalSuggestion = 20,

  // On Desktop, link clicked in the DefaultStoreChanged bubble.
  kDefaultStoreChangedBubble = 21,

  // Corresponds to the manage password details bubble when clicking on the key
  // icon and navigating to the details view of a particular password.
  kManagePasswordDetailsBubble = 22,

  // On Desktop, the bubble that notifies the user that a passkey was saved.
  kPasskeySavedConfirmationBubble = 23,

  // On Desktop, the bubble that notifies the user that a passkey was deleted.
  kPasskeyDeletedConfirmationBubble = 24,

  // On Desktop, the bubble that notifies the user that a passkey was updated.
  kPasskeyUpdatedConfirmationBubble = 25,

  // On the desktop, the bubble notifies the user that a passkey was deleted
  // because it was not accepted.
  kPasskeyNotAcceptedBubble = 26,

  // The warning (Android only) informs the user that they may loose access to
  // their passwords because the transition to UPM has not happened.
  kAccessLossWarning = 27,

  // NOTE: When adding a new value to this enum that applies or could apply to
  // Android, make sure it is correctly handled by the internal credential
  // manager launcher java implementation.
  kMaxValue = kAccessLossWarning,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_
