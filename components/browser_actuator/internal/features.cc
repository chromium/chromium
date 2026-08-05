// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/features.h"

namespace browser_actuator {

BASE_FEATURE(kBrowserActuatorChannelEnabled, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBrowserActuatorProtoStreamTransport,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kProtoStreamBaseReconnectionTime,
                   &kBrowserActuatorProtoStreamTransport,
                   base::Seconds(3));
BASE_FEATURE_PARAM(base::TimeDelta,
                   kProtoStreamMaxReconnectionTime,
                   &kBrowserActuatorProtoStreamTransport,
                   base::Minutes(5));

// TODO(crbug.com/534573786): Align the default with the production
// server's noop cadence once that is known.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kProtoStreamStallTimeout,
                   &kBrowserActuatorProtoStreamTransport,
                   base::Minutes(2));

BASE_FEATURE_PARAM(int,
                   kMaxTransportSessions,
                   &kBrowserActuatorProtoStreamTransport,
                   3);

}  // namespace browser_actuator
