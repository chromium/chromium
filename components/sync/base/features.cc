// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kCacheBaseEntitySpecificsInMetadata,
             "CacheBaseEntitySpecificsInMetadata",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             "DeferredSyncStartupCustomDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIgnoreSyncEncryptionKeysLongMissing,
             "IgnoreSyncEncryptionKeysLongMissing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordNotesWithBackup,
             "PasswordNotesWithBackup",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncAndroidLimitNTPPromoImpressions,
             "SyncAndroidLimitNTPPromoImpressions",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncAutofillWalletUsageData,
             "SyncAutofillWalletUsageData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncExtensionTypesThrottling,
             "SyncExtensionTypesThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSegmentationDataType,
             "SyncSegmentationDataType",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kChromeOSSyncedSessionSharing,
             "ChromeOSSyncedSessionSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
             "SyncTrustedVaultDegradedRecoverabilityHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Keep this entry in sync with the equivalent name in
// ChromeFeatureList.java.
BASE_FEATURE(kSyncTrustedVaultVerifyDeviceRegistration,
             "SyncTrustedVaultVerifyDeviceRegistration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseSyncInvalidations,
             "UseSyncInvalidations",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncPersistInvalidations,
             "SyncPersistInvalidations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUseSyncInvalidationsForWalletAndOffer,
             "UseSyncInvalidationsForWalletAndOffer",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated,
             "SkipInvalidationOptimizationsWhenDeviceInfoUpdated",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableHistoryDataType,
             "SyncEnableHistoryDataType",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataType,
             "SyncEnableContactInfoDataType",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeEarlyReturnNoDatabase,
             "SyncEnableContactInfoDataTypeEarlyReturnNoDatabase",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeInTransportMode,
             "SyncEnableContactInfoDataTypeInTransportMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
             "SyncEnableContactInfoDataTypeForCustomPassphraseUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers,
             "SyncEnableContactInfoDataTypeForDasherUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnforceBookmarksCountLimit,
             "SyncEnforceBookmarksCountLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAllowClearingMetadataWhenDataTypeIsStopped,
             "SyncAllowClearingMetadataWhenDataTypeIsStopped",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableLoadModelsTimeout,
             "SyncEnableLoadModelsTimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnforcePreferencesAllowlist,
             "SyncEnforcePreferencesAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePreferencesAccountStorage,
             "EnablePreferencesAccountStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
BASE_FEATURE(kSyncIgnoreSyncRequestedPreference,
             "SyncIgnoreSyncRequestedPreference",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

BASE_FEATURE(kSyncPollImmediatelyOnEveryStartup,
             "SyncPollImmediatelyOnEveryStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncPollWithoutDelayOnStartup,
             "SyncPollWithoutDelayOnStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kIndicateAccountStorageErrorInAccountCell,
             "IndicateAccountStorageErrorInAccountCell",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_ANDROID) || !BUILDFLAG(IS_IOS)
BASE_FEATURE(kSyncWebauthnCredentials,
             "SyncWebauthnCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) || !BUILDFLAG(IS_IOS)

BASE_FEATURE(kSyncIgnoreGetUpdatesRetryDelay,
             "SyncIgnoreGetUpdatesRetryDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnablePersistentStorageForAccountPreferences,
             "SyncEnablePersistentStorageForAccountPreferences",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAvoidReconfigurationIfAlreadyStopping,
             "SyncAvoidReconfigurationIfAlreadyStopping",
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace syncer
