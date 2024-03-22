// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/device.h"

#include "base/functional/bind.h"
#include "base/logging.h"

namespace data_migration {

Device::Device(NearbyConnection* nearby_connection,
               NearbyConnectionsManager* nearby_connections_manager)
    : nearby_connection_(nearby_connection),
      nearby_connections_manager_(nearby_connections_manager),
      rts_receiver_(nearby_connection, &pending_file_transfer_queue_) {
  InitializeNextFileTransfer();
}

Device::~Device() = default;

void Device::InitializeNextFileTransfer() {
  CHECK(!current_file_transfer_) << "File transfer already active";
  current_file_transfer_.emplace(
      nearby_connection_.get(), nearby_connections_manager_.get(),
      pending_file_transfer_queue_,
      base::BindOnce(&Device::OnCurrentFileTransferComplete,
                     weak_factory_.GetWeakPtr()));
}

void Device::OnCurrentFileTransferComplete(bool success) {
  if (success) {
    VLOG(1) << "File transfer completed successfully";
  } else {
    LOG(ERROR) << "File transfer failed";
  }
  current_file_transfer_.reset();
  InitializeNextFileTransfer();
}

}  // namespace data_migration
