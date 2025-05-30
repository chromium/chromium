// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             "DeferredSyncStartupCustomDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAutofillLoyaltyCard,
             "SyncAutofillLoyaltyCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSharedTabGroupAccountData,
             "SyncSharedTabGroupAccountData",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kUnoPhase2FollowUp,
             "UnoPhase2FollowUp",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncAutofillWalletCredentialData,
             "SyncAutofillWalletCredentialData",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
             "SyncEnableContactInfoDataTypeForCustomPassphraseUsers",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers,
             "SyncEnableContactInfoDataTypeForDasherUsers",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTabGroupsSaveNudgeDelay,
             "TabGroupsSaveNudgeDelay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountSearchEngines,
             "SeparateLocalAndAccountSearchEngines",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kReplaceSyncPromosWithSignInPromos,
             "ReplaceSyncPromosWithSignInPromos",
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncSupportAlwaysSyncingPriorityPreferences,
             "SyncSupportAlwaysSyncingPriorityPreferences",
#if BUILDFLAG(IS_CHROMEOS)
             // TODO(crbug.com/418991364): Enable by default once prefs account
             // storage is launched on ChromeOS.
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kEnableBookmarksSelectedTypeOnSigninForTesting,
             "EnableBookmarksSelectedTypeOnSigninForTesting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEngineAvoidFaviconOnlyCommits,
             "SearchEngineAvoidFaviconOnlyCommits",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
             "ReadingListEnableSyncTransportModeUponSignIn",
             base::FEATURE_DISABLED_BY_DEFAULT
);

bool IsReadingListAccountStorageEnabled() {
  return base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

BASE_FEATURE(kSyncPasswordCleanUpAccidentalBatchDeletions,
             "SyncPasswordCleanUpAccidentalBatchDeletions",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMigrateAccountPrefs,
             "MigrateAccountPrefs",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

// Enabled by default, intended as a kill switch.
BASE_FEATURE(kSyncReadingListBatchUploadSelectedItems,
             "SyncReadingListBatchUploadSelectedItems",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountThemes,
             "SeparateLocalAndAccountThemes",
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kThemesBatchUpload,
             "ThemesBatchUpload",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient,
             "SyncIncreaseNudgeDelayForSingleClient",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebApkBackupAndRestoreBackend,
             "WebApkBackupAndRestoreBackend",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncEnablePasswordsSyncErrorMessageAlternative,
             "SyncEnablePasswordsSyncErrorMessageAlternative",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kSyncTrustedVaultInfobarImprovements,
             "SyncTrustedVaultInfobarImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kSyncTrustedVaultInfobarMessageImprovements,
             "SyncTrustedVaultInfobarMessageImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace syncer
