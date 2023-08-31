// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_FEATURES_H_
#define COMPONENTS_SYNC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace syncer {

// Customizes the delay of a deferred sync startup.
BASE_DECLARE_FEATURE(kDeferredSyncStartupCustomDelay);
inline constexpr base::FeatureParam<int>
    kDeferredSyncStartupCustomDelayInSeconds{
        &kDeferredSyncStartupCustomDelay,
        "DeferredSyncStartupCustomDelayInSeconds", 1};

// Causes Sync to ignore updates encrypted with keys that have been missing for
// too long from this client; Sync will proceed normally as if those updates
// didn't exist.
BASE_DECLARE_FEATURE(kIgnoreSyncEncryptionKeysLongMissing);

// The threshold for kIgnoreSyncEncryptionKeysLongMissing to start ignoring keys
// (measured in number of GetUpdatesResponses messages).
inline constexpr base::FeatureParam<int> kMinGuResponsesToIgnoreKey{
    &kIgnoreSyncEncryptionKeysLongMissing, "MinGuResponsesToIgnoreKey", 3};

// Enables adding, displaying and modifying extra notes to stored credentials.
// When enabled, "PasswordViewPageInSettings" feature in the password manager
// codebase is ignored and the new password view subpage is force enabled. When
// enabled, Sync machinery will read and writes password notes to the
// `encrypted_notes_backup` field inside the PasswordSpecifics proto. Together
// with the logic on the server. this protects against notes being overwritten
// by legacy clients not supporting password notes.
// This feature is added here instead of the password manager codebase to avoid
// cycle dependencies.
// This feature is used in Credential Provider Extension on iOS. Keep the
// default value in sync with the default value in
// ios/chrome/credential_provider_extension/ui/feature_flags.mm.
BASE_DECLARE_FEATURE(kPasswordNotesWithBackup);
// Decides how long the user does not require reuathentication after
// successfully authenticated.
inline constexpr base::FeatureParam<base::TimeDelta> kPasswordNotesAuthValidity{
    &kPasswordNotesWithBackup, "authentication_validity_duration",
    base::Minutes(5)};

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

// If enabled, Segmentation data type will be synced.
BASE_DECLARE_FEATURE(kSyncSegmentationDataType);

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
// TODO(crbug/1365292): Add more information about this feature after
// upload/download invalidations support from ModelTypeState msg will be added.
BASE_DECLARE_FEATURE(kSyncPersistInvalidations);

// When enabled, optimization flags (single client and a list of FCM
// registration tokens) will be disabled if during the current sync cycle
// DeviceInfo has been updated.
BASE_DECLARE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated);

// If enabled, the HISTORY data type replaces TYPED_URLS.
BASE_DECLARE_FEATURE(kSyncEnableHistoryDataType);
inline constexpr base::FeatureParam<int>
    kSyncHistoryForeignVisitsToDeletePerBatch{
        &kSyncEnableHistoryDataType, "foreign_visit_deletions_per_batch", 100};

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeInTransportMode);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers);
inline constexpr base::FeatureParam<bool>
    kSyncEnableContactInfoDataTypeForDasherGoogleUsers{
        &kSyncEnableContactInfoDataTypeForDasherUsers,
        "enable_for_google_accounts", false};

// If enabled, issues error and disables bookmarks sync when limit is crossed.
BASE_DECLARE_FEATURE(kSyncEnforceBookmarksCountLimit);

// Enables a separate account-scoped storage for preferences, for syncing users.
// (Note that opposed to other "account storage" features, this one does not
// have any effect for signed-in non-syncing users!)
BASE_DECLARE_FEATURE(kEnablePreferencesAccountStorage);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Influences how precisely SyncServiceImpl determines whether Sync-the-feature
// is enabled. If the feature is on, the new approach is used, which leans on
// the state reported by IdentityManager. If false, the legacy approach is used,
// which is based on preference prefs::kSyncRequested.
// TODO(crbug.com/1219990): Remove this.
BASE_DECLARE_FEATURE(kSyncIgnoreSyncRequestedPreference);
#endif  // BUILDFLAG(!IS_CHROMEOS_ASH)

// If enabled, Sync will send a poll GetUpdates request on every browser
// startup. This is a temporary hack; see crbug.com/1425026.
// TODO(crbug.com/1425071): Remove this.
BASE_DECLARE_FEATURE(kSyncPollImmediatelyOnEveryStartup);

// If enabled, and a poll GetUpdates request is scheduled on browser startup,
// there won't be an additional delay.
BASE_DECLARE_FEATURE(kSyncPollWithoutDelayOnStartup);

#if !BUILDFLAG(IS_ANDROID) || !BUILDFLAG(IS_IOS)
// Enables syncing the WEBAUTHN_CREDENTIAL data type.
BASE_DECLARE_FEATURE(kSyncWebauthnCredentials);
#endif  // !BUILDFLAG(IS_ANDROID) || !BUILDFLAG(IS_IOS)

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

// If enabled, there will be two different BookmarkModel instances per profile:
// one instance for "profile" bookmarks and another instance for "account"
// bookmarks. See https://crbug.com/1404250 for details.
BASE_DECLARE_FEATURE(kEnableBookmarksAccountStorage);

// Feature flag that controls a technical rollout of a new codepath that doesn't
// itself cause user-facing changes but sets the foundation for later rollouts
// namely, `kReadingListEnableSyncTransportModeUponSignIn` below).
BASE_DECLARE_FEATURE(kReadingListEnableDualReadingListModel);

// Feature flag used for enabling sync (transport mode) for signed-in users that
// haven't turned on full sync.
BASE_DECLARE_FEATURE(kReadingListEnableSyncTransportModeUponSignIn);

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

// Flag to enable the option to batch upload local data from the new account
// settings panel.
BASE_DECLARE_FEATURE(kSyncEnableBatchUploadLocalData);
BASE_DECLARE_FEATURE(kSyncEnableBatchUploadLocalDataWithDummyDataForTesting);
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncResponseDelayForBatchUploadLocalDataWithDummyDataForTesting{
        &kSyncEnableBatchUploadLocalDataWithDummyDataForTesting,
        "SyncResponseDelayForBatchUploadLocalDataWithDummyDataForTesting",
        base::Seconds(1)};

#if BUILDFLAG(IS_ANDROID)
// Feature flag for enabling the restoration of synced placeholder tabs missing
// on the local session, which typically happens only on Android only.
BASE_DECLARE_FEATURE(kRestoreSyncedPlaceholderTabs);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
