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

// Overrides the default server used for profile sync.
const char kSyncServiceURL[] = "sync-url";

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

// If enabled, the sync engine will be shut down in the "paused" state.
const base::Feature kStopSyncInPausedState{"StopSyncInPausedState",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USS implementation of Passwords datatype.
const base::Feature kSyncUSSPasswords{"SyncUSSPasswords",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable USS implementation of Nigori datatype.
const base::Feature kSyncUSSNigori{"SyncUSSNigori",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable syncing of Wi-Fi configurations.
const base::Feature kSyncWifiConfigurations{"SyncWifiConfigurations",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables updating a BookmarkNode's GUID by replacing the node itself.
const base::Feature kUpdateBookmarkGUIDWithNodeReplacement{
    "UpdateGUIDWithNodeReplacement", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the GUID-aware merge algorithm.
const base::Feature kMergeBookmarksUsingGUIDs{
    "MergeBookmarksUsingGUIDs", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSyncDeviceInfoInTransportMode{
    "SyncDeviceInfoInTransportMode", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the running of backend ProfileSyncService tasks on the ThreadPool.
const base::Feature kProfileSyncServiceUsesThreadPool{
    "ProfileSyncServiceUsesThreadPool", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace switches
