// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

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

  Storage(const Storage& other) = delete;
  Storage& operator=(const Storage& other) = delete;

  // Wraps and serializes Record (taking ownership of it), encrypts and writes
  // the resulting blob into the Storage (the last file of it) according to the
  // priority with the next sequencing id assigned. If file is going to
  // become too large, it is closed and new file is created.
  void Write(Priority priority,
             Record record,
             base::OnceCallback<void(Status)> completion_cb);

  // Confirms acceptance of the records according to the
  // |sequence_information.priority()| up to
  // |sequence_information.sequencing_id()| (inclusively), if the
  // |sequence_information.generation_id()| matches. All records with sequencing
  // ids <= this one can be removed from the Storage, and can no longer be
  // uploaded. In order to reset to the very first record (seq_id=0)
  // |sequence_information.sequencing_id()| should be set to -1.
  // If |force| is false (which is used in most cases),
  // |sequence_information.sequencing_id()| is only accepted if no higher ids
  // were confirmed before; otherwise it is accepted unconditionally.
  void Confirm(SequenceInformation sequence_information,
               bool force,
               base::OnceCallback<void(Status)> completion_cb);

  // Initiates upload of collected records according to the priority.
  // Called usually for a queue with an infinite or very large upload period.
  // Multiple |Flush| calls can safely run in parallel.
  // Invokes |completion_cb| with error if upload fails or cannot start.
  void Flush(Priority priority, base::OnceCallback<void(Status)> completion_cb);

  // If the server attached signed encryption key to the response, it needs to
  // be paased here.
  void UpdateEncryptionKey(SignedEncryptionInfo signed_encryption_key);

  // Registers completion notification callback. Thread-safe.
  // All registered callbacks are called when all queues destructions come
  // to their completion and the Storage is destructed as well.
  void RegisterCompletionCallback(base::OnceClosure callback);

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
  StatusOr<scoped_refptr<StorageQueue>> GetQueue(Priority priority) const;

  // Helper method to select queue by priority on the Storage task runner and
  // then perform `queue_action`, if succeeded. Returns failure on any stage
  // with `completion_cb`.
  void AsyncGetQueueAndProceed(
      Priority priority,
      base::OnceCallback<void(scoped_refptr<StorageQueue>,
                              base::OnceCallback<void(Status)>)> queue_action,
      base::OnceCallback<void(Status)> completion_cb);

  // Immutable options, stored at the time of creation.
  const StorageOptions options_;

  // Encryption module.
  const scoped_refptr<EncryptionModuleInterface> encryption_module_;

  // Internal module for initiail key delivery from server.
  const std::unique_ptr<KeyDelivery, base::OnTaskRunnerDeleter> key_delivery_;

  // Compression module.
  const scoped_refptr<CompressionModule> compression_module_;

  // Internal key management module.
  const std::unique_ptr<KeyInStorage> key_in_storage_;

  // Upload provider callback.
  const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;

  // Task runner for storage-wide operations (initialization, queues selection).
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Map priority->StorageQueue.
  base::flat_map<Priority, scoped_refptr<StorageQueue>> queues_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_H_
