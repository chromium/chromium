// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_FEATURES_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace browser_actuator {

// Features and params internal to the browser_actuator implementation:
// kill switches and tuning knobs that are not part of the embedder
// contract (the embedder-facing feature lives in public/features.h).

// Controls whether the background transport channel is enabled. If disabled,
// no connection will be made.
BASE_DECLARE_FEATURE(kBrowserActuatorChannelEnabled);

// The server→client push transport backed by the Rust StreamBody framing
// parser. On by default; serves as a kill switch and as the anchor for
// the transport tuning params below.
BASE_DECLARE_FEATURE(kBrowserActuatorProtoStreamTransport);

// Reconnection backoff: the first retry waits the base time, and each
// further consecutive failure doubles the wait, up to the max.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kProtoStreamBaseReconnectionTime);
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kProtoStreamMaxReconnectionTime);

// How long a connected stream may stay byteless before the stall watchdog
// declares it dead and reconnects; the server is expected to emit periodic
// `noop` keep-alives. Zero disables the watchdog entirely.
BASE_DECLARE_FEATURE_PARAM(base::TimeDelta, kProtoStreamStallTimeout);

// Maximum number of concurrent active transport sessions.
BASE_DECLARE_FEATURE_PARAM(int, kMaxTransportSessions);

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_FEATURES_H_
