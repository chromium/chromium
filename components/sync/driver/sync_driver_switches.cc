// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/sync_driver_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

bool IsSyncAllowedByFlag() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSync);
}

#if defined(OS_IOS)
bool IsSyncTrustedVaultPassphraseiOSRPCEnabled() {
  return base::FeatureList::IsEnabled(
             switches::kSyncTrustedVaultPassphraseRecovery) &&
         base::FeatureList::IsEnabled(
             switches::kSyncTrustedVaultPassphraseiOSRPC);
}
#endif  // defined(OS_IOS)

// Disables syncing browser data to a Google Account.
const char kDisableSync[] = "disable-sync";

// Allows overriding the deferred init fallback timeout.
const char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";

// Enables deferring sync backend initialization until user initiated changes
// occur.
const char kSyncDisableDeferredStartup[] = "sync-disable-deferred-startup";

// Controls whether the initial state of the "Capture Specifics" flag on
// chrome://sync-internals is enabled.
const char kSyncIncludeSpecificsInProtocolLog[] = "sync-include-specifics";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
const char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";

// This flag significantly shortens the delay between nudge cycles. Its primary
// purpose is to speed up integration tests. The normal delay allows coalescing
// and prevention of server overload, so don't use this unless you're really
// sure that it's what you want.
const char kSyncShortNudgeDelayForTest[] = "sync-short-nudge-delay-for-test";

// Allows custom passphrase users to receive Wallet data for secondary accounts
// while in transport-only mode.
const base::Feature kSyncAllowWalletDataInTransportModeWithCustomPassphrase{
    "SyncAllowAutofillWalletDataInTransportModeWithCustomPassphrase",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable syncing of Autofill Wallet offer data.
const base::Feature kSyncAutofillWalletOfferData{
    "SyncAutofillWalletOfferData", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable syncing of Wi-Fi configurations.
const base::Feature kSyncWifiConfigurations{"SyncWifiConfigurations",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Sync requires policies to be loaded before starting.
const base::Feature kSyncRequiresPoliciesLoaded{
    "SyncRequiresPoliciesLoaded", base::FEATURE_DISABLED_BY_DEFAULT};

// Max time to delay the sync startup while waiting for policies to load.
const base::FeatureParam<base::TimeDelta> kSyncPolicyLoadTimeout{
    &kSyncRequiresPoliciesLoaded, "SyncPolicyLoadTimeout", base::Seconds(10)};

#if defined(OS_IOS)
// Whether RPC is enabled.
const base::Feature kSyncTrustedVaultPassphraseiOSRPC{
    "SyncTrustedVaultPassphraseiOSRPC", base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_IOS)

// Keep this entry in sync with the equivalent name in
// ChromeFeatureList.java.
const base::Feature kSyncTrustedVaultPassphraseRecovery{
  "SyncTrustedVaultPassphraseRecovery",
#if defined(OS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Whether the entry point to opt in to trusted vault in settings should be
// shown.
const base::Feature kSyncTrustedVaultPassphrasePromo{
    "SyncTrustedVaultPassphrasePromo", base::FEATURE_DISABLED_BY_DEFAULT};

#if defined(OS_CHROMEOS)
// Whether warning should be shown in sync settings page when lacros
// side-by-side mode is enabled.
const base::Feature kSyncSettingsShowLacrosSideBySideWarning{
    "SyncSettingsShowLacrosSideBySideWarning",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

}  // namespace switches
