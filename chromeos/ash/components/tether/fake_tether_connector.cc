// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_tether_connector.h"

namespace ash {

namespace tether {

FakeTetherConnector::FakeTetherConnector() = default;

FakeTetherConnector::~FakeTetherConnector() = default;

void FakeTetherConnector::ConnectToNetwork(
    const std::string& tether_network_guid,
    base::OnceClosure success_callback,
    StringErrorCallback error_callback) {
  last_connected_tether_network_guid_ = tether_network_guid;

  if (connection_error_name_.empty())
    std::move(success_callback).Run();
  else
    std::move(error_callback).Run(connection_error_name_);
}

bool FakeTetherConnector::CancelConnectionAttempt(
    const std::string& tether_network_guid) {
  last_canceled_tether_network_guid_ = tether_network_guid;
  return should_cancel_successfully_;
}

}  // namespace tether

}  // namespace ash
