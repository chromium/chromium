// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"

namespace password_manager::features {
#if !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kActorLogin, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginConflictingPermissionCleanup,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginLocalClassificationModel,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_IOS)

BASE_FEATURE(kActorLoginSyncsPasswordPermissions,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kActorLoginNoPermanentPermissionsAndroid,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginPermissionsUi, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#if !BUILDFLAG(IS_IOS)
BASE_FEATURE(kActorLoginQualityLogs, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAndroidSmsOtpFilling, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kApplyClientsideModelPredictionsForPasswordTypes,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAutoApproveSharedPasswordUpdatesFromSameSender,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kAutofillPasswordUserPerceptionSurvey,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAwaitPageStabilityForPasswordChange,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kAwaitPageStabilityTimeout = {
    &kAwaitPageStabilityForPasswordChange, "stability_timeout",
    base::Seconds(5)};

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kRetryCapturePageContent, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kCapturePageContentDelay = {
    &kRetryCapturePageContent, "retry_capture_delay", base::Seconds(0)};
const base::FeatureParam<int> kCapturePageContentRetryCount = {
    &kRetryCapturePageContent, "retry_count", 3};

BASE_FEATURE(kBiometricTouchToFill, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCallOnAddPasswordFillDataAsynchronously,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCheckIfSubmittedFormIdenticalToObserved,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClearUndecryptablePasswords,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kClearUndecryptablePasswordsOnSync,
             "ClearUndecryptablePasswordsInSync",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_IOS) || \
    BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)  // Desktop
BASE_FEATURE(kCredentialManagementUnifiedUi, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kPasswordSaveInContextErrorResolutionOnDesktop,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kDebugUiForOtps, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisablePasswordChangeFromNewPasswordFields,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kEnablePasswordManagerMojoApi, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kFetchChangePasswordUrlForPasswordChange,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
             // Desktop only since password change is not available on mobile.
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kFillChangePasswordFormByTyping,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFillOnAccountSelect,
             "fill-on-account-select",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kInFlowTrustedVaultKeyRetrievalAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kInFlowTrustedVaultKeyRetrievalIos,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIosCleanupHangingPasswordFormExtractionRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<int> kIosPasswordFormExtractionRequestsTimeoutMs = {
    &kIosCleanupHangingPasswordFormExtractionRequests,
    /*name=*/"period-ms", /*default_value=*/250};

BASE_FEATURE(kIOSProactivePasswordGenerationBottomSheet,
             "kIOSProactivePasswordGenerationBottomSheet",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // IS_IOS

BASE_FEATURE(kMarkAllCredentialsAsLeaked, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOtpPhishGuard, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

// Temporarily disabled as mitigation for crbug.com/485895402.
BASE_FEATURE(kPasswordDateLastFilled, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordFormClientsideClassifier,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordFormGroupedAffiliations,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordManagerLogToTerminal, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreventPasswordManagerOnFederatedLogin,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPreventAPCOnFederatedLogin, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordStorePropagatesActionableErrors,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kProactivelyDownloadModelForPasswordChange,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordCheckupPrototype, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kRestartToGainAccessToKeychain,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

BASE_FEATURE(kRunPasswordChangeInBackgroundTab,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kShowTabWithPasswordChangeOnSuccess,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipUndecryptablePasswords,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kTriggerPasswordResyncWhenUndecryptablePasswordsDetected,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseDetachedWidget, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUserInterventionForPasswordChange,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop

// Enabled by default in M138. Remove in or after M141.
BASE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu,
             base::FEATURE_ENABLED_BY_DEFAULT);

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace password_manager::features
