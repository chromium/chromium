// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_

// This file defines all password manager features used in the browser process.
// Prefer adding new features here instead of "core/common/".

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace password_manager::features {
// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if !BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kActorLogin);
// Killswitch for the conflicting permission cleanup. Conflicting permissions
// are the ones granted for 2 different accounts on the same website.
BASE_DECLARE_FEATURE(kActorLoginConflictingPermissionCleanup);
BASE_DECLARE_FEATURE(kActorLoginLocalClassificationModel);
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
// When enabled, it completely ignores existing permanent permissions
// and does not store new ones.
// TODO(crbug.com/507403760): Remove once the permissions management UI is
// available.
BASE_DECLARE_FEATURE(kActorLoginNoPermanentPermissionsAndroid);
// Enables the Actor Login Permissions Settings UI on Android.
BASE_DECLARE_FEATURE(kActorLoginPermissionsUi);
#endif

// Enables syncing password permissions.
BASE_DECLARE_FEATURE(kActorLoginSyncsPasswordPermissions);

#if !BUILDFLAG(IS_IOS)
// Enables logging quality for actor login.
BASE_DECLARE_FEATURE(kActorLoginQualityLogs);
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
// Enables filling of OTPs received via SMS on Android.
BASE_DECLARE_FEATURE(kAndroidSmsOtpFilling);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables using clientside form classifier predictions for password forms.
BASE_DECLARE_FEATURE(kApplyClientsideModelPredictionsForPasswordTypes);

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

// Waits for the page to reach stability before triggering any password change
// actions.
BASE_DECLARE_FEATURE(kAwaitPageStabilityForPasswordChange);
extern const base::FeatureParam<base::TimeDelta> kAwaitPageStabilityTimeout;

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Retries capturing annotated page context during automated password change if
// capturing failed for some reason.
BASE_DECLARE_FEATURE(kRetryCapturePageContent);
extern const base::FeatureParam<base::TimeDelta> kCapturePageContentDelay;
extern const base::FeatureParam<int> kCapturePageContentRetryCount;

// Enables Biometrics for the Touch To Fill feature. This only effects Android.
BASE_DECLARE_FEATURE(kBiometricTouchToFill);

// Kill switch for calling OnAddPasswordFillData() asynchronously to avoid
// reentrant AutofillManager::Observer events.
// TODO(crbug.com/500883329): Clean up after M141 BP (June 29, 2026).
BASE_DECLARE_FEATURE(kCallOnAddPasswordFillDataAsynchronously);

// Checks if submitted form is identical to an observed form before evaluating
// login success/failure.
BASE_DECLARE_FEATURE(kCheckIfSubmittedFormIdenticalToObserved);

// Delete undecryptable passwords from the login database.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswords);

// Delete undecryptable passwords from the store when Sync is active.
BASE_DECLARE_FEATURE(kClearUndecryptablePasswordsOnSync);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables the Unified UI for the Password Manager.
BASE_DECLARE_FEATURE(kCredentialManagementUnifiedUi);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables debug data popups on OTP fields for manual testing of
// one-time-passwords. Only for OTP detection testing, not intended to be
// launched.
BASE_DECLARE_FEATURE(kDebugUiForOtps);

// When enabled, automated password change won't be offered when the form
// contains new password field.
BASE_DECLARE_FEATURE(kDisablePasswordChangeFromNewPasswordFields);

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
// When enabled, the user can be prompted to retrieve the trusted vault key
// during a password saving flow.
BASE_DECLARE_FEATURE(kInFlowTrustedVaultKeyRetrievalAndroid);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// When enabled, the user can be prompted to retrieve the trusted vault key
// during a password saving flow.
BASE_DECLARE_FEATURE(kInFlowTrustedVaultKeyRetrievalIos);

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
#endif  // BUILDFLAG(IS_IOS)

// Marks all submitted credentials as leaked, useful for testing of a password
// leak dialog.
BASE_DECLARE_FEATURE(kMarkAllCredentialsAsLeaked);

#if BUILDFLAG(IS_ANDROID)
// Enables OTP phishing checks.
BASE_DECLARE_FEATURE(kOtpPhishGuard);

// The minimum GMS version required to send deletion origin to Android Backend.
extern const base::FeatureParam<int> kPassDeletionOriginMinGmsVersion;

// When enabled, DeletionOrigin is sent to Android Backend for password
// deletions.
BASE_DECLARE_FEATURE(kPassDeletionOriginToAndroidBackend);
#endif  // BUILDFLAG(IS_ANDROID)

// Populate the `date_last_filled` timestamp for passwords.
BASE_DECLARE_FEATURE(kPasswordDateLastFilled);

// Enables running the clientside form classifier to parse password forms.
BASE_DECLARE_FEATURE(kPasswordFormClientsideClassifier);

// Enables offering credentials for filling across grouped domains.
BASE_DECLARE_FEATURE(kPasswordFormGroupedAffiliations);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kPasswordSaveInContextErrorResolutionOnDesktop);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// When enabled, the password store triggers the `OnErrorStateChanged`
// notifications.
BASE_DECLARE_FEATURE(kPasswordStorePropagatesActionableErrors);

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

// Prevents password manager from showing save/update UI on federated login.
BASE_DECLARE_FEATURE(kPreventPasswordManagerOnFederatedLogin);

// Prevents offering Automatic Password Change on federated login.
BASE_DECLARE_FEATURE(kPreventAPCOnFederatedLogin);

// Triggers password change glow invoking Glic from settings.
// This flag is only for the prototype version.
BASE_DECLARE_FEATURE(kPasswordCheckupPrototype);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables "Needs access to keychain, restart chrome" bubble and banner.
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Shows a confirmation dialog before filling grouped credentials from the
// manual fallback popup on Desktop.
BASE_DECLARE_FEATURE(kShowConfirmationForGroupedCredentials);

// Shows a tab with password change instead of bubble/settings page after
// successful password change.
BASE_DECLARE_FEATURE(kShowTabWithPasswordChangeOnSuccess);

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

// Starts passwords resync when undecryptable passwords are detected.
BASE_DECLARE_FEATURE(kTriggerPasswordResyncWhenUndecryptablePasswordsDetected);


// The feature enables the use of detached Widget during password change
// to which WebContents is attached. This helps to resolve the problem
// that requestAnimationFrame() is not fired on a detached WebContents.
BASE_DECLARE_FEATURE(kUseDetachedWidget);


#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop

// Moves the "Use a passkey / Use a different passkey" to the context menu from
// the autofill dropdown. This is now decoupled from
// "PasswordManualFallbackAvailable" flag.
BASE_DECLARE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu);

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables the "Use a passkey / Use a different passkey" in the password manual
// fallback.
BASE_DECLARE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInManualFallback);

// All features parameters in alphabetical order.

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
