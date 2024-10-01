// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             "DeferredSyncStartupCustomDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kEnableBatchUploadFromSettings,
             "EnableBatchUploadFromSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUnoPhase2FollowUp,
             "UnoPhase2FollowUp",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncAutofillWalletUsageData,
             "SyncAutofillWalletUsageData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAutofillWalletCredentialData,
             "SyncAutofillWalletCredentialData",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncPlusAddressSetting,
             "SyncPlusAddressSetting",
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

BASE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated,
             "SkipInvalidationOptimizationsWhenDeviceInfoUpdated",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeInTransportMode,
             "SyncEnableContactInfoDataTypeInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
             "SyncEnableContactInfoDataTypeForCustomPassphraseUsers",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnablePasswordsAccountStorageForSyncingUsers,
             "EnablePasswordsAccountStorageForSyncingUsers",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnablePasswordsAccountStorageForNonSyncingUsers,
             "EnablePasswordsAccountStorageForNonSyncingUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers,
             "SyncEnableContactInfoDataTypeForDasherUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePreferencesAccountStorage,
             "EnablePreferencesAccountStorage",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if !BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncWebauthnCredentials,
             "SyncWebauthnCredentials",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);
#endif  // !BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncIgnoreGetUpdatesRetryDelay,
             "SyncIgnoreGetUpdatesRetryDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsSaveNudgeDelay,
             "TabGroupsSaveNudgeDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReplaceSyncPromosWithSignInPromos,
             "ReplaceSyncPromosWithSignInPromos",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableBookmarksInTransportMode,
             "SyncEnableBookmarksInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_IOS)
);

BASE_FEATURE(kEnableBookmarksSelectedTypeOnSigninForTesting,
             "EnableBookmarksSelectedTypeOnSigninForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_IOS)
BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
             "ReadingListEnableSyncTransportModeUponSignIn",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsReadingListAccountStorageEnabled() {
  return base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
}
#endif  // !BUILDFLAG(IS_IOS)

BASE_FEATURE(kSyncSharedTabGroupDataInTransportMode,
             "SyncSharedTabGroupDataInTransportMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableWalletMetadataInTransportMode,
             "SyncEnableWalletMetadataInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableWalletOfferInTransportMode,
             "SyncEnableWalletOfferInTransportMode",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEntityMetadataRecordDeletedByVersionOnLocalDeletion,
             "SyncEntityMetadataRecordDeletedByVersionOnLocalDeletion",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncPasswordCleanUpAccidentalBatchDeletions,
             "SyncPasswordCleanUpAccidentalBatchDeletions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountThemes,
             "SeparateLocalAndAccountThemes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient,
             "SyncIncreaseNudgeDelayForSingleClient",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTrustedVaultAutoUpgradeSyntheticFieldTrial,
             "TrustedVaultAutoUpgradeSyntheticFieldTrial",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMoveThemePrefsToSpecifics,
             "MoveThemePrefsToSpecifics",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebApkBackupAndRestoreBackend,
             "WebApkBackupAndRestoreBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncEnableModelTypeLocalDataBatchUploaders,
             "SyncEnableModelTypeLocalDataBatchUploaders",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableExtensionsInTransportMode,
             "kSyncEnableExtensionsInTransportMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace syncer
