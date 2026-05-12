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

// Enables syncing of settings from the user's account.
BASE_DECLARE_FEATURE(kSyncAccountSettings);

// Enables syncing of valuables from the user's account.
#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kSyncAutofillValuable);
#endif

// Enables syncing of usage metadata from Google Wallet passes.
BASE_DECLARE_FEATURE(kSyncAutofillValuableMetadata);

// Enables syncing account-local metadata for shared tab groups.
BASE_DECLARE_FEATURE(kSyncSharedTabGroupAccountData);

// Enables syncing comments for shared contexts.
BASE_DECLARE_FEATURE(kSyncSharedComment);

// Enables syncing of AI threads across devices.
BASE_DECLARE_FEATURE(kSyncAIThread);

// Enables syncing of contextual tasks.
BASE_DECLARE_FEATURE(kSyncContextualTask);

// Enables syncing of Gemini threads across devices.
BASE_DECLARE_FEATURE(kSyncGeminiThread);

// Enables syncing of themes across iOS devices.
BASE_DECLARE_FEATURE(kSyncThemesIos);

// Enables syncing of New Tab Page customization themes on Android.
BASE_DECLARE_FEATURE(kNewTabPageCustomizationThemeSync);

// Enables syncing of usage metadata for loyalty cards.
BASE_DECLARE_FEATURE(kSyncLoyaltyCardMetadata);

// Enables syncing of accessibility annotations to devices.
BASE_DECLARE_FEATURE(kSyncAccessibilityAnnotation);

#if !BUILDFLAG(IS_CHROMEOS)
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
#endif  // !BUILDFLAG(IS_CHROMEOS)

// If enabled, shows a user-actionable error when the bookmarks count limit is
// exceeded.
BASE_DECLARE_FEATURE(kSyncShowBookmarksLimitExceededError);

// Do not use this flag directly. Use
// IsContactInfoDataTypeForCustomPassphraseUsersEnabled() instead.
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);

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

// Feature flag to replace all sync-related UI with sign-in ones. This
// feature has the same behavior as kReplaceSyncPromosWithSignInPromos, but only
// enables extensions and bookmarks on new sign-ins.
BASE_DECLARE_FEATURE(kReplaceSyncPromosWithSigninPromosNewSignin);

// Returns true if the replace sync promos with sign-in promos feature is
// enabled. The launch may be controlled by multiple `base::Feature` flags,
// prefer using this function over checking the feature flags directly.
bool IsReplaceSyncPromosWithSignInPromosEnabled();

// If enabled, allowlisted priority preferences will be synced even if the
// preferences user toggle is off. Note that this flag is only meaningful if
// kEnablePreferencesAccountStorage is enabled.
BASE_DECLARE_FEATURE(kSyncSupportAlwaysSyncingPriorityPreferences);

// Enables syncing of flight reservations coming from Google Wallet.
BASE_DECLARE_FEATURE(kSyncWalletFlightReservations);

// Enables syncing of vehicle registrations coming from Google Wallet.
BASE_DECLARE_FEATURE(kSyncWalletVehicleRegistrations);

// If enabled, the spellcheck custom dictionary will keep the account dictionary
// separate from the local dictionary.
// TODO(crbug.com/443954137): This feature doesn't yet do anything. Implement
// the local and account data separation behind this feature flag.
BASE_DECLARE_FEATURE(kSpellcheckSeparateLocalAndAccountDictionaries);

// Normally, if kReplaceSyncPromosWithSignInPromos is disabled,
// UserSelectableType::kBookmarks is disabled by default upon sign-in. This
// flag makes the type enabled by default, for manual testing.
BASE_DECLARE_FEATURE(kEnableBookmarksSelectedTypeOnSigninForTesting);

// If enabled, avoids committing changes containing only favicon URL related
// change.
BASE_DECLARE_FEATURE(kSearchEngineAvoidFaviconOnlyCommits);

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

