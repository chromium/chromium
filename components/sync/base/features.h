// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_FEATURES_H_
#define COMPONENTS_SYNC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace syncer {

// If enabled, EntitySpecifics will be cached in EntityMetadata in order to
// prevent data loss caused by older clients dealing with unknown proto fields
// (introduced later).
BASE_DECLARE_FEATURE(kCacheBaseEntitySpecificsInMetadata);

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

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kSyncAndroidLimitNTPPromoImpressions);
inline constexpr base::FeatureParam<int> kSyncAndroidNTPPromoMaxImpressions{
    &kSyncAndroidLimitNTPPromoImpressions, "SyncAndroidNTPPromoMaxImpressions",
    5};
#endif  // BUILDFLAG(IS_ANDROID)

// Controls whether to enable syncing of Autofill Wallet Usage Data.
BASE_DECLARE_FEATURE(kSyncAutofillWalletUsageData);

// Causes the sync engine to count a quota for commits of data types that can
// be committed by extension JS API. If the quota is depleted, an extra long
// nudge delay is applied to that data type. As a result, more changes are
// likely to get combined into one commit message.
BASE_DECLARE_FEATURE(kSyncExtensionTypesThrottling);

// TODO(crbug.com/1425065): Remove this.
BASE_DECLARE_FEATURE(kSyncResetPollIntervalOnStart);

// If enabled, Segmentation data type will be synced.
BASE_DECLARE_FEATURE(kSyncSegmentationDataType);

#if BUILDFLAG(IS_CHROMEOS)
// Whether warning should be shown in sync settings page when lacros
// side-by-side mode is enabled.
BASE_DECLARE_FEATURE(kSyncSettingsShowLacrosSideBySideWarning);

// Whether explicit passphrase sharing between Ash and Lacros is enabled.
BASE_DECLARE_FEATURE(kSyncChromeOSExplicitPassphraseSharing);

// Whether Apps toggle value is exposed by Ash to Lacros.
BASE_DECLARE_FEATURE(kSyncChromeOSAppsToggleSharing);

// Whether SyncedSessions are updated by Lacros to Ash.
BASE_DECLARE_FEATURE(kChromeOSSyncedSessionSharing);
#endif  // BUILDFLAG(IS_CHROMEOS)

// Whether the periodic degraded recoverability polling is enabled.
BASE_DECLARE_FEATURE(kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling);
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling{
        &kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
        "kSyncTrustedVaultLongPeriodDegradedRecoverabilityPolling",
        base::Days(7)};
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling{
        &kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling,
        "kSyncTrustedVaultShortPeriodDegradedRecoverabilityPolling",
        base::Hours(1)};

// Enables logging a UMA metric that requires first communicating with the
// trusted vault server, in order to verify that the local notion of the device
// being registered is consistent with the server-side state.
BASE_DECLARE_FEATURE(kSyncTrustedVaultVerifyDeviceRegistration);

// If enabled, the device will register with FCM and listen to new
// invalidations. Also, FCM token will be set in DeviceInfo, which signals to
// the server that device listens to new invalidations.
// The device will not subscribe to old invalidations for any data types except
// Wallet and Offer, since that will be covered by the new system.
// SyncSendInterestedDataTypes must be enabled for this to take effect.
BASE_DECLARE_FEATURE(kUseSyncInvalidations);

// If enabled, all incoming invalidations will be stored in ModelTypeState
// proto message.
// TODO(crbug/1365292): Add more information about this feature after
// upload/download invalidations support from ModelTypeState msg will be added.
BASE_DECLARE_FEATURE(kSyncPersistInvalidations);

// If enabled, types related to Wallet and Offer will be included in interested
// data types, and the device will listen to new invalidations for those types
// (if they are enabled).
// The device will not register for old invalidations at all.
// UseSyncInvalidations must be enabled for this to take effect.
BASE_DECLARE_FEATURE(kUseSyncInvalidationsForWalletAndOffer);

// When enabled, optimization flags (single client and a list of FCM
// registration tokens) will be disabled if during the current sync cycle
// DeviceInfo has been updated.
BASE_DECLARE_FEATURE(kSkipInvalidationOptimizationsWhenDeviceInfoUpdated);

// If enabled, the HISTORY data type replaces TYPED_URLS.
BASE_DECLARE_FEATURE(kSyncEnableHistoryDataType);
inline constexpr base::FeatureParam<int>
    kSyncHistoryForeignVisitsToDeletePerBatch{
        &kSyncEnableHistoryDataType, "foreign_visit_deletions_per_batch", 100};

BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataType);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeEarlyReturnNoDatabase);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeInTransportMode);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForCustomPassphraseUsers);
BASE_DECLARE_FEATURE(kSyncEnableContactInfoDataTypeForDasherUsers);
inline constexpr base::FeatureParam<bool>
    kSyncEnableContactInfoDataTypeForDasherGoogleUsers{
        &kSyncEnableContactInfoDataTypeForDasherUsers,
        "enable_for_google_accounts", false};

// If enabled, issues error and disables bookmarks sync when limit is crossed.
BASE_DECLARE_FEATURE(kSyncEnforceBookmarksCountLimit);

// Enables codepath to allow clearing metadata when the data type is stopped.
BASE_DECLARE_FEATURE(kSyncAllowClearingMetadataWhenDataTypeIsStopped);

// Enabled by default, this acts as a kill switch for a timeout introduced over
// loading of models for enabled types in ModelLoadManager. When enabled, it
// skips waiting for types not loaded yet and tries to stop them once they
// finish loading.
BASE_DECLARE_FEATURE(kSyncEnableLoadModelsTimeout);

// Timeout duration for loading data types in ModelLoadManager.
// TODO(crbug.com/992340): Update the timeout duration based on uma metrics
// Sync.ModelLoadManager.LoadModelsElapsedTime
inline constexpr base::FeatureParam<base::TimeDelta>
    kSyncLoadModelsTimeoutDuration{&kSyncEnableLoadModelsTimeout,
                                   "sync_load_models_timeout_duration",
                                   base::Seconds(30)};

// Enable check to ensure only preferences in the allowlist are registered as
// syncable.
BASE_DECLARE_FEATURE(kSyncEnforcePreferencesAllowlist);

// Enables a separate account-scoped storage for preferences, for syncing users.
// (Note that opposed to other "account storage" features, this one does not
// have any effect for signed-in non-syncing users!)
BASE_DECLARE_FEATURE(kEnablePreferencesAccountStorage);

// If enabled, Sync will send a poll GetUpdates request on every browser
// startup. This is a temporary hack; see crbug.com/1425026.
// TODO(crbug.com/1425071): Remove this.
BASE_DECLARE_FEATURE(kSyncPollImmediatelyOnEveryStartup);

#if BUILDFLAG(IS_IOS)
// Feature flag to enable indicating the Account Storage error in the Account
// Cell when Sync is turned OFF (iOS only).
BASE_DECLARE_FEATURE(kIndicateAccountStorageErrorInAccountCell);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
