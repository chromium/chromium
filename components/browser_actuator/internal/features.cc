// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/features.h"

namespace browser_actuator {

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

}  // namespace browser_actuator
