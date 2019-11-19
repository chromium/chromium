// Copyright 2018 The Chromium Authors. All rights reserved.
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
  // Only used on desktop.
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
  // Only used on Android.
  kPasswordsAccessorySheet = 6,
  // Corresponds to the touch to fill bottom sheet that replaces the dropdown.
  // Only used on Android.
  kTouchToFill = 7,
  kMaxValue = kTouchToFill,
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MANAGE_PASSWORDS_REFERRER_H_
