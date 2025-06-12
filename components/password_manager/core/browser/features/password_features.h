// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_

// This file defines all password manager features used in the browser process.
// Prefer adding new features here instead of "core/common/".

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace password_manager::features {
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_ANDROID)
// Enables filling of OTPs received via SMS on Android.
BASE_DECLARE_FEATURE(kAndroidSmsOtpFilling);
#endif  // BUILDFLAG(IS_ANDROID)

// When enabled, updates to shared existing passwords from the same sender are
// auto-approved.
BASE_DECLARE_FEATURE(kAutoApproveSharedPasswordUpdatesFromSameSender);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Feature flag to control the displaying of an ongoing hats survey that
// measures users perception of autofilling password forms. Differently from
// other surveys, the Autofill user perception surveys will not have a specific
// target number of answers where it will be fully stop, instead, it will run
// indefinitely. A target number of full answers exists, but per quarter. The
// goal is to have a go to place to understand how users are perceiving autofill
// across quarters.
BASE_DECLARE_FEATURE(kAutofillPasswordUserPerceptionSurvey);
// Moves the "Use a passkey / Use a different passkey" to the context menu from
// the autofill dropdown. This is now decoupled from
// "PasswordManualFallbackAvailable" flag.
BASE_DECLARE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_DECLARE_FEATURE(kBiometricTouchToFill);

// Delete undecryptable passwords from the login database.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswords);

// Delete undecryptable passwords from the store when Sync is active.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswordsOnSync);

// Marks form submission as failed whenever a POST request has failed for the
// same iframe with 400-403 status code.
BASE_DECLARE_FEATURE(kFailedLoginDetectionBasedOnResourceLoadingErrors);

// Marks form submission as failed whenever a password field is cleared for the
// sign-in forms.
BASE_DECLARE_FEATURE(kFailedLoginDetectionBasedOnFormClearEvent);

#if BUILDFLAG(IS_ANDROID)
// Enables reading credentials from SharedPreferences.
BASE_DECLARE_FEATURE(kFetchGaiaHashOnSignIn);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
BASE_DECLARE_FEATURE(kFillOnAccountSelect);

#if BUILDFLAG(IS_ANDROID)
// Allows filling from a secondary recovery password saved as a backup.
BASE_DECLARE_FEATURE(kFillRecoveryPassword);
#endif

#if BUILDFLAG(IS_IOS)

// Enables the clean up of hanging form extraction requests made by the
// password suggestion helper. This is to fix the cases where the suggestions
// pipeline is broken because the pipeline is waiting for password suggestions
// that are never provided.
BASE_DECLARE_FEATURE(kIosCleanupHangingPasswordFormExtractionRequests);

// The feature parameter that determines the minimal period of time in
// milliseconds before the form extraction request times out.
extern const base::FeatureParam<int>
    kIosPasswordFormExtractionRequestsTimeoutMs;

// Enables the second version of the bottom sheet to fix a few bugs that we've
// seen in production since the launch of the V1 of the feature.
BASE_DECLARE_FEATURE(kIOSPasswordBottomSheetV2);

// Enables password generation bottom sheet to be displayed (on iOS) when a user
// is signed-in and taps on a new password field.
BASE_DECLARE_FEATURE(kIOSProactivePasswordGenerationBottomSheet);

#endif

// Enables running the clientside form classifier to parse password forms.
BASE_DECLARE_FEATURE(kPasswordFormClientsideClassifier);

// Enables offering credentials for filling across grouped domains.
BASE_DECLARE_FEATURE(kPasswordFormGroupedAffiliations);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables "chunking" generated passwords by adding hyphens every 4 characters
// to make them more readable.
BASE_DECLARE_FEATURE(kPasswordGenerationChunking);
// Enables triggering password suggestions through the context menu.
BASE_DECLARE_FEATURE(kPasswordManualFallbackAvailable);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

// Detects password reuse based on hashed password values.
BASE_DECLARE_FEATURE(kReuseDetectionBasedOnPasswordHashes);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables "Needs access to keychain, restart chrome" bubble and banner.
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
// Enables biometric authentication on for Password Autofill on ChromeOS.
BASE_DECLARE_FEATURE(kBiometricsAuthForPwdFill);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Sets request criticality when calling leak check service to detect leaked
// passwords.
BASE_DECLARE_FEATURE(kSetLeakCheckRequestCriticality);

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

// Starts passwords resync after undecryptable passwords were removed. This flag
// is enabled by default and should be treaded as a killswitch.
BASE_DECLARE_FEATURE(kTriggerPasswordResyncAfterDeletingUndecryptablePasswords);

// Starts passwords resync when undecryptable passwords are detected.
BASE_DECLARE_FEATURE(kTriggerPasswordResyncWhenUndecryptablePasswordsDetected);

#if BUILDFLAG(IS_ANDROID)
// The feature flag for the Identity Check feature. The feature makes biometric
// authentication mandatory before password filling in untrusted locations.
BASE_DECLARE_FEATURE(kBiometricAuthIdentityCheck);

// If enabled, the password store no longer uses the Login DB as a backend.
// Instead, it either uses the Android-specific storage or an empty backend
// if the client isn't eligible for the former.
BASE_DECLARE_FEATURE(kLoginDbDeprecationAndroid);

inline constexpr base::FeatureParam<int> kLoginDbDeprecationExportDelay = {
    &kLoginDbDeprecationAndroid,
    /*name=*/"login-db-deprecation-export-delay-seconds", /*default_value=*/5};
#endif  // BUILDFLAG(IS_ANDROID)

// Improves PSL matching capabilities by utilizing PSL-extension list from
// affiliation service. It fixes problem with incorrect password suggestions on
// websites like slack.com.
BASE_DECLARE_FEATURE(kUseExtensionListForPSLMatching);

// Enables new encryption method of OSCrypt inside LoginDatabase (Stage 2).
BASE_DECLARE_FEATURE(kUseNewEncryptionMethod);

// Enables re-encryption of all passwords. Done separately for each store
// (Stage 3).
BASE_DECLARE_FEATURE(kEncryptAllPasswordsWithOSCryptAsync);

// Marks all submitted credentials as leaked, useful for testing of a password
// leak dialog.
BASE_DECLARE_FEATURE(kMarkAllCredentialsAsLeaked);

// Enables improvements to password change functionality.
BASE_DECLARE_FEATURE(kImprovedPasswordChangeService);

// All features parameters in alphabetical order.
}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
