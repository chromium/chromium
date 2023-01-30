// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_SETTINGS_METRICS_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_SETTINGS_METRICS_H_

namespace safe_browsing {

// Enum representing possible entry point to access the Safe Browsing settings
// page. They are used to construct the suffix of
// "SafeBrowsing.Settings.UserAction" histogram.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.safe_browsing.metrics)
enum class SettingsAccessPoint : int {
  kDefault = 0,
  // From Settings > Privacy and security.
  kParentSettings = 1,
  // From Settings > Safety check.
  kSafetyCheck = 2,
  // From PromoSlinger on Surface Explorer on Android.
  kSurfaceExplorerPromoSlinger = 3,
  // From security interstitial.
  kSecurityInterstitial = 4,
  // From UX shown due to the Tailored Security setting changing.
  kTailoredSecurity = 5,
  kMaxValue = kTailoredSecurity
};

// Enum representing actions taken by users visiting the
// Safe Browsing settings page. They are used for logging histograms, entries
// must not be removed or reordered.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.chrome.browser.safe_browsing.metrics)
enum class UserAction : int {
  // The page is shown to the user.
  kShowed = 0,
  // The user clicks the enhanced protection button. It will opt the user in
  // enhanced protection mode. Only logged if the user is from another Safe
  // Browsing state.
  kEnhancedProtectionClicked = 1,
  // The user clicks the standard protection button. It will opt the user in
  // standard protection mode. Only logged if the user is from another Safe
  // Browsing state.
  kStandardProtectionClicked = 2,
  // The user clicks the disable Safe Browsing button. A disable Safe Browsing
  // dialog will be shown. Only logged if the user is from another Safe Browsing
  // state.
  kDisableSafeBrowsingClicked = 3,
  // The user clicks the expand arrow of enhanced protection. It will show the
  // user more information for enhanced protection.
  kEnhancedProtectionExpandArrowClicked = 4,
  // The user clicks the expand arrow of standard protection. It will show the
  // user more information for standard protection.
  kStandardProtectionExpandArrowClicked = 5,
  // The user clicks the confirmed button on the disable Safe Browsing dialog.
  // It will opt the user in no protection mode.
  kDisableSafeBrowsingDialogConfirmed = 6,
  // The user clicks the denied button on the disable Safe Browsing dialog. The
  // user will stay in the original Safe Browsing mode.
  kDisableSafeBrowsingDialogDenied = 7,
  kMaxValue = kDisableSafeBrowsingDialogDenied
};

// Records the user action when the user navigates to the Enhanced Protection
// page.
void LogShowEnhancedProtectionAction();

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_SETTINGS_METRICS_H_
