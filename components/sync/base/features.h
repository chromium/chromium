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

// Enables syncing of Loyalty Cards coming from Google Wallet.
BASE_DECLARE_FEATURE(kSyncAutofillLoyaltyCard);

// Enables syncing account-local metadata for shared tab groups.
BASE_DECLARE_FEATURE(kSyncSharedTabGroupAccountData);

#if BUILDFLAG(IS_ANDROID)
// Flag that controls Uno fast-follow features which are:
// - Batch upload of left-behind bookmarks from the bookmark manager
// - Turn on bookmarks and reading list when signing in from bookmark manager
// - Confirmation dialog when turning off “Allow Chrome sign-in”
// - Promo for signed-in users with bookmarks toggle off
BASE_DECLARE_FEATURE(kUnoPhase2FollowUp);
#endif  // BUILDFLAG(IS_ANDROID)

// Controls whether to enable syncing of Autofill Wallet Credential Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletCredentialData);

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers);

// Wrapper flag to control the nudge delay of the #tab-groups-save feature.
BASE_DECLARE_FEATURE(kTabGroupsSaveNudgeDelay);

// If enabled, keeps local and account search engines separate.
BASE_DECLARE_FEATURE(kSeparateLocalAndAccountSearchEngines);

// If provided, changes the amount of time before we send messages to the sync
// service.
inline constexpr base::FeatureParam<base::TimeDelta>
    kTabGroupsSaveCustomNudgeDelay(&kTabGroupsSaveNudgeDelay,
                                   "TabGroupsSaveCustomNudgeDelay",
                                   base::Seconds(11));

// Feature flag to replace all sync-related UI with sign-in ones.
BASE_DECLARE_FEATURE(kReplaceSyncPromosWithSignInPromos);

// If enabled, allowlisted priority preferences will be synced even if the
// preferences user toggle is off. Note that this flag is only meaningful if
// kEnablePreferencesAccountStorage is enabled.
BASE_DECLARE_FEATURE(kSyncSupportAlwaysSyncingPriorityPreferences);

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

// Flags to allow AUTOFILL_WALLET_METADATA and AUTOFILL_WALLET_OFFER,
// respectively, to run in transport mode.
BASE_DECLARE_FEATURE(kSyncEnableWalletMetadataInTransportMode);
BASE_DECLARE_FEATURE(kSyncEnableWalletOfferInTransportMode);

// Flag to enable clean up of password deletions that may be unintentional.
BASE_DECLARE_FEATURE(kSyncPasswordCleanUpAccidentalBatchDeletions);
// The minimum number of deletions that can be considered a batch deletion.
inline constexpr base::FeatureParam<int>
    kSyncPasswordCleanUpAccidentalBatchDeletionsCountThreshold{
        &kSyncPasswordCleanUpAccidentalBatchDeletions,
        "SyncPasswordCleanUpAccidentalBatchDeletionsCountThreshold", 3};
// The maximum time between earliest and latest deletion to be considered an
// accidental batch deletion.
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncPasswordCleanUpAccidentalBatchDeletionsTimeThreshold{
        &kSyncPasswordCleanUpAccidentalBatchDeletions,
        "SyncPasswordCleanUpAccidentalBatchDeletionsTimeThreshold",
        base::Milliseconds(100)};

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
        &kSyncEnablePasswordsSyncErrorMessageAlternative, "version", 1};
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// Enables a set of improvements to the existing trusted vault error infobar on
// iOS (displaying it on pages with password forms, adjusting display time,
// adding dismiss conditions, adding a notification pause after dismissal).
BASE_DECLARE_FEATURE(kSyncTrustedVaultInfobarImprovements);
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
// Enables a message improvements to the existing trusted vault error infobar
// (informing users that fixing the error will help them to start syncing their
// passwords).
BASE_DECLARE_FEATURE(kSyncTrustedVaultInfobarMessageImprovements);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
