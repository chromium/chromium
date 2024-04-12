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

#if BUILDFLAG(IS_WIN)
BASE_FEATURE(kAuthenticateUsingNewWindowsHelloApi,
             "AuthenticateUsingNewWindowsHelloApi",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kBiometricTouchToFill,
             "BiometricTouchToFill",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kButterOnDesktopFollowup,
             "ButterOnDesktopFollowup",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kClearUndecryptablePasswords,
             "ClearUndecryptablePasswords",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearUndecryptablePasswordsOnSync,
             "ClearUndecryptablePasswordsInSync",
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_IOS)
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
BASE_FEATURE(kIOSPasswordSignInUff,
             "IOSPasswordSignInUff",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIosDetectUsernameInUff,
             "IosSaveUsernameInUff",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // IS_IOS

BASE_FEATURE(kLocalStateEnterprisePasswordHashes,
             "LocalStateEnterprisePasswordHashes",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kNewConfirmationBubbleForGeneratedPasswords,
             "NewConfirmationBubbleForGeneratedPasswords",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)  // Desktop
BASE_FEATURE(kPasswordGenerationExperiment,
             "PasswordGenerationExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// TODO(crbug.com/330686628): Keep disabled for Android when enabling on Desktop
// and iOS.
BASE_FEATURE(kPasswordManagerEnableReceiverService,
             "PasswordManagerEnableReceiverService",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/330686628): Keep disabled for Android when enabling on Desktop
// and iOS.
BASE_FEATURE(kPasswordManagerEnableSenderService,
             "PasswordManagerEnableSenderService",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kSharedPasswordNotificationUI,
             "SharedPasswordNotificationUI",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipUndecryptablePasswords,
             "SkipUndecryptablePasswords",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidNoMigration,
             "UnifiedPasswordManagerLocalPasswordsAndroidNoMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
             "UnifiedPasswordManagerLocalPasswordsAndroidWithMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);
// A parameter which reflects the delay when the local passwords migration is
// triggered after Chrome startup in seconds.
constexpr base::FeatureParam<int>
    kLocalPasswordsMigrationToAndroidBackendDelayParam{
        &kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration,
        /* name= */ "local_pwd_migration_delay_seconds",
        /* default_value= */ 5};

int GetLocalPasswordsMigrationToAndroidBackendDelay() {
  return kLocalPasswordsMigrationToAndroidBackendDelayParam.Get();
}

BASE_FEATURE(kUnifiedPasswordManagerSyncOnlyInGMSCore,
             "UnifiedPasswordManagerSyncOnlyInGMSCore",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClearLoginDatabaseForUPMUsers,
             "ClearLoginDatabaseForUPMUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsUnifiedPasswordManagerSyncOnlyInGMSCoreEnabled() {
#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  return false;
#else
  return base::FeatureList::IsEnabled(kUnifiedPasswordManagerSyncOnlyInGMSCore);
#endif
}
#endif

BASE_FEATURE(kUseExtensionListForPSLMatching,
             "UseExtensionListForPSLMatching",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kUsernameFirstFlowFallbackCrowdsourcing,
             "UsernameFirstFlowFallbackCrowdsourcing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUsernameFirstFlowStoreSeveralValues,
             "UsernameFirstFlowStoreSeveralValues",
             base::FEATURE_ENABLED_BY_DEFAULT);
extern const base::FeatureParam<int> kMaxSingleUsernameFieldsToStore{
    &kUsernameFirstFlowStoreSeveralValues, /*name=*/"max_elements",
    /*default_value=*/10};

BASE_FEATURE(kUsernameFirstFlowWithIntermediateValues,
             "UsernameFirstFlowWithIntermediateValues",
             base::FEATURE_ENABLED_BY_DEFAULT);
extern const base::FeatureParam<int> kSingleUsernameTimeToLive{
    &kUsernameFirstFlowWithIntermediateValues, /*name=*/"ttl",
    /*default_value=*/5};

BASE_FEATURE(kUsernameFirstFlowWithIntermediateValuesPredictions,
             "UsernameFirstFlowWithIntermediateValuesPredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUsernameFirstFlowWithIntermediateValuesVoting,
             "UsernameFirstFlowWithIntermediateValuesVoting",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace password_manager::features
