// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_COMMAND_LINE_SWITCHES_H_
#define COMPONENTS_SYNC_BASE_COMMAND_LINE_SWITCHES_H_

namespace syncer {

// Disables syncing browser data to a Google Account.
inline constexpr char kDisableSync[] = "disable-sync";

// Allows overriding the deferred init fallback timeout.
inline constexpr char kSyncDeferredStartupTimeoutSeconds[] =
    "sync-deferred-startup-timeout-seconds";

// Controls whether the initial state of the "Capture Specifics" flag on
// chrome://sync-internals is enabled.
inline constexpr char kSyncIncludeSpecificsInProtocolLog[] =
    "sync-include-specifics";

// Controls the number of ProtocolEvents that are buffered, and thus can be
// displayed on newly-opened chrome://sync-internals tabs.
inline constexpr char kSyncProtocolLogBufferSize[] =
    "sync-protocol-log-buffer-size";

// Overrides the default server used for profile sync.
inline constexpr char kSyncServiceURL[] = "sync-url";

// This flag causes sync to retry very quickly (see polling_constants.h) the
// when it encounters an error, as the first step towards exponential backoff.
inline constexpr char kSyncShortInitialRetryOverride[] =
    "sync-short-initial-retry-override";
// This flag significantly shortens the delay between nudge cycles. Its primary
// purpose is to speed up integration tests. The normal delay allows coalescing
// and prevention of server overload, so don't use this unless you're really
// sure that it's what you want.
inline constexpr char kSyncShortNudgeDelayForTest[] =
    "sync-short-nudge-delay-for-test";

// Returns whether sync is allowed to run based on command-line switches.
// Profile::IsSyncAllowed() is probably a better signal than this function.
// This function can be called from any thread, and the implementation doesn't
// assume it's running on the UI thread.
bool IsSyncAllowedByFlag();

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_COMMAND_LINE_SWITCHES_H_
