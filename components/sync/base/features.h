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

// Controls whether to enable bootstrapping Public-private keys in Nigori
// key-bag.
BASE_DECLARE_FEATURE(kSharingOfferKeyPairBootstrap);

// Kill switch to read sharing-offer related keys.
BASE_DECLARE_FEATURE(kSharingOfferKeyPairRead);

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kSyncAndroidLimitNTPPromoImpressions);
inline constexpr base::FeatureParam<int> kSyncAndroidNTPPromoMaxImpressions{
    &kSyncAndroidLimitNTPPromoImpressions, "SyncAndroidNTPPromoMaxImpressions",
    5};
#endif  // BUILDFLAG(IS_ANDROID)

// Controls whether to enable syncing of Autofill Wallet Usage Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletUsageData);

// Controls whether to enable syncing of Autofill Wallet Credential Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletCredentialData);

// Controls if the `PlusAddressSyncBridge`, controlling PLUS_ADDRESS should be
// instantiated.
// TODO(b/322147254): Cleanup when launched.
BASE_DECLARE_FEATURE(kSyncPlusAddress);

#if BUILDFLAG(IS_CHROMEOS)
// Whether explicit passphrase sharing between Ash and Lacros is enabled.
BASE_DECLARE_FEATURE(kSyncChromeOSExplicitPassphraseSharing);

// Whether Apps toggle value is exposed by Ash to Lacros.
BASE_DECLARE_FEATURE(kSyncChromeOSAppsToggleSharing);

// Whether SyncedSessions are updated by Lacros to Ash.
BASE_DECLARE_FEATURE(kChromeOSSyncedSessionSharing);
#endif  // BUILDFLAG(IS_CHROMEOS)

// If enabled, all incoming invalidations will be stored in ModelTypeState
// proto message.
// TODO(crbug.com/40239360): Add more information about this feature after
// upload/download invalidations support from ModelTypeState msg will be added.
BASE_DECLARE_FEATURE(kSyncPersistInvalidations);

// When enabled, optimization flags (single client and a list of FCM
// registration tokens) will be disabled if during the current sync cycle
// DeviceInfo has been updated.
BASE_DECLARE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated);

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeInTransportMode);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForChildUsers);

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

// If enabled, Sync will send a poll GetUpdates request on every browser
// startup. This is a temporary hack; see crbug.com/1425026.
// TODO(crbug.com/40260698): Remove this.
BASE_DECLARE_FEATURE(kSyncPollImmediatelyOnEveryStartup);

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
BASE_DECLARE_FEATURE(kEnableBookmarkFoldersForAccountStorage);

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

// If enabled, triggers a synchronisation when WebContentsObserver's
// -OnVisibilityChanged method is called.
BASE_DECLARE_FEATURE(kSyncSessionOnVisibilityChanged);

// The minimum time between two sync updates of last_active_time when the tab
// hasn't changed.
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncSessionOnVisibilityChangedTimeThreshold{
        &kSyncSessionOnVisibilityChanged,
        "SyncSessionOnVisibilityChangedTimeThreshold", base::Minutes(10)};

// If enabled, sync-the-transport will auto-start (avoid deferring startup) if
// sync metadata isn't available (i.e. initial sync never completed).
BASE_DECLARE_FEATURE(kSyncAlwaysForceImmediateStartIfTransportDataMissing);

// If enabled, the local change nudge delays for single-client users are
// increased by some factor, specified via the FeatureParam below.
BASE_DECLARE_FEATURE(kSyncIncreaseNudgeDelayForSingleClient);

inline constexpr base::FeatureParam<double>
    kSyncIncreaseNudgeDelayForSingleClientFactor{
        &kSyncIncreaseNudgeDelayForSingleClient,
        "SyncIncreaseNudgeDelayForSingleClientFactor", 2.0};

// If enabled, SyncSchedulerImpl uses a WallClockTimer instead of a OneShotTimer
// to schedule poll requests.
BASE_DECLARE_FEATURE(kSyncSchedulerUseWallClockTimer);

#if BUILDFLAG(IS_ANDROID)
// If enabled, WebAPK data will be synced for Backup&Restore purposes.
BASE_DECLARE_FEATURE(kWebApkBackupAndRestoreBackend);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
