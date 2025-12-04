// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAccountSettings, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAutofillLoyaltyCard,
#if !BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enabled by default, intended as a kill switch.
BASE_FEATURE(kSyncMakeAutofillValuableNonEncryptable,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAutofillValuableMetadata, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncMoveValuablesToProfileDb, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSharedTabGroupAccountData, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSharedComment, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAIThread, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncContextualTask, base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kUnoPhase2FollowUp,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);
#endif  // !BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kSyncAutofillWalletCredentialData,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSyncBookmarksLimit, base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, shows a user-actionable error when the bookmarks count limit is
// exceeded.
BASE_FEATURE(kSyncShowBookmarksLimitExceededError,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncResetBookmarksInitialMergeLimitExceededError,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountSearchEngines,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kReplaceSyncPromosWithSignInPromos,
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSyncSupportAlwaysSyncingPriorityPreferences,
#if BUILDFLAG(IS_CHROMEOS)
             // TODO(crbug.com/418991364): Enable by default once prefs account
             // storage is launched on ChromeOS.
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kSyncWalletFlightReservations, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncWalletVehicleRegistrations,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSpellcheckSeparateLocalAndAccountDictionaries,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableBookmarksSelectedTypeOnSigninForTesting,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSearchEngineAvoidFaviconOnlyCommits,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

bool IsReadingListAccountStorageEnabled() {
  return base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kSyncPasswordCleanUpAccidentalBatchDeletions,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMigrateAccountPrefs, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

// Enabled by default, intended as a kill switch.
BASE_FEATURE(kSyncReadingListBatchUploadSelectedItems,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountThemes,
#if BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_CHROMEOS)
);

BASE_FEATURE(kThemesBatchUpload, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kWebApkBackupAndRestoreBackend, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncEnablePasswordsSyncErrorMessageAlternative,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
BASE_FEATURE(kSyncTrustedVaultInfobarMessageImprovements,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

BASE_FEATURE(kSyncUseOsCryptAsync, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncDetermineAccountManagedStatus,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kSyncDetermineAccountManagedStatusTimeout,
                   &kSyncDetermineAccountManagedStatus,
                   "account_managed_status_timeout",
                   base::Seconds(5));

BASE_FEATURE(kSyncEnableNewSyncDashboardUrl, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace syncer
