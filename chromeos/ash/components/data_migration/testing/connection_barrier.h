// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_CONNECTION_BARRIER_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_CONNECTION_BARRIER_H_

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"

class NearbyConnection;

namespace data_migration {

// Waits until both sides of a data_migration connection have been accepted.
class ConnectionBarrier
    : public NearbyConnectionsManager::IncomingConnectionListener {
 public:
  explicit ConnectionBarrier(
      NearbyConnectionsManager* nearby_connections_manager);
  ConnectionBarrier(const ConnectionBarrier&) = delete;
  ConnectionBarrier& operator=(const ConnectionBarrier&) = delete;
  ~ConnectionBarrier() override;

  // Blocks until both sides of the connection have been accepted and returns
  // the resulting `NearbyConnection` used to exchange bytes payloads. Returns
  // nullptr if the connection was not established within an internal timeout.
  NearbyConnection* Wait();

 private:
  // NearbyConnectionsManager::IncomingConnectionListener:
  void OnIncomingConnectionInitiated(
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info) override {}

  void OnIncomingConnectionAccepted(const std::string& endpoint_id,
                                    const std::vector<uint8_t>& endpoint_info,
                                    NearbyConnection* connection) override;

  const raw_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  base::test::TestFuture<NearbyConnection*> connection_accepted_;
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_CONNECTION_BARRIER_H_
