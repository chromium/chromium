// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_DEVICE_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_DEVICE_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/data_migration/file_transfer.h"
#include "chromeos/ash/components/data_migration/pending_file_transfer_queue.h"
#include "chromeos/ash/components/data_migration/rts_receiver.h"

class NearbyConnection;
class NearbyConnectionsManager;

namespace data_migration {

// Represents a remote device from which the desired data is being copied.
// Devices are instantiated after a nearby connection with it has been initiated
// and accepted. It primarily handles exchanging payloads with the remote
// device.
class Device {
 public:
  Device(NearbyConnection* nearby_connection,
         NearbyConnectionsManager* nearby_connections_manager);
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;
  ~Device();

 private:
  void InitializeNextFileTransfer();
  void OnCurrentFileTransferComplete(bool success);

  const raw_ptr<NearbyConnection> nearby_connection_;
  const raw_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  PendingFileTransferQueue pending_file_transfer_queue_;
  RtsReceiver rts_receiver_;
  std::optional<FileTransfer> current_file_transfer_;

  base::WeakPtrFactory<Device> weak_factory_{this};
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_DEVICE_H_
