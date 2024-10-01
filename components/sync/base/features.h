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

#if BUILDFLAG(IS_ANDROID)
// Controls whether to show a batch upload card in Android unified settings
// panel.
BASE_DECLARE_FEATURE(kEnableBatchUploadFromSettings);

// Flag that controls Uno fast-follow features which are:
// - Batch upload of left-behind bookmarks from the bookmark manager
// - Turn on bookmarks and reading list when signing in from bookmark manager
// - Confirmation dialog when turning off “Allow Chrome sign-in”
// - Promo for signed-in users with bookmarks toggle off
BASE_DECLARE_FEATURE(kUnoPhase2FollowUp);
#endif  // BUILDFLAG(IS_ANDROID)

// Controls whether to enable syncing of Autofill Wallet Usage Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletUsageData);

// Controls whether to enable syncing of Autofill Wallet Credential Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletCredentialData);

// Controls if the `PlusAddressSettingSyncBridge`, controlling
// PLUS_ADDRESS_SETTING should be instantiated.
// TODO(b/342089839): Cleanup when launched.
BASE_DECLARE_FEATURE(kSyncPlusAddressSetting);

#if BUILDFLAG(IS_CHROMEOS)
// Whether explicit passphrase sharing between Ash and Lacros is enabled.
BASE_DECLARE_FEATURE(kSyncChromeOSExplicitPassphraseSharing);

// Whether Apps toggle value is exposed by Ash to Lacros.
BASE_DECLARE_FEATURE(kSyncChromeOSAppsToggleSharing);

// Whether SyncedSessions are updated by Lacros to Ash.
BASE_DECLARE_FEATURE(kChromeOSSyncedSessionSharing);
#endif  // BUILDFLAG(IS_CHROMEOS)

// When enabled, optimization flags (single client and a list of FCM
// registration tokens) will be disabled if during the current sync cycle
// DeviceInfo has been updated.
BASE_DECLARE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated);

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeInTransportMode);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers);

// For users who support separate "profile" and "account" password stores -
// see password_manager::features_util::CanCreateAccountStore() - and have
// sync-the-feature on, enabling this flag means:
// - New passwords are saved to the account store if the passwords data type is
//   "selected", and to the profile store otherwise. When the flag is disabled,
//   saves always happen to the profile store.
// - The account store is synced. When the flag is disabled, the profile one is.
BASE_DECLARE_FEATURE(kEnablePasswordsAccountStorageForSyncingUsers);
// For users who support separate "profile" and "account" password stores -
// see password_manager::features_util::CanCreateAccountStore() - and have
// sync-the-transport on, enabling this flag means:
// - New passwords are saved to the account store if the passwords data type is
//   "selected", and to the profile store otherwise. When the flag is disabled,
//   saves always happen to the profile store.
// - The account store is synced. When the flag is disabled, no store is.
BASE_DECLARE_FEATURE(kEnablePasswordsAccountStorageForNonSyncingUsers);

// Enables a separate account-scoped storage for preferences, for syncing users.
// (Note that opposed to other "account storage" features, this one does not
// have any effect for signed-in non-syncing users!)
BASE_DECLARE_FEATURE(kEnablePreferencesAccountStorage);

#if !BUILDFLAG(IS_ANDROID)
// Enables syncing the WEBAUTHN_CREDENTIAL data type.
// Enabled by default on M123. Remove on or after M126 on all platforms,
// except on iOS, where it has not been enabled by default yet.
BASE_DECLARE_FEATURE(kSyncWebauthnCredentials);
#endif  // !BUILDFLAG(IS_ANDROID)

// If enabled, ignore GetUpdates retry delay command from the server.
BASE_DECLARE_FEATURE(kSyncIgnoreGetUpdatesRetryDelay);

// Wrapper flag to control the nudge delay of the #tab-groups-save feature.
BASE_DECLARE_FEATURE(kTabGroupsSaveNudgeDelay);

