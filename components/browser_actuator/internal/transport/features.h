// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_FEATURES_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace browser_actuator::transport {

// Features and params of the transport: kill switches and tuning knobs.
// The endpoints are not here; the embedder passes them in through
// `TransportServiceConfig`.

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

}  // namespace browser_actuator::transport

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_FEATURES_H_
