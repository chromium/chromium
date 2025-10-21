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

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kActorLogin);
BASE_DECLARE_FEATURE(kActorLoginFillingHeuristics);
BASE_DECLARE_FEATURE(kActorLoginLocalClassificationModel);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
// Enables filling of OTPs received via SMS on Android.
BASE_DECLARE_FEATURE(kAndroidSmsOtpFilling);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables using clientside form classifier predictions for password forms.
BASE_DECLARE_FEATURE(kApplyClientsideModelPredictionsForPasswordTypes);

// Enables using clientside form classifier predictions for OTP forms.
BASE_DECLARE_FEATURE(kApplyClientsideModelPredictionsForOtps);

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

// Undoes the effect of WebAuthnUsePasskeyFromAnotherDeviceInContextMenu by
// adding the hybrid item back into the dropdown. It also adds the entry point
// to autofill dropdowns.
// Needs autofill::features::AutofillAndPasswordsInSameSurface to be enabled.
BASE_DECLARE_FEATURE(kAutofillReintroduceHybridPasskeyDropdownItem);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_DECLARE_FEATURE(kBiometricTouchToFill);

// Checks if submitted form is identical to an observed form before evaluating
// login success/failure.
BASE_DECLARE_FEATURE(kCheckIfSubmittedFormIdenticalToObserved);

// Checks if the new password field is visible in the viewport before returning
// the form in the ChangePasswordFormWaiter.
BASE_DECLARE_FEATURE(kCheckVisibilityInChangePasswordFormWaiter);

// Identifies if the user is fully signed in in the main tab
// before starting the Automated Password Change flow.
BASE_DECLARE_FEATURE(kCheckLoginStateBeforePasswordChange);

// Delete undecryptable passwords from the login database.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswords);

// Delete undecryptable passwords from the store when Sync is active.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswordsOnSync);

// Enables debug data popups on OTP fields for manual testing of
// one-time-passwords. Only for OTP detection testing, not intended to be
// launched.
BASE_DECLARE_FEATURE(kDebugUiForOtps);

// Updates password change flow to await for local ML model availability. The
// model has a superior performance compared to existing password manager
// classifications.
BASE_DECLARE_FEATURE(kDownloadModelForPasswordChange);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables Actor Login permissions UI in Password Manager settings
BASE_DECLARE_FEATURE(kEnableActorLoginPermissions);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables the Mojo JavaScript API for the password manager, replacing the
// legacy passwordsPrivate extension API.
BASE_DECLARE_FEATURE(kEnablePasswordManagerMojoApi);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Fetches change password url if the credential has been identified as leaked.
// Later change password url is used during password change.
BASE_DECLARE_FEATURE(kFetchChangePasswordUrlForPasswordChange);

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

// Enables password generation bottom sheet to be displayed (on iOS) when a user
// is signed-in and taps on a new password field.
BASE_DECLARE_FEATURE(kIOSProactivePasswordGenerationBottomSheet);

// Allows filling from a secondary recovery password saved as a backup on iOS.
// Acts as an iOS kill switch for the `kImprovedPasswordChangeService` feature.
BASE_DECLARE_FEATURE(kIOSFillRecoveryPassword);

#endif  // BUILDFLAG(IS_IOS)

// Populate the `date_last_filled` timestamp for passwords.
BASE_DECLARE_FEATURE(kPasswordDateLastFilled);

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

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables "Needs access to keychain, restart chrome" bubble and banner.
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Sets request criticality when calling leak check service to detect leaked
// passwords.
BASE_DECLARE_FEATURE(kSetLeakCheckRequestCriticality);

// Shows recovery password for the improved password change flow in the
// management UI.
BASE_DECLARE_FEATURE(kShowRecoveryPassword);

// Shows a tab with password change instead of bubble/settings page after
// successful password change.
BASE_DECLARE_FEATURE(kShowTabWithPasswordChangeOnSuccess);

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
#endif  // BUILDFLAG(IS_ANDROID)

// Improves PSL matching capabilities by utilizing PSL-extension list from
// affiliation service. It fixes problem with incorrect password suggestions on
// websites like slack.com.
BASE_DECLARE_FEATURE(kUseExtensionListForPSLMatching);

// Marks all submitted credentials as leaked, useful for testing of a password
// leak dialog.
BASE_DECLARE_FEATURE(kMarkAllCredentialsAsLeaked);

// Enables improvements to password change functionality.
BASE_DECLARE_FEATURE(kImprovedPasswordChangeService);

// Runs the Password Change flow (enabled by kImprovedPasswordChangeService
// feature flag) in a user-visible background tab.
BASE_DECLARE_FEATURE(kRunPasswordChangeInBackgroundTab);

#if BUILDFLAG(IS_ANDROID)
// The feature flag for reloading passwords when the trusted vault encryption
// state changes.
BASE_DECLARE_FEATURE(kReloadPasswordsOnTrustedVaultEncryptionChange);

// The feature flag for showing an action to unlock passwords in case of a
// trusted vault error in the keyboard accessory.
BASE_DECLARE_FEATURE(kRetrieveTrustedVaultKeyKeyboardAccessoryAction);
#endif  // BUILDFLAG(IS_ANDROID)

inline constexpr base::FeatureParam<std::string>
    kPasswordChangeSuccessSurveyTriggerId{
        &kImprovedPasswordChangeService, "PasswordChangeSuccessSurveyTriggerId",
        /*default_value=*/""};
inline constexpr base::FeatureParam<std::string>
    kPasswordChangeErrorSurveyTriggerId{&kImprovedPasswordChangeService,
                                        "PasswordChangeErrorSurveyTriggerId",
                                        /*default_value=*/""};
inline constexpr base::FeatureParam<std::string>
    kPasswordChangeCanceledSurveyTriggerId{
        &kImprovedPasswordChangeService,
        "PasswordChangeCanceledSurveyTriggerId",
        /*default_value=*/""};
inline constexpr base::FeatureParam<std::string>
    kPasswordChangeDelayedSurveyTriggerId{
        &kImprovedPasswordChangeService, "PasswordChangeDelayedSurveyTriggerId",
        /*default_value=*/""};

// All features parameters in alphabetical order.

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
