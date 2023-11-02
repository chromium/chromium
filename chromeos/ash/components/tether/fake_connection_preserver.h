// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_CONNECTION_PRESERVER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_CONNECTION_PRESERVER_H_

#include <string>

#include "chromeos/ash/components/tether/connection_preserver.h"

namespace ash {

namespace tether {

// Test double for ConnectionPreserver.
class FakeConnectionPreserver : public ConnectionPreserver {
 public:
  FakeConnectionPreserver();

  FakeConnectionPreserver(const FakeConnectionPreserver&) = delete;
  FakeConnectionPreserver& operator=(const FakeConnectionPreserver&) = delete;

  ~FakeConnectionPreserver() override;

  void HandleSuccessfulTetherAvailabilityResponse(
      const std::string& device_id) override;

  const std::string& last_requested_preserved_connection_device_id() {
    return last_requested_preserved_connection_device_id_;
  }

 private:
  std::string last_requested_preserved_connection_device_id_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_FAKE_CONNECTION_PRESERVER_H_
