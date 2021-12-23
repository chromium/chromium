// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_
#define CHROMEOS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_

#include "base/component_export.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

// Provides APIs for logging to general network metrics that apply to all
// network types and their variants.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkMetricsHelper {
 public:
  // Logs connection result for network with given |guid|. If |shill_error| has
  // no value, a connection success is logged.
  static void LogAllConnectionResult(
      const std::string& guid,
      const absl::optional<std::string>& shill_error = absl::nullopt);

  NetworkMetricsHelper();
  ~NetworkMetricsHelper();
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_METRICS_NETWORK_METRICS_HELPER_H_
