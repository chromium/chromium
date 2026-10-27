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

BASE_DECLARE_FEATURE(kActorLogin);
// Killswitch for the conflicting permission cleanup. Conflicting permissions
// are the ones granted for 2 different accounts on the same website.
BASE_DECLARE_FEATURE(kActorLoginConflictingPermissionCleanup);
BASE_DECLARE_FEATURE(kActorLoginLocalClassificationModel);

#if BUILDFLAG(IS_ANDROID)
// Enables the Actor Login Permissions Settings UI on Android.
BASE_DECLARE_FEATURE(kActorLoginPermissionsUi);
#endif

// Enables syncing password permissions.
BASE_DECLARE_FEATURE(kActorLoginSyncsPasswordPermissions);

#if BUILDFLAG(IS_ANDROID)
// Enables the "Select password" option in the right-click context menu on
// Android.
BASE_DECLARE_FEATURE(kAndroidSelectPasswordContextMenu);
#endif  // BUILDFLAG(IS_ANDROID)

// Enables using clientside form classifier predictions for password forms.
BASE_DECLARE_FEATURE(kApplyClientsideModelPredictionsForPasswordTypes);

// When enabled, updates to shared existing passwords from the same sender are
// auto-approved.
BASE_DECLARE_FEATURE(kAutoApproveSharedPasswordUpdatesFromSameSender);

// If enabled, the user may be prompted a HaTS survey about the personalization
// experience and the perceived trust they have in the features.
BASE_DECLARE_FEATURE(kAutofillPersonalizationAndTrustPasswordSurvey);

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

#if BUILDFLAG(IS_WIN)
// Killswitch for invoking the pending callback and recording kCanceledByChrome
// when DeviceAuthenticatorWin::Cancel() is called.
// TODO(crbug.com/515090582): Remove in M163.
BASE_DECLARE_FEATURE(kDeviceAuthenticatorWinCancel);
#endif  // BUILDFLAG(IS_WIN)

// Killswitch to revert the device reauth refactor that splits availability
// checking from authentication into `DeviceAuthAvailabilityChecker`.
BASE_DECLARE_FEATURE(kDeviceReauthRefactor);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables Phase 2 of the Mojo JavaScript API migration for the password
// manager, replacing the legacy passwordsPrivate extension API.
BASE_DECLARE_FEATURE(kEnablePasswordManagerMojoApiPhase2);

// Enables the Desktop Trusted Vault unlock UI flow.
BASE_DECLARE_FEATURE(kTrustedVaultDesktopUnlock);

// Enables the Desktop Passkey unlock promo card UI flow.
BASE_DECLARE_FEATURE(kPasskeyUnlockPromo);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Cross domain credential data is not previewed by the manual fallback
// suggestion popup.
BASE_DECLARE_FEATURE(kFallbackNoPreviewForCrossDomainCredentials);

// Fetches change password url if the credential has been identified as leaked.
// Later change password url is used during password change.
BASE_DECLARE_FEATURE(kFetchChangePasswordUrlForPasswordChange);

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
BASE_DECLARE_FEATURE(kFillOnAccountSelect);

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
#endif  // BUILDFLAG(IS_IOS)

// Marks all submitted credentials as leaked, useful for testing of a password
// leak dialog.
BASE_DECLARE_FEATURE(kMarkAllCredentialsAsLeaked);

#if BUILDFLAG(IS_ANDROID)
// Enables OTP phishing checks.
BASE_DECLARE_FEATURE(kOtpPhishGuard);
#endif  // BUILDFLAG(IS_ANDROID)

// PasswordManagerClient::IsFillingEnabled returns `false` for opaque origins.
// Intended as a kill-switch.
BASE_DECLARE_FEATURE(kPasswordBlockOpaqueOrigins);

#if BUILDFLAG(IS_ANDROID)
// When enabled, DeletionOrigin is sent to Android Backend for password
// deletions.
BASE_DECLARE_FEATURE(kPassDeletionOriginToAndroidBackend);

// The minimum GMS version required to send deletion origin to Android Backend.
extern const base::FeatureParam<int> kPassDeletionOriginMinGmsVersion;
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
// Enables displaying a warning banner for compromised credentials in the
// password details card.
BASE_DECLARE_FEATURE(kPasswordCompromiseWarningInDetailsCard);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Enables the actor PasswordChangeTool.
BASE_DECLARE_FEATURE(kChangePasswordTool);

