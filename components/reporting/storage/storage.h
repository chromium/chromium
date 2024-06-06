// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/key_delivery.h"
#include "components/reporting/storage/storage_base.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// Storage allows for multiple generations for a given priority (if
// multi-genetation mode is enabled for this priority via finch flag).

// In multi-generation mode each queue is uniquely identifiable by a generation
// globally unique ID (guid) + priority tuple The generation guid is a randomly
// generation string. Generation guids have a one-to-one relationship with <DM
// token, Priority> tuples.

// Queues are created lazily with given priority when Write is called with a
// DM token we haven't seen before, as opposed to creating all queues during
// storage creation.

// Multi-generation queue directory names now have the format of
// <priority>.<generation GUID>, as oppsed to legacy queues named just
// <priority>

// Storage only creates queues on startup if it finds non-empty queue
// subdirectories in the storage directory. But these queues do not enqueue
// new records. They send their records and stay empty until they are deleted
// on the next restart of Storage.

// Empty subdirectories in the storage directory are deleted on storage
// creation. TODO(b/278620137): should also delete empty directories every 1-2
// days.

// In single-generation mode (legacy mode) there is only one queue per priority.
// Queues are created at the first start of the Storage and never erased.

class Storage : public base::RefCountedThreadSafe<Storage> {
 public:
  // Creates Storage instance and returns it with the completion callback.
  static void Create(
      const StorageOptions& options,
      scoped_refptr<QueuesContainer> queues_container,
      scoped_refptr<EncryptionModuleInterface> encryption_module,
      scoped_refptr<CompressionModule> compression_module,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      base::OnceCallback<void(StatusOr<scoped_refptr<Storage>>)> completion_cb);

  // Wraps and serializes Record (taking ownership of it), encrypts and writes
  // the resulting blob into the Storage (the last file of it) according to
  // the priority with the next sequencing id assigned. If file is going to
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

 private:
  friend class base::RefCountedThreadSafe<Storage>;

  // Private helper class to initialize a single queue
  friend class CreateQueueContext;

  // Private helper class to flush all queues with a given priority
  friend class FlushContext;

  // Private constructor, to be called by Create factory method only.
  // Queues need to be added afterwards.
  Storage(const StorageOptions& options,
          scoped_refptr<QueuesContainer> queues_container,
          scoped_refptr<EncryptionModuleInterface> encryption_module,
          scoped_refptr<CompressionModule> compression_module,
          UploaderInterface::AsyncStartUploaderCb async_start_upload_cb);

  // Private destructor, as required by RefCountedThreadSafe.
  ~Storage();

  // Initializes the object by adding all queues for all priorities.
  // Must be called once and only once after construction.
  // Returns OK or error status, if anything failed to initialize.
  Status Init();

  // Helper method to select queue by priority on the Storage task runner and
  // return it, if succeeded, or return failure status otherwise.
  StatusOr<scoped_refptr<StorageQueue>> TryGetQueue(
      Priority priority,
      StatusOr<GenerationGuid> generation_guid);

  // Writes a record to the given queue.
  void WriteToQueue(Record record,
                    scoped_refptr<StorageQueue> queue,
                    base::OnceCallback<void(Status)> completion_cb);

  // Immutable options, stored at the time of creation.
  const StorageOptions options_;

  // Task runner for storage-wide operations (initialized in
  // `queues_container_`).
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

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

  // Queues container and storage degradation controller. If degradation is
  // enabled, in case of disk space pressure it facilitates dropping low
  // priority events to free up space for the higher priority ones.
  const scoped_refptr<QueuesContainer> queues_container_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_H_
