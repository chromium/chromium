// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_FEATURES_H_
#define COMPONENTS_SYNC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace syncer {

// Customizes the delay of a deferred sync startup.
// Note from 04/2024: The first attempt to roll this out on 05/2023 ran into
// performance regressions (go/deferred-startup-experiment-metrics, sorry
// Googlers only). It might still be possible to launch by investigating and
// fixing the performance issues. crbug.com/40872516 tracks that.
BASE_DECLARE_FEATURE(kDeferredSyncStartupCustomDelay);
inline constexpr base::FeatureParam<int>
    kDeferredSyncStartupCustomDelayInSeconds{
        &kDeferredSyncStartupCustomDelay,
        "DeferredSyncStartupCustomDelayInSeconds", 1};

// Enables syncing comments for shared contexts.
BASE_DECLARE_FEATURE(kSyncSharedComment);

// Enables syncing of AI threads across devices.
BASE_DECLARE_FEATURE(kSyncAIThread);

// Enables syncing of contextual tasks.
BASE_DECLARE_FEATURE(kSyncContextualTask);

// Enables syncing of Gemini threads across devices.
BASE_DECLARE_FEATURE(kSyncGeminiThread);

// Enables syncing of Encrypted Tab Context Containers across devices.
BASE_DECLARE_FEATURE(kSyncEncryptedTabContextContainer);

// Enables syncing of themes across iOS devices.
BASE_DECLARE_FEATURE(kSyncThemesIos);

// Enables syncing of New Tab Page customization themes on Android.
BASE_DECLARE_FEATURE(kNewTabPageCustomizationThemeSync);

// Enables syncing of usage metadata for loyalty cards.
BASE_DECLARE_FEATURE(kSyncLoyaltyCardMetadata);

// Enables syncing Notebooks.
BASE_DECLARE_FEATURE(kSyncNotebook);

// Enables syncing history journeys.
BASE_DECLARE_FEATURE(kSyncJourney);

// Enables syncing entity suppression records for Autofill AI.
BASE_DECLARE_FEATURE(kSyncAutofillEntitySuppression);

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)
// Flag that controls Uno fast-follow features which are:
// On Android:
// - Batch upload of left-behind bookmarks from the bookmark manager
// - Turn on bookmarks and reading list when signing in from bookmark manager
// - Confirmation dialog when turning off “Allow Chrome sign-in”
// - Promo for signed-in users with bookmarks toggle off
// On desktop:
// Adding history sync opt-in entry points, and other follow-ups to
// `kReplaceSyncPromosWithSignInPromos`.
BASE_DECLARE_FEATURE(kUnoPhase2FollowUp);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS)
// Do not use this flag directly. Use
// IsContactInfoDataTypeForCustomPassphraseUsersEnabled() instead.
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
#endif  // BUILDFLAG(IS_CHROMEOS)

// If enabled, the Contact Info data type will be enabled for users with custom
// passphrase.
bool IsContactInfoDataTypeForCustomPassphraseUsersEnabled();

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers);

// If enabled, keeps local and account search engines separate.
BASE_DECLARE_FEATURE(kSeparateLocalAndAccountSearchEngines);

// Feature flag to replace all sync-related UI with sign-in ones.
// Do not use this flag directly in production code. Use
// `syncer::IsReplaceSyncPromosWithSignInPromosEnabled()` instead.
BASE_DECLARE_FEATURE(kReplaceSyncPromosWithSignInPromos);

// Returns true if replacing sync promos with sign-in promos is enabled.
// Always true on desktop platforms (Linux, Mac, Windows), and controlled by
// `kReplaceSyncPromosWithSignInPromos` on other platforms. Prefer using this
// function over checking the feature flag directly.
bool IsReplaceSyncPromosWithSignInPromosEnabled();

// AutofillAi is launched globally on desktop, but restricted to certain
// countries on mobile.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#define DECLARE_SYNC_AUTOFILL_AI_FEATURE(feature_name) \
  BASE_DECLARE_FEATURE_WITH_COUNTRY_RESTRICTIONS(feature_name)
#else
#define DECLARE_SYNC_AUTOFILL_AI_FEATURE(feature_name) \
  BASE_DECLARE_FEATURE(feature_name)
#endif

// Enables syncing of settings from the user's account.
DECLARE_SYNC_AUTOFILL_AI_FEATURE(kSyncAccountSettings);

// Enables syncing of valuables from the user's account.
#if BUILDFLAG(IS_IOS)
DECLARE_SYNC_AUTOFILL_AI_FEATURE(kSyncAutofillValuable);
#endif

// Enables syncing of usage metadata from Google Wallet passes.
DECLARE_SYNC_AUTOFILL_AI_FEATURE(kSyncAutofillValuableMetadata);

// Enables syncing of flight reservations coming from Google Wallet.
DECLARE_SYNC_AUTOFILL_AI_FEATURE(kSyncWalletFlightReservations);

// Enables syncing of vehicle registrations coming from Google Wallet.
DECLARE_SYNC_AUTOFILL_AI_FEATURE(kSyncWalletVehicleRegistrations);

