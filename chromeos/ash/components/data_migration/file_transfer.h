// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_FILE_TRANSFER_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_FILE_TRANSFER_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/data_migration/file_receiver.h"

class NearbyConnection;
class NearbyConnectionsManager;

namespace data_migration {

class PendingFileTransferQueue;

// Pops the next pending file from the `PendingFileTransferQueue` and performs
// the file transfer. `FileTransfer` may be destroyed at any time to cancel the
// transfer.
class FileTransfer {
 public:
  // `completion_cb` is run at the end of the transfer with success or failure.
  // Note `FileTransfer` does not handle retrying the transfer internally. It
  // is the remote device's responsibility to initiate a retry as it should also
  // get a failure notice for the transfer.
  FileTransfer(NearbyConnection* nearby_connection,
               NearbyConnectionsManager* nearby_connections_manager,
               PendingFileTransferQueue& pending_file_transfer_queue,
               base::OnceCallback<void(bool)> completion_cb);
  FileTransfer(const FileTransfer&) = delete;
  FileTransfer& operator=(const FileTransfer&) = delete;
  ~FileTransfer();

 private:
  void StartFileTransfer(int64_t payload_id);
  void SendCts(int64_t payload_id);

  const raw_ptr<NearbyConnection> nearby_connection_;
  const raw_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  base::OnceCallback<void(bool)> completion_cb_;
  std::optional<FileReceiver> file_receiver_;
  base::WeakPtrFactory<FileTransfer> weak_ptr_factory_{this};
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_FILE_TRANSFER_H_
