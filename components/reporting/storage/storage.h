// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {

// Storage represents the data to be collected, stored persistently and uploaded
// according to the priority.
class Storage : public base::RefCountedThreadSafe<Storage> {
 public:
  // Creates Storage instance, and returns it with the completion callback.
  static void Create(
      const StorageOptions& options,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      scoped_refptr<EncryptionModuleInterface> encryption_module,
      scoped_refptr<CompressionModule> compression_module,
      base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> completion_cb);

  // Wraps and serializes Record (taking ownership of it), encrypts and writes
  // the resulting blob into the Storage (the last file of it) according to the
  // priority with the next sequencing id assigned. If file is going to
  // become too large, it is closed and new file is created.
  void Write(Priority priority,
             Record record,
             base::OnceCallback<void(Status)> completion_cb);

  // Confirms acceptance of the records according to the priority up to
  // |sequencing_id| (inclusively). All records with sequencing ids <= this
  // one can be removed from the Storage, and can no longer be uploaded.
  // If |force| is false (which is used in most cases), |sequencing_id| is
  // only accepted if no higher ids were confirmed before; otherwise it is
  // accepted unconditionally.
  void Confirm(Priority priority,
               absl::optional<int64_t> sequencing_id,
               bool force,
               base::OnceCallback<void(Status)> completion_cb);

  // Initiates upload of collected records according to the priority.
  // Called usually for a queue with an infinite or very large upload period.
  // Multiple |Flush| calls can safely run in parallel.
  // Returns error if cannot start upload.
  Status Flush(Priority priority);

  // If the server attached signed encryption key to the response, it needs to
  // be paased here.
  void UpdateEncryptionKey(SignedEncryptionInfo signed_encryption_key);

  Storage(const Storage& other) = delete;
  Storage& operator=(const Storage& other) = delete;

 protected:
  virtual ~Storage();

 private:
  friend class base::RefCountedThreadSafe<Storage>;

  // Private bridge class.
  class QueueUploaderInterface;

  // Private helper class for key upload/download to the file system.
  class KeyInStorage;

  // Private helper class for initial key delivery from the server.
  // It can be invoked multiple times in parallel, but will only do
  // one server roundtrip and notify all requestors upon its completion.
  class KeyDelivery;

  // Private constructor, to be called by Create factory method only.
  // Queues need to be added afterwards.
  Storage(const StorageOptions& options,
          scoped_refptr<EncryptionModuleInterface> encryption_module,
          scoped_refptr<CompressionModule> compression_module,
          UploaderInterface::AsyncStartUploaderCb async_start_upload_cb);

  // Initializes the object by adding all queues for all priorities.
  // Must be called once and only once after construction.
  // Returns OK or error status, if anything failed to initialize.
  Status Init();

  // Helper method that selects queue by priority. Returns error
  // if priority does not match any queue.
  // Note: queues_ never change after initialization is finished, so there is no
  // need to protect or serialize access to it.
  StatusOr<scoped_refptr<StorageQueue>> GetQueue(Priority priority);

  // Immutable options, stored at the time of creation.
  const StorageOptions options_;

  // Encryption module.
  scoped_refptr<EncryptionModuleInterface> encryption_module_;

  // Internal module for initiail key delivery from server.
  std::unique_ptr<KeyDelivery> key_delivery_;

  // Compression module.
  scoped_refptr<CompressionModule> compression_module_;

  // Internal key management module.
  std::unique_ptr<KeyInStorage> key_in_storage_;

  // Map priority->StorageQueue.
  base::flat_map<Priority, scoped_refptr<StorageQueue>> queues_;

  // Upload provider callback.
  const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_H_