// If enabled, support displaying and uploading individual Reading List items in
// the Batch Upload UI.
//
// Batch Upload of all items is supported regardless of this feature flag.
//
// On Windows/Mac/Linux: this flag only affects behavior if the
// `syncer::kReadingListEnableSyncTransportModeUponSignIn` feature is also
// enabled.
//
// On Android: this flag does not affect user-visiable behavior, but does enable
// new code paths.
BASE_DECLARE_FEATURE(kSyncReadingListBatchUploadSelectedItems);

// If enabled, distinguishes between local and account themes.
BASE_DECLARE_FEATURE(kSeparateLocalAndAccountThemes);

// If enabled, offers batch upload of local themes upon sign in.
BASE_DECLARE_FEATURE(kThemesBatchUpload);

// If enabled, the local change nudge delays for single-client users are
// increased by some factor, specified via the FeatureParam below.
BASE_DECLARE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient);

inline constexpr base::FeatureParam<double>
    kSyncIncreaseNudgeDelayForSingleClientFactor{
        &kSyncIncreaseNudgeDelayForSingleClient,
        "SyncIncreaseNudgeDelayForSingleClientFactor", 2.0};

#if BUILDFLAG(IS_ANDROID)
// If enabled, WebAPK data will be synced for Backup&Restore purposes.
BASE_DECLARE_FEATURE(kWebApkBackupAndRestoreBackend);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// Flag to test different alternatives for the passwords sync error message
// content.
BASE_DECLARE_FEATURE(kSyncEnablePasswordsSyncErrorMessageAlternative);
inline constexpr base::FeatureParam<int>
    kSyncEnablePasswordsSyncErrorMessageAlternativeVersion{
        &kSyncEnablePasswordsSyncErrorMessageAlternative, "version", 3};

// If enabled, the error message to unlock passwords is shown for longer.
BASE_DECLARE_FEATURE(kSyncTrustedVaultErrorMessageDuration);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// Enables a message improvements to the existing trusted vault error infobar
// (informing users that fixing the error will help them to start syncing their
// passwords).
BASE_DECLARE_FEATURE(kSyncTrustedVaultInfobarMessageImprovements);
#endif  // BUILDFLAG(IS_IOS)

// If enabled, the preferences sync service will use the selected types to
// determine whether the pref values should be set in the account storage.
BASE_DECLARE_FEATURE(kSyncPreferencesUseSelectedTypes);

BASE_DECLARE_FEATURE(kSyncDetermineAccountManagedStatus);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSyncDetermineAccountManagedStatusTimeout);

// If enabled, the new sync dashboard URL will be opened when the user clicks
// on the "Review your synced data" (or equivalent) entrypoint in settings.
BASE_DECLARE_FEATURE(kSyncEnableNewSyncDashboardUrl);

// If enabled, Sync will fetch device statistics for all accounts on the device,
// and record summary metrics about them.
BASE_DECLARE_FEATURE(kSyncRecordDeviceStatisticsMetrics);
// Delay before downloading device statistics and recording related metrics. The
// exact number is somewhat arbitrary, chosen to ensure that refresh tokens are
// loaded, the local cache GUID is up to date, and to avoid interfering with
// general (sync or browser) startup.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSyncRecordDeviceStatisticsMetricsDelay);
// Controls how often device statistics are collected and recorded in metrics,
// as the minimum number of days between recordings.
BASE_DECLARE_FEATURE_PARAM(int, kSyncRecordDeviceStatisticsMetricsPeriodDays);

// If enabled, DeviceInfoSyncBridge uses WallClockTimer for pulse updates,
// which is more resilient to device suspension.
BASE_DECLARE_FEATURE(kSyncDeviceInfoUseWallClockTimer);

// If enabled, validate the access token before sending the request to the
// server.
BASE_DECLARE_FEATURE(kSyncValidateAccessToken);

// If enabled, Sync invalidations will bypass the scheduler on Android.
BASE_DECLARE_FEATURE(kSyncInvalidationsBypassScheduler);

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

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
