// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_DATA_MIGRATION_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_DATA_MIGRATION_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/data_migration/device.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace data_migration {

// Core entrypoint for go/spa-data-migration.
//
// Handles connection establishment with a remote device, and once that's
// complete, the `Device` handles the actual data transfer from the remote
// device to this device.
//
// Intended sequence:
// (NC-* means * function or concept * in Nearby Connection library.)
//
// 1. Caller invokes `StartAdvertising()`. NC starts advertising itself to
//    remote devices in the vicinity.
// 2. Remote device discovers this device and sends a request to initiate a
//    connection - `OnIncomingConnectionInitiated()`.
// 3. NC library automatically accepts the incoming request. The incoming
//    request has an authentication token, from which a four digit pin is
//    derived. This pin is displayed in a UI to the user on this device.
// 4. The exact same pin is derived and displayed on the remote device. The
//    user checks that the pins match and proceeds with the transfer. The remote
//    device then accepts the nearby connection, ultimately causing
//    `OnIncomingConnectionAccepted()` to be invoked.
// 5. `Device` is instantiated and the remote device starts transferring data
//    to this device.
class DataMigration
    : public KeyedService,
      public NearbyConnectionsManager::IncomingConnectionListener {
 public:
  explicit DataMigration(
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager);
  DataMigration(const DataMigration&) = delete;
  DataMigration& operator=(const DataMigration&) = delete;
  ~DataMigration() override;

  // Starts advertising this device as a target device to which data can be
  // copied from a remote device; the remote device is expected to run
  // discovery. This is the only method the caller has to invoke, and it only
  // needs to be invoked once. `DataMigration` handles the rest of the transfer
  // internally. `DataMigration` can be destroyed at any point to gracefully
  // abort the transfer.
  //
  // TODO(esum): Add a completion callback that's invoked when the transfer is
  // complete. Currently, there isn't a completion message in the migration
  // protocol, so this can't be added yet.
  void StartAdvertising();

 private:
  // KeyedService:
  void Shutdown() override;

  // NearbyConnectionsManager::IncomingConnectionListener:
  void OnIncomingConnectionInitiated(
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info) override;
  void OnIncomingConnectionAccepted(const std::string& endpoint_id,
                                    const std::vector<uint8_t>& endpoint_info,
                                    NearbyConnection* connection) override;

  void OnStartAdvertising(NearbyConnectionsManager::ConnectionsStatus status);
  void OnStopAdvertising(NearbyConnectionsManager::ConnectionsStatus status);
  void OnDeviceDisconnected();

  const std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  // Null if a remote device is not connected (i.e. advertisement is active).
  std::optional<Device> connected_device_;
  base::WeakPtrFactory<DataMigration> weak_factory_{this};
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_DATA_MIGRATION_H_
