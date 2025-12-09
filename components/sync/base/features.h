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

// Enables storing valuables in the profile db instead of the account db.
BASE_DECLARE_FEATURE(kSyncMoveValuablesToProfileDb);

// Enables syncing account-local metadata for shared tab groups.
BASE_DECLARE_FEATURE(kSyncSharedTabGroupAccountData);

// Enables syncing comments for shared contexts.
BASE_DECLARE_FEATURE(kSyncSharedComment);

// Enables syncing of AI threads across devices.
BASE_DECLARE_FEATURE(kSyncAIThread);

// Enables syncing of contextual tasks.
BASE_DECLARE_FEATURE(kSyncContextualTask);

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

// If enabled, the bookmarks count limit is controlled by a Finch parameter.
BASE_DECLARE_FEATURE(kSyncBookmarksLimit);

constexpr size_t kDefaultSyncBookmarksLimit = 100000;
inline constexpr base::FeatureParam<size_t> kSyncBookmarksLimitValue{
    &kSyncBookmarksLimit, "sync-bookmarks-limit-value",
    kDefaultSyncBookmarksLimit};
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
// Enables a message improvements to the existing trusted vault error infobar
// (informing users that fixing the error will help them to start syncing their
// passwords).
BASE_DECLARE_FEATURE(kSyncTrustedVaultInfobarMessageImprovements);
#endif  // BUILDFLAG(IS_IOS)

// When enabled, Sync will use OSCryptAsync for encryption/decryption instead
// of OSCrypt within the sync code.
BASE_DECLARE_FEATURE(kSyncUseOsCryptAsync);

BASE_DECLARE_FEATURE(kSyncDetermineAccountManagedStatus);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                           kSyncDetermineAccountManagedStatusTimeout);

// If enabled, the new sync dashboard URL will be opened when the user clicks
// on the "Review your synced data" (or equivalent) entrypoint in settings.
BASE_DECLARE_FEATURE(kSyncEnableNewSyncDashboardUrl);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