// Populate the `date_last_filled` timestamp for passwords.
BASE_DECLARE_FEATURE(kPasswordDateLastFilled);

// Enables running the clientside form classifier to parse password forms.
BASE_DECLARE_FEATURE(kPasswordFormClientsideClassifier);

// Kill switch for dropping fullscreen and checking pointer lock before
// showing or accepting password generation popups.
BASE_DECLARE_FEATURE(kPasswordGenerationDropFullscreenAndCheckPointerLock);

// Enables logging the content of chrome://password-manager-internals to the
// terminal.
BASE_DECLARE_FEATURE(kPasswordManagerLogToTerminal);

// Enables the OnDeviceEncryptionMetricsReporter service which tracks
// on-device encryption states of passwords and passkeys and records
// readiness metrics.
BASE_DECLARE_FEATURE(kPasswordManagerOnDeviceEncryptionMetricsReporter);

// Performs additional security checks wrt. the triggering frame before adding
// the "Select password" entry to the context menu.
BASE_DECLARE_FEATURE(kPasswordManualFallbackSecurityChecks);

// Doesn't trigger the password manual fallback flow for opaque origins.
// TODO(crbug.com/562276485): Clean up when launched.
BASE_DECLARE_FEATURE(kPasswordManualFallbackSkipOpaqueOrigins);

// When enabled, the user can resolve actionable errors in context during a
// password save flow.
BASE_DECLARE_FEATURE(kPasswordSaveInContextErrorResolution);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Enables resolution of Trusted Vault encryption errors directly within the
// sign-in autofill dropdown.
BASE_DECLARE_FEATURE(kPasswordSignInInContextErrorResolution);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Kill switch for preventing non-HTTP suggestions from being offered in
// manual fallback flows on HTTP pages.
// TODO(crbug.com/562975181): Clean up in M160.
BASE_DECLARE_FEATURE(kPreventNonHttpSuggestionsInHttpManualFallbackFlows);

// Prevents password manager from showing save/update UI on federated login.
BASE_DECLARE_FEATURE(kPreventPasswordManagerOnFederatedLogin);

// Enables publishing of the password readines metric for users with the trusted
// vault passphrase type.
BASE_DECLARE_FEATURE(kRecordPasswordReadiness);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Enables "Needs access to keychain, restart chrome" bubble and banner.
BASE_DECLARE_FEATURE(kRestartToGainAccessToKeychain);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

// Shows a tab with password change instead of bubble/settings page after
// successful password change.
BASE_DECLARE_FEATURE(kShowTabWithPasswordChangeOnSuccess);

#if BUILDFLAG(IS_ANDROID)
// When enabled, Chrome doesn't trigger its own biometric re-auth before filling
// credentials selected in Android Credential Manager, since CredMan already
// authenticates the user. Disable to restore Chrome's re-auth.
// Kill switch. TODO(crbug.com/352720504): Clean up once the CredMan re-auth is
// stable.
BASE_DECLARE_FEATURE(kSkipChromeReauthForCredMan);
#endif  // BUILDFLAG(IS_ANDROID)

// Displays at least the decryptable and never saved logins in the password
// manager
BASE_DECLARE_FEATURE(kSkipUndecryptablePasswords);

// Starts passwords resync when undecryptable passwords are detected.
BASE_DECLARE_FEATURE(kTriggerPasswordResyncWhenUndecryptablePasswordsDetected);

// Killswitch: When enabled, unifies the leak checkup dialog variations into a
// single checkup dialog and removes the check for whether the credential was
// saved on the current site.
BASE_DECLARE_FEATURE(kUnifiedPasswordLeakDialog);

// Controls rollout of storing `PasswordString` password values encrypted in
// memory via `crypto::ProcessBoundU16String`. When disabled, `PasswordString`
// keeps the password as a plaintext `std::u16string`; when enabled, it uses
// `crypto::ProcessBoundU16String`. The choice is captured at `PasswordString`
// construction time.
// TODO(crbug.com/513276101): Remove flag after rollout completes
BASE_DECLARE_FEATURE(kUseProcessBoundPasswordString);

// All features parameters in alphabetical order.

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FEATURES_PASSWORD_FEATURES_H_
