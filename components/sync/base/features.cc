// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/features.h"

#include "base/feature_list.h"

namespace syncer {

BASE_FEATURE(kDeferredSyncStartupCustomDelay,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncSharedComment, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAIThread, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncContextualTask, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncGeminiThread, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncEncryptedTabContextContainer,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncThemesIos, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewTabPageCustomizationThemeSync,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncLoyaltyCardMetadata, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncNotebook, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncJourney, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncAutofillEntitySuppression, base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kUnoPhase2FollowUp, base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsContactInfoDataTypeForCustomPassphraseUsersEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return base::FeatureList::IsEnabled(
      kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
#else
  return true;
#endif
}

BASE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSeparateLocalAndAccountSearchEngines,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReplaceSyncPromosWithSignInPromos,
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

bool IsReplaceSyncPromosWithSignInPromosEnabled() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  return true;
#else
  return base::FeatureList::IsEnabled(kReplaceSyncPromosWithSignInPromos);
#endif
}

// Like DECLARE_SYNC_AUTOFILL_AI_FEATURE but for the definition.
#if BUILDFLAG(IS_ANDROID)
#define DEFINE_SYNC_AUTOFILL_AI_FEATURE(feature_name)                         \
  BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(                                     \
      feature_name, base::FEATURE_DISABLED_FOR_COUNTRIES, "ao", "at", "au",   \
      "be", "bg", "br", "ca", "ch", "cy", "cz", "de", "dk", "dz", "ee", "es", \
      "fi", "fr", "gb", "gr", "hr", "hu", "id", "ie", "in", "is", "it", "jp", \
      "kr", "li", "lt", "lu", "lv", "md", "mk", "ml", "mt", "nl", "no", "om", \
      "pl", "pt", "ro", "se", "si", "sk", "th")
#elif BUILDFLAG(IS_IOS)
#define DEFINE_SYNC_AUTOFILL_AI_FEATURE(feature_name) \
  BASE_FEATURE_WITH_COUNTRY_RESTRICTIONS(             \
      feature_name, base::FEATURE_ENABLED_FOR_COUNTRIES, "us")
#else
#define DEFINE_SYNC_AUTOFILL_AI_FEATURE(feature_name) \
  BASE_FEATURE(feature_name, base::FEATURE_ENABLED_BY_DEFAULT)
#endif

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncAccountSettings);

#if BUILDFLAG(IS_IOS)
DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncAutofillValuable);
#endif

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncAutofillValuableMetadata);

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncWalletFlightReservations);

DEFINE_SYNC_AUTOFILL_AI_FEATURE(kSyncWalletVehicleRegistrations);

#undef DEFINE_SYNC_AUTOFILL_AI_FEATURE

BASE_FEATURE(kSpellcheckSeparateLocalAndAccountDictionaries,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn,
             base::FEATURE_ENABLED_BY_DEFAULT);

bool IsReadingListAccountStorageEnabled() {
  return base::FeatureList::IsEnabled(
      syncer::kReadingListEnableSyncTransportModeUponSignIn);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSeparateLocalAndAccountThemes, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncTrustedVaultErrorMessageDuration,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kSyncUsePropagatedAccessToken, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSyncSearchEnginesAndroidLFF, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kAlwaysRegisterSessionsInvalidationsAndroid,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kEstimateNewSignInUsersWithFinchAvailablePopulation,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

BASE_FEATURE(kSyncDisambiguateDeviceNamesWithChannel,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncNigoriAuthenticateIV, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSyncMigrateFromImplicitPassphrase,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncMigrateLoopbackServerBookmarksToClientTagHash,
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_CHROMEOS)
BASE_FEATURE(kSyncDoNotSyncAppsAndAppSettings,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSyncWipeAppsAndAppSettingsData,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_FEATURE(kDeviceTabVisibilitySettings, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCrossDeviceTabsInAppMenu, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

BASE_FEATURE(kSyncRecordDeviceStatisticsMetrics,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace syncer