// If provided, changes the amount of time before we send messages to the sync
// service.
inline constexpr base::FeatureParam<base::TimeDelta>
    kTabGroupsSaveCustomNudgeDelay(&kTabGroupsSaveNudgeDelay,
                                   "TabGroupsSaveCustomNudgeDelay",
                                   base::Seconds(11));

// Feature flag to replace all sync-related UI with sign-in ones.
BASE_DECLARE_FEATURE(kReplaceSyncPromosWithSignInPromos);

// This gates the new single-model approach where account bookmarks are stored
// in separate permanent folders in BookmarkModel. The flag has to be in the
// sync namespace as it controls whether BOOKMARKS datatype is enabled in the
// transport mode.
// TODO(crbug.com/40943550): Remove this.
BASE_DECLARE_FEATURE(kSyncEnableBookmarksInTransportMode);

// Normally, if kReplaceSyncPromosWithSignInPromos is disabled,
// UserSelectableType::kBookmarks is disabled by default upon sign-in. This
// flag makes the type enabled by default, for manual testing.
BASE_DECLARE_FEATURE(kEnableBookmarksSelectedTypeOnSigninForTesting);

// Feature flag used for enabling sync (transport mode) for signed-in users that
// haven't turned on full sync.
#if !BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn);
// Returns whether reading list storage related UI can be enabled, by testing
// `kReadingListEnableSyncTransportModeUponSignIn`.
bool IsReadingListAccountStorageEnabled();
#else
constexpr bool IsReadingListAccountStorageEnabled() {
  return true;
}
#endif  // !BUILDFLAG(IS_IOS)

// Flag to allow SHARED_TAB_GROUP_DATA to run in transport mode.
BASE_DECLARE_FEATURE(kSyncSharedTabGroupDataInTransportMode);

// Flags to allow AUTOFILL_WALLET_METADATA and AUTOFILL_WALLET_OFFER,
// respectively, to run in transport mode.
BASE_DECLARE_FEATURE(kSyncEnableWalletMetadataInTransportMode);
BASE_DECLARE_FEATURE(kSyncEnableWalletOfferInTransportMode);

// Flag to enable setting `deleted_by_version` on a `EntityMetadata`.
BASE_DECLARE_FEATURE(kSyncEntityMetadataRecordDeletedByVersionOnLocalDeletion);

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

// If enabled, distinguishes between local and account themes.
BASE_DECLARE_FEATURE(kSeparateLocalAndAccountThemes);

// If enabled, the local change nudge delays for single-client users are
// increased by some factor, specified via the FeatureParam below.
BASE_DECLARE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient);

inline constexpr base::FeatureParam<double>
    kSyncIncreaseNudgeDelayForSingleClientFactor{
        &kSyncIncreaseNudgeDelayForSingleClient,
        "SyncIncreaseNudgeDelayForSingleClientFactor", 2.0};

// Guards the registration of synthetic field trials based on information in
// Nigori's TrustedVaultDebugInfo.
BASE_DECLARE_FEATURE(kTrustedVaultAutoUpgradeSyntheticFieldTrial);

// If enabled, uses new fields ThemeSpecifics to replace theme prefs, thus
// avoiding use of preferences to sync themes.
BASE_DECLARE_FEATURE(kMoveThemePrefsToSpecifics);

#if BUILDFLAG(IS_ANDROID)
// If enabled, WebAPK data will be synced for Backup&Restore purposes.
BASE_DECLARE_FEATURE(kWebApkBackupAndRestoreBackend);
#endif  // BUILDFLAG(IS_ANDROID)

// Kill switch for a change in the internal implementation of
// SyncService::GetLocalDataDescriptions() and TriggerLocalDataMigration(),
// which is expected to be a no-op.
BASE_DECLARE_FEATURE(kSyncEnableModelTypeLocalDataBatchUploaders);

// Enables syncing for extensions when in transport mode (when a user is signed
// in but has not turned on full sync).
BASE_DECLARE_FEATURE(kSyncEnableExtensionsInTransportMode);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
