// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"

namespace password_manager::features {

BASE_FEATURE(kAutoApproveSharedPasswordUpdatesFromSameSender,
             "AutoApproveSharedPasswordUpdatesFromSameSender",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kAutofillPasswordUserPerceptionSurvey,
             "AutofillPasswordUserPerceptionSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Default enabled in M131. Remove in or after M134.
BASE_FEATURE(kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu,
             "WebAuthnUsePasskeyFromAnotherDeviceInContextMenu",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kAuthenticateUsingUserConsentVerifierInteropApi,
             "AuthenticateUsingUserConsentVerifierInteropApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAuthenticateUsingUserConsentVerifierApi,
             "AuthenticateUsingUserConsentVerifierApi",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kBiometricTouchToFill,
             "BiometricTouchToFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearUndecryptablePasswords,
             "ClearUndecryptablePasswords",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearUndecryptablePasswordsOnSync,
             "ClearUndecryptablePasswordsInSync",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_IOS) || \
    BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kFetchGaiaHashOnSignIn,
             "FetchGaiaHashOnSignIn",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kFillOnAccountSelect,
             "fill-on-account-select",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kIosDetectUsernameInUff,
             "IosSaveUsernameInUff",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIOSProactivePasswordGenerationBottomSheet,
             "kIOSProactivePasswordGenerationBottomSheet",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // IS_IOS

BASE_FEATURE(kLocalStateEnterprisePasswordHashes,
             "LocalStateEnterprisePasswordHashes",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop

BASE_FEATURE(kPasswordGenerationChunking,
             "PasswordGenerationChunkPassword",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordGenerationSoftNudge,
             "PasswordGenerationSoftNudge",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif

BASE_FEATURE(kPasswordManagerLogToTerminal,
             "PasswordManagerLogToTerminal",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordManualFallbackAvailable,
             "PasswordManualFallbackAvailable",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kRestartToGainAccessToKeychain,
             "RestartToGainAccessToKeychain",
#if BUILDFLAG(IS_MAC)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kScreenlockReauthPromoCard,
             "ScreenlockReauthPromoCard",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kBiometricsAuthForPwdFill,
             "BiometricsAuthForPwdFill",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

BASE_FEATURE(kSkipUndecryptablePasswords,
             "SkipUndecryptablePasswords",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kTriggerPasswordResyncAfterDeletingUndecryptablePasswords,
             "TriggerPasswordResyncAfterDeletingUndecryptablePasswords",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning,
             "UnifiedPasswordManagerLocalPasswordsAndroidAccessLossWarning",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBiometricAuthIdentityCheck,
             "BiometricAuthIdentityCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearLoginDatabaseForAllMigratedUPMUsers,
             "ClearLoginDatabaseForAllMigratedUPMUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing,
             "UsernameFirstFlowFallbackCrowdsourcing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUsernameFirstFlowWithIntermediateValuesPredictions,
             "UsernameFirstFlowWithIntermediateValuesPredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUsernameFirstFlowWithIntermediateValuesVoting,
             "UsernameFirstFlowWithIntermediateValuesVoting",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseAsyncOsCryptInLoginDatabase,
             "UseAsyncOsCryptInLoginDatabase",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseNewEncryptionMethod,
             "UseNewEncryptionMethod",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEncryptAllPasswordsWithOSCryptAsync,
             "EncryptAllPasswordsWithOSCryptAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace password_manager::features
