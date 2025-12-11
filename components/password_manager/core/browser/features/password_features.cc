// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"

namespace password_manager::features {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kActorActiveDisablesFillingOnPageLoad,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLogin, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginFieldVisibilityCheck, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginLocalClassificationModel,
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginReauthTaskRefocus, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginQualityLogs, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kActorLoginSameSiteIframeSupport,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

// Enabled by default in M138. Remove in or after M141.
BASE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutofillReintroduceHybridPasskeyDropdownItem,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kBiometricTouchToFill, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCheckIfSubmittedFormIdenticalToObserved,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCheckVisibilityInChangePasswordFormWaiter,
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kDebugUiForOtps, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadModelForPasswordChange,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDisableFillingOnPageLoadForLeakedCredentials,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kFetchChangePasswordUrlForPasswordChange,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
             // Desktop only since password change is not available on mobile.
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kFillChangePasswordFormByTyping, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFillOnAccountSelect,
             "fill-on-account-select",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kFillRecoveryPassword, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kIosCleanupHangingPasswordFormExtractionRequests,
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kIosPasswordFormExtractionRequestsTimeoutMs = {
    &kIosCleanupHangingPasswordFormExtractionRequests,
    /*name=*/"period-ms", /*default_value=*/250};

BASE_FEATURE(kIOSProactivePasswordGenerationBottomSheet,
             "kIOSProactivePasswordGenerationBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIOSFillRecoveryPassword, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // IS_IOS

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kOtpPhishGuard, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kPasswordDateLastFilled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordFormGroupedAffiliations,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordFormClientsideClassifier,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kPasswordGenerationChunking,
             "PasswordGenerationChunkPassword",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kPasswordManualFallbackAvailable,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kPasswordManagerLogToTerminal, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kRestartToGainAccessToKeychain,
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)


BASE_FEATURE(kShowRecoveryPassword, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShowTabWithPasswordChangeOnSuccess,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kThrottlePasswordChangeDialog, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSkipUndecryptablePasswords,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kStopLoginCheckOnFailedLogin, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTriggerPasswordResyncAfterDeletingUndecryptablePasswords,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTriggerPasswordResyncWhenUndecryptablePasswordsDetected,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kMarkAllCredentialsAsLeaked, base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kEnablePasswordManagerMojoApi, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kImprovedPasswordChangeService, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRunPasswordChangeInBackgroundTab,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReduceRequirementsForPasswordChange,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kReloadPasswordsOnTrustedVaultEncryptionChange,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRetrieveTrustedVaultKeyKeyboardAccessoryAction,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kUseActionablesForImprovedPasswordChange,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace password_manager::features
