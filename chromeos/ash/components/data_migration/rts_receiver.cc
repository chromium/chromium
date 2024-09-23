// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/rts_receiver.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/data_migration/pending_file_transfer_queue.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"

namespace data_migration {

RtsReceiver::RtsReceiver(NearbyConnection* nearby_connection,
                         PendingFileTransferQueue* pending_file_transfer_queue)
    : nearby_connection_(nearby_connection),
      pending_file_transfer_queue_(pending_file_transfer_queue) {
  CHECK(nearby_connection_);
  CHECK(pending_file_transfer_queue_);
  ReadNextRts();
}

RtsReceiver::~RtsReceiver() = default;

void RtsReceiver::ReadNextRts() {
  CHECK(nearby_connection_) << "Payload received after disconnection";
  nearby_connection_->Read(base::BindOnce(&RtsReceiver::OnRtsReceived,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void RtsReceiver::OnRtsReceived(std::optional<std::vector<uint8_t>> bytes) {
  // If `bytes` is null, the remote endpoint is disconnected or has become
  // unreachable.
  if (!bytes) {
    VLOG(4)
        << "Connection to the remote device has been closed. Stopping reads";
    // Prevent dangling raw_ptr error. `nearby_connection_` will not get used
    // anyways for the rest of the class's lifetime and in reality is destroyed
    // immediately after this.
    nearby_connection_ = nullptr;
    return;
  }

  std::string payload_id_as_string(bytes->begin(), bytes->end());
  int64_t payload_id = 0;
  if (base::StringToInt64(payload_id_as_string, &payload_id)) {
    VLOG(1) << "Received RTS for file payload " << payload_id;
    pending_file_transfer_queue_->Push(payload_id);
  } else {
    LOG(ERROR) << "Received invalid RTS file payload id";
  }
  ReadNextRts();
}

}  // namespace data_migration
