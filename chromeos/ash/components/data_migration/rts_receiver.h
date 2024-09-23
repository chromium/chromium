// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_RTS_RECEIVER_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_RTS_RECEIVER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class NearbyConnection;

namespace data_migration {

class PendingFileTransferQueue;

// RTS - "Request-to-Send". A message sent from the remote device to this device
// asking if it's OK to start transferring a file (identified by its
// payload_id). The remote device will wait for a CTS ("Clear-to-Send") message
// in response before initiating the transfer. Both RTS and CTS are small
// metadata payloads.
//
// Continuously receives and parses RTS messages from the remote device and
// pushes the requested file payload ids to the `PendingFileTransferQueue`. If
// the connection to the remote device is closed while `RtsReceiver` is active,
// it silently stops receiving and is idle until destroyed.
class RtsReceiver {
 public:
  RtsReceiver(NearbyConnection* nearby_connection,
              PendingFileTransferQueue* pending_file_transfer_queue);
  RtsReceiver(const RtsReceiver&) = delete;
  RtsReceiver& operator=(const RtsReceiver&) = delete;
  ~RtsReceiver();

 private:
  void ReadNextRts();
  void OnRtsReceived(std::optional<std::vector<uint8_t>> bytes);

  raw_ptr<NearbyConnection> nearby_connection_;
  const raw_ptr<PendingFileTransferQueue> pending_file_transfer_queue_;
  base::WeakPtrFactory<RtsReceiver> weak_ptr_factory_{this};
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_RTS_RECEIVER_H_
