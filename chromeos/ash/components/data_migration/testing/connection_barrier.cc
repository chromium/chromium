// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/testing/connection_barrier.h"

#include <cstdint>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_helpers.h"

namespace data_migration {

ConnectionBarrier::ConnectionBarrier(
    NearbyConnectionsManager* nearby_connections_manager)
    : nearby_connections_manager_(nearby_connections_manager) {
  CHECK(nearby_connections_manager_);
  nearby_connections_manager_->StartAdvertising(
      /*endpoint_info=*/std::vector<uint8_t>(32, 0), this,
      NearbyConnectionsManager::PowerLevel::kHighPower,
      ::nearby_share::mojom::DataUsage::kOffline,
      // If `StartAdvertising()` fails, `Wait()` will fail with a timeout, so
      // there's no need to check this callback's return value.
      base::DoNothing());
}

ConnectionBarrier::~ConnectionBarrier() {
  // Unregisters raw pointer to "this" provided in past call to
  // `StartAdvertising()`.
  nearby_connections_manager_->StopAdvertising(base::DoNothing());
}

NearbyConnection* ConnectionBarrier::Wait() {
  return connection_accepted_.Wait() ? connection_accepted_.Get<0>() : nullptr;
}

void ConnectionBarrier::OnIncomingConnectionAccepted(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    NearbyConnection* connection) {
  connection_accepted_.SetValue(connection);
}

}  // namespace data_migration
