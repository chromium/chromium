// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/mock_host_connection_metrics_logger.h"

#include "chromeos/ash/components/tether/active_host.h"

namespace ash {

namespace tether {

MockHostConnectionMetricsLogger::MockHostConnectionMetricsLogger(
    ActiveHost* active_host)
    : HostConnectionMetricsLogger(active_host) {}

MockHostConnectionMetricsLogger::~MockHostConnectionMetricsLogger() = default;

}  // namespace tether

}  // namespace ash
