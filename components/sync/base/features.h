// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_FEATURES_H_
#define COMPONENTS_SYNC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace syncer {

// Allows device registration within trusted vault server without having trusted
// vault key. Effectively disabled if kSyncTrustedVaultPassphraseRecovery
// is disabled.
inline constexpr base::Feature kAllowSilentTrustedVaultDeviceRegistration{
    "AllowSilentTrustedVaultDeviceRegistration",
    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, EntitySpecifics will be cached in EntityMetadata in order to
// prevent data loss caused by older clients dealing with unknown proto fields
// (introduced later).
inline constexpr base::Feature kCacheBaseEntitySpecificsInMetadata{
    "CacheBaseEntitySpecificsInMetadata", base::FEATURE_DISABLED_BY_DEFAULT};

inline constexpr base::Feature kEnableSyncImmediatelyInFRE{
    "EnableSyncImmediatelyInFRE", base::FEATURE_ENABLED_BY_DEFAULT};

// Causes Sync to ignore updates encrypted with keys that have been missing for
// too long from this client; Sync will proceed normally as if those updates
// didn't exist.
inline constexpr base::Feature kIgnoreSyncEncryptionKeysLongMissing{
    "IgnoreSyncEncryptionKeysLongMissing", base::FEATURE_DISABLED_BY_DEFAULT};
// The threshold for kIgnoreSyncEncryptionKeysLongMissing to start ignoring keys
// (measured in number of GetUpdatesResponses messages).
inline constexpr base::FeatureParam<int> kMinGuResponsesToIgnoreKey{
    &kIgnoreSyncEncryptionKeysLongMissing, "MinGuResponsesToIgnoreKey", 3};

// When enabled, Sync machinery will read and writes password notes to the
// `encrypted_notes_backup` field inside the PasswordSpecifics proto. Together
// with the logic on the server. this protects against notes being overwritten
// by legacy clients not supporting password notes.
inline constexpr base::Feature kReadWritePasswordNotesBackupField{
    "ReadWritePasswordNotesBackupField",base::FEATURE_DISABLED_BY_DEFAULT};

// Allows custom passphrase users to receive Wallet data for secondary accounts
// while in transport-only mode.
inline constexpr base::Feature
    kSyncAllowWalletDataInTransportModeWithCustomPassphrase{
        "SyncAllowAutofillWalletDataInTransportModeWithCustomPassphrase",
        base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
inline constexpr base::Feature kSyncAndroidLimitNTPPromoImpressions{
    "SyncAndroidLimitNTPPromoImpressions", base::FEATURE_DISABLED_BY_DEFAULT};
inline constexpr base::FeatureParam<int> kSyncAndroidNTPPromoMaxImpressions{
    &kSyncAndroidLimitNTPPromoImpressions, "SyncAndroidNTPPromoMaxImpressions",
    5};
inline constexpr base::Feature kSyncAndroidPromosWithAlternativeTitle{
    "SyncAndroidPromosWithAlternativeTitle", base::FEATURE_DISABLED_BY_DEFAULT};
inline constexpr base::Feature kSyncAndroidPromosWithIllustration{
    "SyncAndroidPromosWithIllustration", base::FEATURE_DISABLED_BY_DEFAULT};
inline constexpr base::Feature kSyncAndroidPromosWithSingleButton{
    "SyncAndroidPromosWithSingleButton", base::FEATURE_DISABLED_BY_DEFAULT};
inline constexpr base::Feature kSyncAndroidPromosWithTitle{
    "SyncAndroidPromosWithTitle", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_ANDROID)

// Controls whether to enable syncing of Autofill Wallet Usage Data.
inline constexpr base::Feature kSyncAutofillWalletUsageData{
    "SyncAutofillWalletUsageData", base::FEATURE_DISABLED_BY_DEFAULT};

// Causes the sync engine to count a quota for commits of data types that can
// be committed by extension JS API. If the quota is depleted, an extra long
// nudge delay is applied to that data type. As a result, more changes are
// likely to get combined into one commit message.
inline constexpr base::Feature kSyncExtensionTypesThrottling{
    "SyncExtensionTypesThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

// Sync requires policies to be loaded before starting.
inline constexpr base::Feature kSyncRequiresPoliciesLoaded{
    "SyncRequiresPoliciesLoaded", base::FEATURE_DISABLED_BY_DEFAULT};
// Max time to delay the sync startup while waiting for policies to load.
inline constexpr base::FeatureParam<base::TimeDelta> kSyncPolicyLoadTimeout{
    &kSyncRequiresPoliciesLoaded, "SyncPolicyLoadTimeout", base::Seconds(10)};

inline constexpr base::Feature kSyncResetPollIntervalOnStart{
    "SyncResetPollIntervalOnStart", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, interested data types, excluding Wallet and Offer, will be sent
// to the Sync Server as part of DeviceInfo.
inline constexpr base::Feature kSyncSendInterestedDataTypes = {
    "SyncSendInterestedDataTypes", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS)
// Whether warning should be shown in sync settings page when lacros
// side-by-side mode is enabled.
inline constexpr base::Feature kSyncSettingsShowLacrosSideBySideWarning{
    "SyncSettingsShowLacrosSideBySideWarning",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Whether explicit passphrase sharing between Ash and Lacros is enabled.
inline constexpr base::Feature kSyncChromeOSExplicitPassphraseSharing{
    "SyncChromeOSExplicitPassphraseSharing", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether Apps toggle value is exposed by Ash to Lacros.
inline constexpr base::Feature kSyncChromeOSAppsToggleSharing{
    "SyncChromeOSAppsToggleSharing", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS)

// Whether the periodic degraded recoverability polling is enabled.
inline constexpr base::Feature
    kSyncTrustedVaultPeriodicDegradedRecoverabilityPolling{
        "SyncTrustedVaultDegradedRecoverabilityHandler",
        base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_IOS)
// Whether RPC is enabled.
inline constexpr base::Feature kSyncTrustedVaultPassphraseiOSRPC{
    "SyncTrustedVaultPassphraseiOSRPC", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_IOS)

// Whether the entry point to opt in to trusted vault in settings should be
// shown.
inline constexpr base::Feature kSyncTrustedVaultPassphrasePromo{
    "SyncTrustedVaultPassphrasePromo", base::FEATURE_DISABLED_BY_DEFAULT};

// Keep this entry in sync with the equivalent name in
// ChromeFeatureList.java.
inline constexpr base::Feature kSyncTrustedVaultPassphraseRecovery{
    "SyncTrustedVaultPassphraseRecovery", base::FEATURE_ENABLED_BY_DEFAULT};
// Specifies how long requests to vault service shouldn't be retried after
// encountering transient error.
inline constexpr base::FeatureParam<base::TimeDelta>
    kTrustedVaultServiceThrottlingDuration{
        &kSyncTrustedVaultPassphraseRecovery,
        "TrustedVaultServiceThrottlingDuration", base::Days(1)};

// Enables logging a UMA metric that requires first communicating with the
// trusted vault server, in order to verify that the local notion of the device
// being registered is consistent with the server-side state.
inline constexpr base::Feature kSyncTrustedVaultVerifyDeviceRegistration{
    "SyncTrustedVaultVerifyDeviceRegistration",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Triggers another device registration attempt if the device was registered
// before this feature was introduced.
inline constexpr base::Feature kSyncTrustedVaultRedoDeviceRegistration{
    "SyncTrustedVaultRedoDeviceRegistration", base::FEATURE_ENABLED_BY_DEFAULT};

// Triggers one-off reset of `keys_are_stale`, allowing another device
// registration attempt if previous was failed.
inline constexpr base::Feature kSyncTrustedVaultResetKeysAreStale{
    "SyncTrustedVaultResetKeysAreStale", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the device will register with FCM and listen to new
// invalidations. Also, FCM token will be set in DeviceInfo, which signals to
// the server that device listens to new invalidations.
// The device will not subscribe to old invalidations for any data types except
// Wallet and Offer, since that will be covered by the new system.
// SyncSendInterestedDataTypes must be enabled for this to take effect.
inline constexpr base::Feature kUseSyncInvalidations = {
    "UseSyncInvalidations", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, types related to Wallet and Offer will be included in interested
// data types, and the device will listen to new invalidations for those types
// (if they are enabled).
// The device will not register for old invalidations at all.
// UseSyncInvalidations must be enabled for this to take effect.
inline constexpr base::Feature kUseSyncInvalidationsForWalletAndOffer = {
    "UseSyncInvalidationsForWalletAndOffer", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, optimization flags (single client and a list of FCM
// registration tokens) will be disabled if during the current sync cycle
// DeviceInfo has been updated.
inline constexpr base::Feature
    kSkipInvalidationOptimizationsWhenDeviceInfoUpdated = {
        "SkipInvalidationOptimizationsWhenDeviceInfoUpdated",
        base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_IOS)
// Returns whether RPC is enabled.
bool IsSyncTrustedVaultPassphraseiOSRPCEnabled();
#endif  // BUILDFLAG(IS_IOS)

inline constexpr base::Feature kSyncEnableHistoryDataType = {
    "SyncEnableHistoryDataType", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_FEATURES_H_
