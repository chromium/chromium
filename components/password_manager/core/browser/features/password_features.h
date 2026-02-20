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

#if !BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kActorLogin);
// Enables FedCM support for Actor Login.
BASE_DECLARE_FEATURE(kActorLoginFederatedLoginSupport);
// Enables Actor Login form finding with async check
BASE_DECLARE_FEATURE(kActorLoginFieldVisibilityCheck);
// Ensures that `GetCredentials` differentiates between no saved credentials
// and no signin form found on the page.
BASE_DECLARE_FEATURE(kActorLoginGetCredentialsNoLoginForm);
BASE_DECLARE_FEATURE(kActorLoginLocalClassificationModel);
// Enables the usage of temporary permissions across affiliated origins for
// Actor Login.
BASE_DECLARE_FEATURE(kActorLoginPermissionsUseStrongAffiliations);
BASE_DECLARE_FEATURE(kActorLoginReauthTaskRefocus);
// Enables logging quality for actor login.
BASE_DECLARE_FEATURE(kActorLoginQualityLogs);
// Enables finding and filling forms in same-site iframes for actor login.
BASE_DECLARE_FEATURE(kActorLoginSameSiteIframeSupport);
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

// Enables filling of change password form by typing.
BASE_DECLARE_FEATURE(kFillChangePasswordFormByTyping);

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
#endif  // BUILDFLAG(IS_ANDROID)

// Populate the `date_last_filled` timestamp for passwords.
BASE_DECLARE_FEATURE(kPasswordDateLastFilled);

// When enabled, the callback in `OnButtonClicked` will be invoked immediately
// with the result.
BASE_DECLARE_FEATURE(kPasswordChangeImmediateSubmission);

// Enables running the clientside form classifier to parse password forms.
BASE_DECLARE_FEATURE(kPasswordFormClientsideClassifier);

// Enables offering credentials for filling across grouped domains.
BASE_DECLARE_FEATURE(kPasswordFormGroupedAffiliations);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables "chunking" generated passwords by adding hyphens every 4 characters
// to make them more readable.
BASE_DECLARE_FEATURE(kPasswordGenerationChunking);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_DECLARE_FEATURE(kPasswordSaveInContextErrorResolutionOnDesktop);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// When enabled, the password store triggers the `OnErrorStateChanged`
// notifications.
BASE_DECLARE_FEATURE(kPasswordStorePropagatesActionableErrors);

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

// Updates password change flow to await for local ML model availability. The
// model has a superior performance compared to existing password manager
// classifications.
BASE_DECLARE_FEATURE(kProactivelyDownloadModelForPasswordChange);

// Removes country and language restrictions for password change. This allows to
// control locale/country server side.
BASE_DECLARE_FEATURE(kReduceRequirementsForPasswordChange);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables "Needs access to keychain, restart chrome" bubble and banner.
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Runs the Password Change flow (enabled by kImprovedPasswordChangeService
// feature flag) in a user-visible background tab.
BASE_DECLARE_FEATURE(kRunPasswordChangeInBackgroundTab);

// Shows a tab with password change instead of bubble/settings page after
// successful password change.
BASE_DECLARE_FEATURE(kShowTabWithPasswordChangeOnSuccess);

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

// Starts passwords resync when undecryptable passwords are detected.
BASE_DECLARE_FEATURE(kTriggerPasswordResyncWhenUndecryptablePasswordsDetected);

// Updates password change flow to use the refined prompt on Open form step. The
// prompt uses the list of interactable actionables on the web page to identify
// the button, which opens the password change form.
BASE_DECLARE_FEATURE(kUseActionablesForImprovedPasswordChange);

// The feature enables the use of detached Widget during password change
// to which WebContents is attached. This helps to resolve the problem
// that requestAnimationFrame() is not fired on a detached WebContents.
BASE_DECLARE_FEATURE(kUseDetachedWidget);

// Handles user intervention in the Password Change flow for all steps except
// IS_LOGGED_IN_STEP.
BASE_DECLARE_FEATURE(kUserInterventionForPasswordChange);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop

// Moves the "Use a passkey / Use a different passkey" to the context menu from
// the autofill dropdown. This is now decoupled from
// "PasswordManualFallbackAvailable" flag.
BASE_DECLARE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu);

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// All features parameters in alphabetical order.

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
