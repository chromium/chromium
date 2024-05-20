// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_H_

#include <memory>

namespace ash::tether {

// Preserves a single BLE Connection beyond its immediately useful lifetime in
// the hope that the BLE Connection will be useful in the future -- thus
// preventing the need for a 2nd expensive setup of the Connection. This logic
// is only used after a host scan, in anticipation of a host connection attempt.
class ConnectionPreserver {
 public:
  virtual ~ConnectionPreserver() = default;

  // Should be called after each successful host scan result, to request that
  // the Connection with that device be preserved.
  virtual void HandleSuccessfulTetherAvailabilityResponse(
      const std::string& device_id) = 0;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_H_
