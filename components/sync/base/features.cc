// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kAllowSilentTrustedVaultDeviceRegistration,
             "AllowSilentTrustedVaultDeviceRegistration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCacheBaseEntitySpecificsInMetadata,
             "CacheBaseEntitySpecificsInMetadata",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kIgnoreSyncEncryptionKeysLongMissing,
             "IgnoreSyncEncryptionKeysLongMissing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordNotesWithBackup,
             "PasswordNotesWithBackup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAllowWalletDataInTransportModeWithCustomPassphrase,
             "SyncAllowAutofillWalletDataInTransportModeWithCustomPassphrase",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncAndroidLimitNTPPromoImpressions,
             "SyncAndroidLimitNTPPromoImpressions",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSyncAndroidPromosWithAlternativeTitle,
             "SyncAndroidPromosWithAlternativeTitle",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kSyncAndroidPromosWithIllustration,
             "SyncAndroidPromosWithIllustration",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSyncAndroidPromosWithSingleButton,
             "SyncAndroidPromosWithSingleButton",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kSyncAndroidPromosWithTitle,
             "SyncAndroidPromosWithTitle",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncAutofillWalletUsageData,
             "SyncAutofillWalletUsageData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncExtensionTypesThrottling,
             "SyncExtensionTypesThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncResetPollIntervalOnStart,
             "SyncResetPollIntervalOnStart",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSegmentationDataType,
             "SyncSegmentationDataType",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSendInterestedDataTypes,
             "SyncSendInterestedDataTypes",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kSyncSettingsShowLacrosSideBySideWarning,
             "SyncSettingsShowLacrosSideBySideWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncChromeOSExplicitPassphraseSharing,
             "SyncChromeOSExplicitPassphraseSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncChromeOSAppsToggleSharing,
             "SyncChromeOSAppsToggleSharing",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
             "SyncTrustedVaultDegradedRecoverabilityHandler",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncTrustedVaultPassphrasePromo,
             "SyncTrustedVaultPassphrasePromo",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Keep this entry in sync with the equivalent name in
// ChromeFeatureList.java.
BASE_FEATURE(kSyncTrustedVaultPassphraseRecovery,
             "SyncTrustedVaultPassphraseRecovery",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncTrustedVaultVerifyDeviceRegistration,
             "SyncTrustedVaultVerifyDeviceRegistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncTrustedVaultRedoDeviceRegistration,
             "SyncTrustedVaultRedoDeviceRegistration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncTrustedVaultResetKeysAreStale,
             "SyncTrustedVaultResetKeysAreStale",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncTrustedVaultUseMD5HashedFile,
             "SyncTrustedVaultUseMD5HashedFile",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseSyncInvalidations,
             "UseSyncInvalidations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncPersistInvalidations,
             "SyncPersistInvalidations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseSyncInvalidationsForWalletAndOffer,
             "UseSyncInvalidationsForWalletAndOffer",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated,
             "SkipInvalidationOptimizationsWhenDeviceInfoUpdated",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableHistoryDataType,
             "SyncEnableHistoryDataType",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataType,
             "SyncEnableContactInfoDataType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Causes sync to pause fully for all persistent auth errors, instead of doing
// this exclusively for web signouts.
BASE_FEATURE(kSyncPauseUponAnyPersistentAuthError,
             "SyncPauseUponAnyPersistentAuthError",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnforceBookmarksCountLimit,
             "SyncEnforceBookmarksCountLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncIgnoreAccountWithoutRefreshToken,
             "SyncIgnoreAccountWithoutRefreshToken",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncDoNotPropagateBrowserShutdownToDataTypes,
             "SyncDoNotPropagateBrowserShutdownToDataTypes",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace syncer
