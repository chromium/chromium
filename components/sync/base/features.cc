// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             "DeferredSyncStartupCustomDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kIgnoreSyncEncryptionKeysLongMissing,
             "IgnoreSyncEncryptionKeysLongMissing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPasswordNotesWithBackup,
             "PasswordNotesWithBackup",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSharingOfferKeyPairBootstrap,
             "SharingOfferKeyPairBootstrap",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSharingOfferKeyPairRead,
             "SharingOfferKeyPairRead",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncAndroidLimitNTPPromoImpressions,
             "SyncAndroidLimitNTPPromoImpressions",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncAutofillWalletUsageData,
             "SyncAutofillWalletUsageData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAutofillWalletCredentialData,
             "SyncAutofillWalletCredentialData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSegmentationDataType,
             "SyncSegmentationDataType",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kSyncChromeOSExplicitPassphraseSharing,
             "SyncChromeOSExplicitPassphraseSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncChromeOSAppsToggleSharing,
             "SyncChromeOSAppsToggleSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kChromeOSSyncedSessionSharing,
             "ChromeOSSyncedSessionSharing",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kSyncPersistInvalidations,
             "SyncPersistInvalidations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated,
             "SkipInvalidationOptimizationsWhenDeviceInfoUpdated",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

BASE_FEATURE(kEnablePreferencesAccountStorage,
             "EnablePreferencesAccountStorage",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncPollImmediatelyOnEveryStartup,
             "SyncPollImmediatelyOnEveryStartup2",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kSyncWebauthnCredentials,
             "SyncWebauthnCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kSyncIgnoreGetUpdatesRetryDelay,
             "SyncIgnoreGetUpdatesRetryDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsSaveNudgeDelay,
             "TabGroupsSaveNudgeDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReplaceSyncPromosWithSignInPromos,
             "ReplaceSyncPromosWithSignInPromos",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableBookmarksAccountStorage,
             "EnableBookmarksAccountStorage",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_IOS)
);

#if !BUILDFLAG(IS_IOS)
BASE_FEATURE(kEnableBookmarkFoldersForAccountStorage,
             "EnableBookmarkFoldersForAccountStorage",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_IOS)

BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
             "ReadingListEnableSyncTransportModeUponSignIn",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_IOS)
);

BASE_FEATURE(kSyncEnableWalletMetadataInTransportMode,
             "SyncEnableWalletMetadataInTransportMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableWalletOfferInTransportMode,
             "SyncEnableWalletOfferInTransportMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEntityMetadataRecordDeletedByVersionOnLocalDeletion,
             "SyncEntityMetadataRecordDeletedByVersionOnLocalDeletion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncPasswordCleanUpAccidentalBatchDeletions,
             "SyncPasswordCleanUpAccidentalBatchDeletions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableBatchUploadLocalData,
             "SyncEnableBatchUploadLocalData",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kRestoreSyncedPlaceholderTabs,
             "RestoreSyncedPlaceholderTabs",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncSessionOnVisibilityChanged,
             "SyncSessionOnVisibilityChanged",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncDecoupleAddressPaymentSettings,
             "SyncDecoupleAddressPaymentSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1374718): Delete feature toggle after a grace period.
BASE_FEATURE(kSyncAlwaysForceImmediateStartIfTransportDataMissing,
             "SyncAlwaysForceImmediateStartIfTransportDataMissing",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace syncer
