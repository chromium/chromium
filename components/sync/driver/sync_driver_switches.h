// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace switches {

// Returns whether sync is allowed to run based on command-line switches.
// Profile::IsSyncAllowed() is probably a better signal than this function.
// This function can be called from any thread, and the implementation doesn't
// assume it's running on the UI thread.
bool IsSyncAllowedByFlag();

#if BUILDFLAG(IS_IOS)
// Returns whether RPC is enabled.
bool IsSyncTrustedVaultPassphraseiOSRPCEnabled();
#endif  // BUILDFLAG(IS_IOS)

// Defines all the command-line switches used by sync driver. All switches in
// alphabetical order.
//
// Disables syncing browser data to a Google Account.
constexpr inline char kDisableSync[] = "disable-sync";
// Allows overriding the deferred init fallback timeout.
constexpr inline char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";
// Enables deferring sync backend initialization until user initiated changes
// occur.
constexpr inline char kSyncDisableDeferredStartup[] =
    "sync-disable-deferred-startup";
// Controls whether the initial state of the "Capture Specifics" flag on
// chrome://sync-internals is enabled.
constexpr inline char kSyncIncludeSpecificsInProtocolLog[] =
    "sync-include-specifics";
// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
constexpr inline char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";
// This flag significantly shortens the delay between nudge cycles. Its primary
// purpose is to speed up integration tests. The normal delay allows coalescing
// and prevention of server overload, so don't use this unless you're really
// sure that it's what you want.
constexpr inline char kSyncShortNudgeDelayForTest[] =
    "sync-short-nudge-delay-for-test";

// Allows custom passphrase users to receive Wallet data for secondary accounts
// while in transport-only mode.
constexpr inline base::Feature
    kSyncAllowWalletDataInTransportModeWithCustomPassphrase{
        "SyncAllowAutofillWalletDataInTransportModeWithCustomPassphrase",
        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable syncing of Autofill Wallet offer data.
constexpr inline base::Feature kSyncAutofillWalletOfferData{
    "SyncAutofillWalletOfferData", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable syncing of Wi-Fi configurations.
constexpr inline base::Feature kSyncWifiConfigurations{
    "SyncWifiConfigurations", base::FEATURE_ENABLED_BY_DEFAULT};

// Sync requires policies to be loaded before starting.
constexpr inline base::Feature kSyncRequiresPoliciesLoaded{
    "SyncRequiresPoliciesLoaded", base::FEATURE_DISABLED_BY_DEFAULT};

// Max time to delay the sync startup while waiting for policies to load.
constexpr inline base::FeatureParam<base::TimeDelta> kSyncPolicyLoadTimeout{
    &kSyncRequiresPoliciesLoaded, "SyncPolicyLoadTimeout", base::Seconds(10)};

#if BUILDFLAG(IS_IOS)
// Whether RPC is enabled.
constexpr inline base::Feature kSyncTrustedVaultPassphraseiOSRPC{
    "SyncTrustedVaultPassphraseiOSRPC", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_IOS)

// Keep this entry in sync with the equivalent name in
// ChromeFeatureList.java.
constexpr inline base::Feature kSyncTrustedVaultPassphraseRecovery{
    "SyncTrustedVaultPassphraseRecovery", base::FEATURE_ENABLED_BY_DEFAULT};

// Whether the entry point to opt in to trusted vault in settings should be
// shown.
constexpr inline base::Feature kSyncTrustedVaultPassphrasePromo{
    "SyncTrustedVaultPassphrasePromo", base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS)
// Whether warning should be shown in sync settings page when lacros
// side-by-side mode is enabled.
constexpr inline base::Feature kSyncSettingsShowLacrosSideBySideWarning{
    "SyncSettingsShowLacrosSideBySideWarning",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace switches

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_DRIVER_SWITCHES_H_