#undef DECLARE_SYNC_AUTOFILL_AI_FEATURE

// If enabled, the spellcheck custom dictionary will keep the account dictionary
// separate from the local dictionary.
// TODO(crbug.com/443954137): This feature doesn't yet do anything. Implement
// the local and account data separation behind this feature flag.
BASE_DECLARE_FEATURE(kSpellcheckSeparateLocalAndAccountDictionaries);

// Feature flag used for enabling sync (transport mode) for signed-in users that
// haven't turned on full sync.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn);
// Returns whether reading list storage related UI can be enabled, by testing
// `kReadingListEnableSyncTransportModeUponSignIn`.
bool IsReadingListAccountStorageEnabled();
#else
constexpr bool IsReadingListAccountStorageEnabled() {
  return true;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// If enabled, sync-the-transport will auto-start (avoid deferring startup) if
// sync metadata isn't available (i.e. initial sync never completed).
BASE_DECLARE_FEATURE(kSyncAlwaysForceImmediateStartIfTransportDataMissing);

// If enabled, distinguishes between local and account themes.
BASE_DECLARE_FEATURE(kSeparateLocalAndAccountThemes);

// If enabled, the local change nudge delays for single-client users are
// increased by some factor, specified via the FeatureParam below.
BASE_DECLARE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient);

inline constexpr base::FeatureParam<double>
    kSyncIncreaseNudgeDelayForSingleClientFactor{
        &kSyncIncreaseNudgeDelayForSingleClient,
        "SyncIncreaseNudgeDelayForSingleClientFactor", 2.0};

#if BUILDFLAG(IS_ANDROID)
// If enabled, the error message to unlock passwords is shown for longer.
BASE_DECLARE_FEATURE(kSyncTrustedVaultErrorMessageDuration);
#endif  // BUILDFLAG(IS_ANDROID)

// If enabled, use the access token propagated from SyncSchedulerImpl through
// SyncCycle rather than reading from ServerConnectionManager's cache.
BASE_DECLARE_FEATURE(kSyncUsePropagatedAccessToken);

#if BUILDFLAG(IS_ANDROID)
// If enabled, search engines and site search will be synced on Android LFF.
BASE_DECLARE_FEATURE(kSyncSearchEnginesAndroidLFF);

// If enabled, ignores the value set in sessions_invalidations_enabled_ and
// always registers for sessions invalidations.
BASE_DECLARE_FEATURE(kAlwaysRegisterSessionsInvalidationsAndroid);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Feature flag for ChromeOS only to estimate new sign-in users population.
BASE_DECLARE_FEATURE(kEstimateNewSignInUsersWithFinchAvailablePopulation);
#endif  // BUILDFLAG(IS_CHROMEOS)

// If enabled, disambiguates duplicate device display names using release
// channel labels (e.g., "(Canary)").
BASE_DECLARE_FEATURE(kSyncDisambiguateDeviceNamesWithChannel);

// Authenticate the Initialization Vector (IV) in Nigori encryption.
BASE_DECLARE_FEATURE(kSyncNigoriAuthenticateIV);

// If enabled, migrates users with deprecated IMPLICIT_PASSPHRASE to
// KEYSTORE_PASSPHRASE (if encrypt_everything is false) or
// CUSTOM_PASSPHRASE (if encrypt_everything is true).
BASE_DECLARE_FEATURE(kSyncMigrateFromImplicitPassphrase);

// When enabled, on-disk LoopbackServer state is migrated to use client tag
// hash and deterministic server IDs for bookmarks.
BASE_DECLARE_FEATURE(kSyncMigrateLoopbackServerBookmarksToClientTagHash);

#if !BUILDFLAG(IS_CHROMEOS)
// If enabled, prevents registration of controllers for APPS and APP_SETTINGS
// data types. Used as a kill switch during deprecation.
// TODO(crbug.com/40267153): Enable `kSyncWipeAppsAndAppSettingsData` when
// cleaning up this kill switch.
BASE_DECLARE_FEATURE(kSyncDoNotSyncAppsAndAppSettings);

// If enabled, wipes existing APPS and APP_SETTINGS data and metadata from
// DataTypeStore on startup.
// TODO(crbug.com/40267153): Enable by default when cleaning up
// `kSyncDoNotSyncAppsAndAppSettings`, and remove after enough time.
BASE_DECLARE_FEATURE(kSyncWipeAppsAndAppSettingsData);
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Enables the "Device tab visibility" subpage in Chrome Settings to choose
// which synced devices appear in cross-device tab lists.
BASE_DECLARE_FEATURE(kDeviceTabVisibilitySettings);

// Adds a top-level "Device tabs" entry to the desktop app menu that lists open
// tabs from the user's other devices, and removes them from the History
// submenu.
BASE_DECLARE_FEATURE(kCrossDeviceTabsInAppMenu);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// If enabled, tracks metrics about the user's storage of data on other devices.
BASE_DECLARE_FEATURE(kSyncRecordDeviceStatisticsMetrics);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
