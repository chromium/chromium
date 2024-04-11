// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/file_transfer.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/data_migration/constants.h"
#include "chromeos/ash/components/data_migration/pending_file_transfer_queue.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"

namespace data_migration {

namespace {

base::FilePath GetPayloadPath(int64_t payload_id) {
  base::FilePath home_dir;
  CHECK(base::PathService::Get(base::DIR_HOME, &home_dir));
  return home_dir.Append(kPayloadTargetDir)
      .Append(base::StrCat({"payload_", base::NumberToString(payload_id)}));
}

}  // namespace

FileTransfer::FileTransfer(
    NearbyConnection* nearby_connection,
    NearbyConnectionsManager* nearby_connections_manager,
    PendingFileTransferQueue& pending_file_transfer_queue,
    base::OnceCallback<void(bool)> completion_cb)
    : nearby_connection_(nearby_connection),
      nearby_connections_manager_(nearby_connections_manager),
      completion_cb_(std::move(completion_cb)) {
  CHECK(nearby_connection_);
  CHECK(completion_cb_);
  pending_file_transfer_queue.Pop(base::BindOnce(
      &FileTransfer::StartFileTransfer, weak_ptr_factory_.GetWeakPtr()));
}

FileTransfer::~FileTransfer() = default;

void FileTransfer::StartFileTransfer(int64_t payload_id) {
  file_receiver_.emplace(
      payload_id, GetPayloadPath(payload_id),
      FileReceiver::Observer(/*on_file_registered=*/base::BindOnce(
                                 &FileTransfer::SendCts,
                                 weak_ptr_factory_.GetWeakPtr(), payload_id),
                             std::move(completion_cb_)),
      nearby_connections_manager_.get());
}

// Clear-to-Send is only sent to the remote device after the file has been
// registered with the NC library. If the remote device were to start
// transmitting the file before it's been registered, it would be a race
// condition and the transmission may fail.
void FileTransfer::SendCts(int64_t payload_id) {
  std::string payload_id_as_string = base::NumberToString(payload_id);
  VLOG(1) << "Sending CTS for file payload " << payload_id;
  nearby_connection_->Write(std::vector<uint8_t>(payload_id_as_string.begin(),
                                                 payload_id_as_string.end()));
}

}  // namespace data_migration
