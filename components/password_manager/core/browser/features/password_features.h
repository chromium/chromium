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
// the autofill dropdown.
BASE_DECLARE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN)
// OS authentication will use IUserConsentVerifierInterop api to trigger Windows
// Hello authentication. This api allows us to specify explicitly to which
// window, the OS prompt should attach.
BASE_DECLARE_FEATURE(kAuthenticateUsingUserConsentVerifierInteropApi);

// OS authentication will use UserConsentVerifier api to trigger Windows Hello
// authentication.
BASE_DECLARE_FEATURE(kAuthenticateUsingUserConsentVerifierApi);
#endif  // BUILDFLAG(IS_WIN)

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_DECLARE_FEATURE(kBiometricTouchToFill);

// Delete undecryptable passwords from the login database.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswords);

// Delete undecryptable passwords from the store when Sync is active.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswordsOnSync);

#if BUILDFLAG(IS_ANDROID)
// Enables reading credentials from SharedPreferences.
BASE_DECLARE_FEATURE(kFetchGaiaHashOnSignIn);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
BASE_DECLARE_FEATURE(kFillOnAccountSelect);

#if BUILDFLAG(IS_IOS)

// Enable saving username in UFF on iOS.
BASE_DECLARE_FEATURE(kIosDetectUsernameInUff);

// Enables password generation bottom sheet to be displayed (on iOS) when a user
// is signed-in and taps on a new password field.
BASE_DECLARE_FEATURE(kIOSProactivePasswordGenerationBottomSheet);

#endif

// Enables saving enterprise password hashes to a local state preference.
BASE_DECLARE_FEATURE(kLocalStateEnterprisePasswordHashes);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop

// Enables "chunking" generated passwords by adding hyphens every 4 characters
// to make them more readable.
BASE_DECLARE_FEATURE(kPasswordGenerationChunking);

// Enables updated password generation UI with a prominent button and previewing
// the generated password on focus.
BASE_DECLARE_FEATURE(kPasswordGenerationSoftNudge);

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

// Enables triggering password suggestions through the context menu.
BASE_DECLARE_FEATURE(kPasswordManualFallbackAvailable);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables "Needs access to keychain, restart chrome" bubble and banner.
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
// Enables promo card in settings encouraging users to enable screenlock reauth
// before filling passwords.
BASE_DECLARE_FEATURE(kScreenlockReauthPromoCard);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enables biometric authentication on for Password Autofill on ChromeOS.
BASE_DECLARE_FEATURE(kBiometricsAuthForPwdFill);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

// Starts passwords resync after undecryptable passwords were removed. This flag
// is enabled by default and should be treaded as a killswitch.
BASE_DECLARE_FEATURE(kTriggerPasswordResyncAfterDeletingUndecryptablePasswords);

#if BUILDFLAG(IS_ANDROID)

// Enables showing various warnings for password manager users not yet enrolled
// into the new experience of storing passwords in GMSCore.
BASE_DECLARE_FEATURE(
    kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning);

// Whether to ignore the timeouts in between password access loss warning
// prompts. Used for manual testing.
// This param will be removed when the feature fully launches.
inline constexpr base::FeatureParam<bool> kIgnoreAccessLossWarningTimeout = {
    &kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning,
    "ignore_access_loss_warning_timeout", false};

// If set to true, this will simulate a failed migration to UPM (only if the
// client hasn't migrated yet).
inline constexpr base::FeatureParam<bool> kSimulateFailedMigration = {
    &kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning,
    "simulate_failed_migration", false};

// The feature flag for the Identity Check feature. The feature makes biometric
// authentication mandatory before password filling in untrusted locations.
BASE_DECLARE_FEATURE(kBiometricAuthIdentityCheck);

// Enables clearing the login database for the users who already migrated their
// credentials to GMS Core.
BASE_DECLARE_FEATURE(kClearLoginDatabaseForAllMigratedUPMUsers);
#endif  // BUILDFLAG(IS_ANDROID)

// Improves PSL matching capabilities by utilizing PSL-extension list from
// affiliation service. It fixes problem with incorrect password suggestions on
// websites like slack.com.
BASE_DECLARE_FEATURE(kUseExtensionListForPSLMatching);

// Enables support of sending additional votes on username first flow. The votes
// are sent on single password forms and contain information about preceding
// single username forms.
// TODO(crbug.com/40626063): Clean up if the main crowdsourcing is good enough
// and we don't need additional signals.
BASE_DECLARE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing);

// Enables new prediction that is based on votes from Username First Flow with
// Intermediate Values.
BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValuesPredictions);

// Enables voting for more text fields outside of the password form in Username
// First Flow.
BASE_DECLARE_FEATURE(kUsernameFirstFlowWithIntermediateValuesVoting);

// Enables async implementation of OSCrypt inside LoginDatabase.
BASE_DECLARE_FEATURE(kUseAsyncOsCryptInLoginDatabase);

// Enables async implementation of OSCrypt inside LoginDatabase.
BASE_DECLARE_FEATURE(kUseNewEncryptionMethod);

// Enables re-encryption of all passwords. Done separately for each store.
BASE_DECLARE_FEATURE(kEncryptAllPasswordsWithOSCryptAsync);

// All features parameters in alphabetical order.
}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
