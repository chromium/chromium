// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_MOCK_HOST_CONNECTION_METRICS_LOGGER_H_
#define CHROMEOS_COMPONENTS_TETHER_MOCK_HOST_CONNECTION_METRICS_LOGGER_H_

#include <string>

#include "chromeos/components/tether/host_connection_metrics_logger.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

namespace tether {

class ActiveHost;

class MockHostConnectionMetricsLogger : public HostConnectionMetricsLogger {
 public:
  MockHostConnectionMetricsLogger(ActiveHost* active_host);

  MockHostConnectionMetricsLogger(const MockHostConnectionMetricsLogger&) =
      delete;
  MockHostConnectionMetricsLogger& operator=(
      const MockHostConnectionMetricsLogger&) = delete;

  ~MockHostConnectionMetricsLogger() override;

  MOCK_METHOD2(RecordConnectionToHostResult,
               void(HostConnectionMetricsLogger::ConnectionToHostResult,
                    const std::string&));
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_MOCK_HOST_CONNECTION_METRICS_LOGGER_H_
