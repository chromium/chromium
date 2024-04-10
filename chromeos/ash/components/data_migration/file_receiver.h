// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_FILE_RECEIVER_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_FILE_RECEIVER_H_

#include <cstdint>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"

namespace data_migration {

// Receives a single file over the network from a remote device via the NC
// library. `FileReceiver` may be destroyed at any time to cancel the file
// transfer. If it is destroyed while the file transfer is still in progress or
// after the transfer has failed, there may be a partially complete local file
// on disc from the transfer. The caller has two options:
// 1) Retry the transfer by creating a new `FileReceiver`, which will overwrite
//    the existing partial file.
// 2) Delete the partial file themselves. This is intentionally not handled by
//    `FileReceiver`'s destructor.
class FileReceiver : public NearbyConnectionsManager::PayloadStatusListener {
 public:
  struct Observer {
    Observer(base::OnceClosure on_file_registered_in,
             base::OnceCallback<void(bool)> on_file_transfer_complete_in);
    Observer(Observer&&);
    Observer& operator=(Observer&&);
    ~Observer();

    // Invoked when the file has been registered with the NC library (a
    // prerequisite for the transmission to begin). When this is run, the
    // NC library is aware of this file's expected transmission, and the
    // remote device can begin the transfer. The transfer on the remote device
    // should not be started until this callback is invoked or there are race
    // conditions where the transfer may fail.
    base::OnceClosure on_file_registered;

    // File transfer has completed. Argument indicates success or failure.
    // The `FileReceiver` is inactive when this is invoked and may be destroyed
    // immediately if desired.
    base::OnceCallback<void(bool)> on_file_transfer_complete;
  };

  // `path`: The location where the file is written as it's received from the
  // remote device. The caller must ensure the |path|'s directory exists and is
  // writeable.
  FileReceiver(int64_t payload_id,
               base::FilePath path,
               Observer observer,
               NearbyConnectionsManager* nearby_connections_manager);
  FileReceiver(const FileReceiver&) = delete;
  FileReceiver& operator=(const FileReceiver&) = delete;
  ~FileReceiver() override;

 private:
  // PayloadStatusListener:
  void OnStatusUpdate(PayloadTransferUpdatePtr update,
                      std::optional<Medium> upgraded_medium) override;

  void RegisterPayloadPath(int attempt_number);
  void OnRegisterPayloadPathComplete(
      int attempt_number,
      NearbyConnectionsManager::ConnectionsStatus result);
  void VerifyFileTransferResult(int64_t expected_size_in_bytes);
  void CompleteTransfer(bool verification_status);

  const int64_t payload_id_;
  const base::FilePath path_;
  Observer observer_;
  const raw_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  // False if either transfer is in progress or the transfer has failed.
  bool transfer_completed_successfully_ = false;
  // Note `NearbyConnectionsManager::PayloadStatusListener` has its own
  // `weak_ptr_factory_`, but a factory of type `FileReceiver` is still
  // required for binding `FileReceiver` methods.
  base::WeakPtrFactory<FileReceiver> file_receiver_weak_factory_{this};
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_FILE_RECEIVER_H_
