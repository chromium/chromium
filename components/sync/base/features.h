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

// Enables syncing of Loyalty Cards coming from Google Wallet.
BASE_DECLARE_FEATURE(kSyncAutofillLoyaltyCard);

// Makes the AUTOFILL_VALUABLE sync type non-encryptable.
BASE_DECLARE_FEATURE(kSyncMakeAutofillValuableNonEncryptable);

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

// Controls whether to enable syncing of Autofill Wallet Credential Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletCredentialData);

// If enabled, the error that the bookmarks count exceeded the limit during the
// last initial merge is reset after a certain period.
BASE_DECLARE_FEATURE(kSyncResetBookmarksInitialMergeLimitExceededError);

// If enabled, shows a user-actionable error when the bookmarks count limit is
// exceeded.
BASE_DECLARE_FEATURE(kSyncShowBookmarksLimitExceededError);

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers);

// If enabled, keeps local and account search engines separate.
BASE_DECLARE_FEATURE(kSeparateLocalAndAccountSearchEngines);

// Feature flag to replace all sync-related UI with sign-in ones.
BASE_DECLARE_FEATURE(kReplaceSyncPromosWithSignInPromos);

// Enables syncing extensions only if the user newly signs in to Chrome, not if
// they were already signed in by the time `kReplaceSyncPromosWithSignInPromos`
// was enabled.
BASE_DECLARE_FEATURE_PARAM(bool, kExplicitSigninForExtensions);

// Enables syncing bookmarks and reading list only if the user newly signs in to
// Chrome, not if they were already signed in by the time
// `kReplaceSyncPromosWithSignInPromos` was enabled.
BASE_DECLARE_FEATURE_PARAM(bool, kExplicitSigninForBookmarks);

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

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// If enabled, holds the account preference values under a dictionary in the
// main preferences file.
BASE_DECLARE_FEATURE(kMigrateAccountPrefs);
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

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

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
