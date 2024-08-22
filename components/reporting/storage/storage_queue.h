// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_QUEUE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_QUEUE_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_managed_buffer.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/refcounted_closure_list.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

BASE_DECLARE_FEATURE(kReportingStorageDegradationFeature);

namespace test {

// Storage Queue operation kind used to associate operations with failures for
// testing purposes
enum class StorageQueueOperationKind {
  kReadBlock,
  kWriteBlock,
  kWriteMetadata,
  kWrappedRecordLowMemory,
  kEncryptedRecordLowMemory,
  kWriteLowDiskSpace,
};

using ErrorInjectionHandlerType =
    base::RepeatingCallback<Status(test::StorageQueueOperationKind, int64_t)>;

}  // namespace test

// Storage queue represents single queue of data to be collected and stored
// persistently. It allows to add whole data records as necessary,
// flush previously collected records and confirm records up to certain
// sequencing id to be eliminated.
class StorageQueue : public base::RefCountedDeleteOnSequence<StorageQueue> {
 public:
  // Creates StorageQueue instance with the specified options, and returns it
  // with the |completion_cb| callback. |async_start_upload_cb| is a factory
  // callback that instantiates UploaderInterface every time the queue starts
  // uploading records - periodically or immediately after Write (and in the
  // near future - upon explicit Flush request).
  static void Create(
      const QueueOptions& options,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      scoped_refptr<EncryptionModuleInterface> encryption_module,
      scoped_refptr<CompressionModule> compression_module,
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageQueue>>)>
          completion_cb);

  StorageQueue(const StorageQueue& other) = delete;
  StorageQueue& operator=(const StorageQueue& other) = delete;

  // Wraps and serializes Record (taking ownership of it), encrypts and writes
  // the resulting blob into the StorageQueue (the last file of it) with the
  // next sequencing id assigned. The write is a non-blocking operation -
  // caller can "fire and forget" it (|completion_cb| allows to verify that
  // record has been successfully enqueued). If file is going to become too
  // large, it is closed and new file is created.
  // Helper methods: AssignLastFile, WriteHeaderAndBlock, OpenNewWriteableFile,
  // WriteMetadata, DeleteOutdatedMetadata.
  void Write(Record record, base::OnceCallback<void(Status)> completion_cb);

  // Confirms acceptance of the records up to
  // |sequence_information.sequencing_id()| (inclusively), if the
  // |sequence_information.generation_id()| matches. All records with
  // sequencing ids <= this one can be removed from the Storage, and can no
  // longer be uploaded. In order to reset to the very first record
  // (seq_id=0) |sequence_information.sequencing_id()| should be set to -1.
  // If |force| is false (which is used in most cases),
  // |sequence_information.sequencing_id()| is only accepted if no higher
  // ids were confirmed before; otherwise it is accepted unconditionally.
  // |sequence_information.priority()| is ignored - should have been used
  // by Storage when selecting the queue.
  // Helper methods: RemoveConfirmedData.
  void Confirm(SequenceInformation sequence_information,
               bool force,
               base::OnceCallback<void(Status)> completion_cb);

  // Initiates upload of collected records. Called periodically by timer, based
  // on upload_period of the queue, and can also be called explicitly - for
  // a queue with an infinite or very large upload period. Multiple |Flush|
  // calls can safely run in parallel.
  // Starts by calling |async_start_upload_cb_| that instantiates
  // |UploaderInterface uploader|. Then repeatedly reads EncryptedRecord(s) one
  // by one from the StorageQueue starting from |first_sequencing_id_|, handing
  // each one over to |uploader|->ProcessRecord (keeping ownership of the
  // buffer) and resuming after result callback returns 'true'. Only files that
  // have been closed are included in reading; |Upload| makes sure to close the
  // last writeable file and create a new one before starting to send records to
  // the |uploader|. If some records are not available or corrupt,
  // |uploader|->ProcessGap is called. If the monotonic order of sequencing is
  // broken, INTERNAL error Status is reported. |Upload| can be stopped after
  // any record by returning 'false' to |processed_cb| callback - in that case
  // |Upload| will behave as if the end of data has been reached. While one or
  // more |Upload|s are active, files can be added to the StorageQueue but
  // cannot be deleted. If processing of the record takes significant time,
  // |uploader| implementation should be offset to another thread to avoid
  // locking StorageQueue. Helper methods: SwitchLastFileIfNotEmpty,
  // CollectFilesForUpload.
  void Flush(base::OnceCallback<void(Status)> completion_cb);

  // Assigns degradation vector to the queue, allowing to shed records
  // from those queue in ascending order when the current queue does not have
  // enough disk space. Can only be called once after the queues are initialized
  // and before they are actually used.
  void AssignDegradationQueues(
      const std::vector<scoped_refptr<StorageQueue>>& degradation_queues);

  // Registers completion notification callback. Thread-safe.
  // All registered callbacks are called when the queue destruction comes
  // to its completion.
  void RegisterCompletionCallback(base::OnceClosure callback);

  // Test only: provides an injection handler that would receive operation kind
  // and seq id, and then return Status. Non-OK Status injects the error and
  // can be returned as a resulting operation status too.
  // If `handler` is null, error injections is disabled.
  void TestInjectErrorsForOperation(
      test::ErrorInjectionHandlerType handler = base::NullCallback());

  // Access queue options.
  const QueueOptions& options() const { return options_; }

  // Returns the file sequence ID (the first sequence ID in the file) if the
  // sequence ID can be extracted from the extension. Otherwise, returns an
  // error status.
  static StatusOr<int64_t> GetFileSequenceIdFromPath(
      const base::FilePath& file_name);

 protected:
  virtual ~StorageQueue();

 private:
  friend class base::RefCountedDeleteOnSequence<StorageQueue>;
  friend class base::DeleteHelper<StorageQueue>;

  // Private data structures for Read and Write (need access to the private
  // StorageQueue fields).
  class WriteContext;
  class ReadContext;
  class ConfirmContext;

  // Private envelope class for single file in a StorageQueue.
  class SingleFile : public base::RefCountedThreadSafe<SingleFile> {
   public:
    // Factory method creates a SingleFile object for existing
    // or new file (of zero size). In case of any error (e.g. insufficient disk
    // space) returns status.
    static StatusOr<scoped_refptr<SingleFile>> Create(
        const base::FilePath& filename,
        int64_t size,
        scoped_refptr<ResourceManager> memory_resource,
        scoped_refptr<ResourceManager> disk_space_resource,
        scoped_refptr<RefCountedClosureList> completion_closure_list);

    Status Open(bool read_only);  // No-op if already opened.
    void Close();                 // No-op if not opened.

    void DeleteWarnIfFailed();

    // Attempts to read |size| bytes from position |pos| and returns
    // reference to the data that were actually read (no more than |size|).
    // End of file is indicated by empty data.
    // |max_buffer_size| specifies the largest allowed buffer, which
    // must accommodate the largest possible data block plus header and
    // overhead.
    // |expect_readonly| must match to is_readonly() (when set to false,
    // the file is expected to be writeable; this only happens when scanning
    // files restarting the queue).
    StatusOr<std::string_view> Read(uint32_t pos,
                                    uint32_t size,
                                    size_t max_buffer_size,
                                    bool expect_readonly = true);

    // Appends data to the file.
    StatusOr<uint32_t> Append(std::string_view data);

    bool is_opened() const { return handle_.get() != nullptr; }
    bool is_readonly() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      CHECK(is_opened());
      return is_readonly_.value();
    }
    uint64_t size() const { return size_; }
    std::string name() const { return filename_.MaybeAsASCII(); }

   protected:
    virtual ~SingleFile();

   private:
    friend class base::RefCountedThreadSafe<SingleFile>;

    // Private constructor, called by factory method only.
    SingleFile(const base::FilePath& filename,
               int64_t size,
               scoped_refptr<ResourceManager> memory_resource,
               scoped_refptr<ResourceManager> disk_space_resource,
               scoped_refptr<RefCountedClosureList> completion_closure_list);

    SEQUENCE_CHECKER(sequence_checker_);

    // Completion closure list reference. Dropped last, when `ReadContext` is
    // destructed.
    const scoped_refptr<RefCountedClosureList> completion_closure_list_;

    // Flag (valid for opened file only): true if file was opened for reading
    // only, false otherwise.
    std::optional<bool> is_readonly_ GUARDED_BY_CONTEXT(sequence_checker_);

    const base::FilePath filename_;  // relative to the StorageQueue directory
    uint64_t size_ = 0;  // tracked internally rather than by filesystem

    std::unique_ptr<base::File> handle_;  // Set only when opened/created.

    const scoped_refptr<ResourceManager> memory_resource_;
    const scoped_refptr<ResourceManager> disk_space_resource_;

    // When reading the file, this is the buffer and data positions.
    // If the data is read sequentially, buffered portions are reused
    // improving performance. When the sequential order is broken (e.g.
    // we start reading the same file in parallel from different position),
    // the buffer is reset.
    size_t data_start_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
    size_t data_end_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
    uint64_t file_position_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
    ResourceManagedBuffer buffer_ GUARDED_BY_CONTEXT(sequence_checker_);
  };

  // Private constructor, to be called by Create factory method only.
  StorageQueue(scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
               const QueueOptions& options,
               UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
               scoped_refptr<EncryptionModuleInterface> encryption_module,
               scoped_refptr<CompressionModule> compression_module);

  // Initializes the object by enumerating files in the assigned directory
  // and determines the sequence information of the last record.
  // Must be called once and only once after construction.
  // Returns OK or error status, if anything failed to initialize.
  // Called once, during initialization.
  // Helper methods: EnumerateDataFiles, ScanLastFile, RestoreMetadata.
  Status Init();

  // Retrieves last record digest (does not exist at a generation start).
  std::optional<std::string> GetLastRecordDigest() const;

  // Helper method for Init(): process single data file.
  // Return sequencing_id from <prefix>.<sequencing_id> file name, or Status
  // in case there is any error.
  StatusOr<int64_t> AddDataFile(
      const base::FilePath& full_name,
      const base::FileEnumerator::FileInfo& file_info);

  // Helper method for Init(): sets generation id based on data file name.
  // For backwards compatibility, accepts file name without generation too.
  Status SetOrConfirmGenerationId(const base::FilePath& full_name);

  // Helper method for Init(): enumerates all data files in the directory.
  // Valid file names are <prefix>.<sequencing_id>, any other names are ignored.
  // Adds used data files to the set.
  Status EnumerateDataFiles(base::flat_set<base::FilePath>* used_files_set);

  // Helper method for Init(): scans the last file in StorageQueue, if there are
  // files at all, and learns the latest sequencing id. Otherwise (if there
  // are no files) sets it to 0.
  Status ScanLastFile();

  // Helper method for Write(): increments sequencing id and assigns last
  // file to place record in. |size| parameter indicates the size of data that
  // comprise the record expected to be appended; if appending the record will
  // make the file too large, the current last file will be closed, and a new
  // file will be created and assigned to be the last one.
  StatusOr<scoped_refptr<SingleFile>> AssignLastFile(size_t size);

  // Helper method for Write() and Read(): creates and opens a new empty
  // writeable file, adding it to |files_|.
  StatusOr<scoped_refptr<SingleFile>> OpenNewWriteableFile();

  // Helper method for Write(): stores a file with metadata to match the
  // incoming new record. Synchronously composes metadata to record, then
  // asynchronously writes it into a file with next sequencing id and then
  // notifies the Write operation that it can now complete. After that it
  // asynchronously deletes all other files with lower sequencing id
  // (multiple Writes can see the same files and attempt to delete them, and
  // that is not an error).
  Status WriteMetadata(std::string_view current_record_digest);

  // Helper method for RestoreMetadata(): loads and verifies metadata file
  // contents. If accepted, adds the file to the set.
  Status ReadMetadata(const base::FilePath& meta_file_path,
                      size_t size,
                      int64_t sequencing_id,
                      base::flat_set<base::FilePath>* used_files_set);

  // Helper method for Init(): locates file with metadata that matches the
  // last sequencing id and loads metadata from it.
  // Adds used metadata file to the set.
  Status RestoreMetadata(base::flat_set<base::FilePath>* used_files_set);

  // Delete all files except those listed in |used_file_set|.
  void DeleteUnusedFiles(
      const base::flat_set<base::FilePath>& used_files_set) const;

  // Helper method for Write(): deletes meta files up to, but not including
  // |sequencing_id_to_keep|. Any errors are ignored.
  void DeleteOutdatedMetadata(int64_t sequencing_id_to_keep) const;

  // Helper method for Write(): composes record header and writes it to the
  // file, followed by data. Stores record digest in the queue, increments
  // next sequencing id.
  Status WriteHeaderAndBlock(std::string_view data,
                             std::string_view current_record_digest,
                             scoped_refptr<SingleFile> file);

  // Helper method for Upload: if the last file is not empty (has at least one
  // record), close it and create the new one, so that its records are also
  // included in the reading.
  Status SwitchLastFileIfNotEmpty();

  // Helper method for Upload: collects and sets aside |files| in the
  // StorageQueue that have data for the Upload (all files that have records
  // with sequencing ids equal or higher than |sequencing_id|).
  std::map<int64_t, scoped_refptr<SingleFile>> CollectFilesForUpload(
      int64_t sequencing_id) const;

  // Helper method for Confirm: Moves |first_sequencing_id_| to
  // (|sequencing_id|+1) and removes files that only have records with seq
  // ids below or equal to |sequencing_id| (below |first_sequencing_id_|).
  Status RemoveConfirmedData(int64_t sequencing_id);

  // Helper method to release all file instances held by the queue.
  // Files on the disk remain as they were.
  void ReleaseAllFileInstances();

  // Helper method to retry upload if prior one failed or if some events below
  // |next_sequencing_id| were not uploaded.
  void CheckBackUpload(Status status, int64_t next_sequencing_id);

  // Helper method called by periodic time to upload data.
  void PeriodicUpload();

  // Helper method to reserve space needed to write a new record.
  Status ReserveNewRecordDiskSpace(size_t total_size);

  // Sequentially removes the files comprising the queue from oldest to newest
  // to recover disk space so higher priority files can be stored. This function
  // is posted iteratively through all StorageQueues in the
  // `degradation_queues_` until enough space is recovered. Once all the queues
  // available are used to shed files, then the Helper function
  // RecordsSheddingHelper is triggered to shed files from the queue that is
  // trying to write a new record, `writing_storage_queue`. Parameters:
  // `degradation_queues` -> contains the queues still available where files can
  // be shed from. writing_storage_queue
  // -> a reference to the queue that is trying to write a record.
  // `space_to_recover` -> an addition of the space RecordsShedding needs to
  // recover by shedding files to write the record. `resume_writing_cb` ->
  // callback to retry writing the new record with the newly available space.
  // `writing_failure_cb` -> callback to log the writing error and close the
  // writing process.
  void RecordsShedding(
      base::span<scoped_refptr<StorageQueue>> degradation_queues,
      scoped_refptr<StorageQueue> writing_storage_queue,
      const size_t space_to_recover,
      base::OnceClosure resume_writing_cb,
      base::OnceClosure writing_failure_cb);

  // Helper function for RecordsShedding used to shed records from the queue
  // that is trying to write a new record.
  void RecordsSheddingHelper(const size_t space_to_recover,
                             base::OnceClosure resume_writing_cb,
                             base::OnceClosure writing_failure_cb);

  // This function iterates over the files_ map and removes them in order of
  // oldest to newest until disk_space_resource has more space available than
  // `space_to_recover`.
  bool FilesShedding(const size_t space_to_recover);

  // Sequential task runner for all activities in this StorageQueue
  // (must be first member in class).
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(storage_queue_sequence_checker_);

  // Completion closure list reference. Dropped when `StorageQueue` is
  // destructed.
  const scoped_refptr<RefCountedClosureList> completion_closure_list_;

  // Dedicated sequence task runner for low priority actions (which make
  // no impact on the main activity - e.g., deletion of the outdated metafiles).
  // Serializeing them should reduce their impact.
  const scoped_refptr<base::SequencedTaskRunner> low_priority_task_runner_;

  // Immutable options, stored at the time of creation.
  const QueueOptions options_;

  // List of queues to be used for degradation (from lowest priority
  // to the one below our own). Can only be set once by
  // `AssignDegradationQueues` after the queues are initialized and before they
  // are actually used, so it becomes effectively immutable.
  std::vector<scoped_refptr<StorageQueue>> degradation_queues_;

  // Current generation id, unique per device and queue.
  // Set up once during initialization by reading from the 'gen_id.NNNN' file
  // matching the last sequencing id, or generated anew as a random number if no
  // such file found (files do not match the id).
  int64_t generation_id_ = 0;

  // Digest of the last written record (loaded at queue initialization, absent
  // if the new generation has just started, and no records where stored yet).
  std::optional<std::string> last_record_digest_;

  // Queue of the write context instances in the order of creation, sequencing
  // ids and record digests. Context is always removed from this queue before
  // being destructed. We use std::list rather than std::queue, because
  // if the write fails, it needs to be removed from the queue regardless of
  // whether it is at the head, tail or middle.
  // Weak pointer allows to detect premature destruction of a context.
  std::list<base::WeakPtr<WriteContext>> write_contexts_queue_
      GUARDED_BY_CONTEXT(storage_queue_sequence_checker_);

  // Reflects reservation for the head of the write contexts queue. Will return
  // to 0 after each writing process is finished. It helps keep disk space usage
  // accurate and within the bounds of the reservation.
  size_t active_write_reservation_size_ = 0;

  // Next sequencing id to store (not assigned yet).
  int64_t next_sequencing_id_ = 0;

  // First sequencing id store still has (no records with lower
  // sequencing id exist in store).
  int64_t first_sequencing_id_ = 0;

  // First unconfirmed sequencing id (no records with lower
  // sequencing id will be ever uploaded). Set by the first
  // Confirm call.
  // If first_unconfirmed_sequencing_id_ < first_sequencing_id_,
  // [first_unconfirmed_sequencing_id_, first_sequencing_id_) is a gap
  // that cannot be filled in and is uploaded as such.
  std::optional<int64_t> first_unconfirmed_sequencing_id_;

  // Ordered map of the files by ascending sequencing id.
  std::map<int64_t, scoped_refptr<SingleFile>> files_;

  // Counter of the Read operations. When not 0, none of the files_ can be
  // deleted. Incremented by |ReadContext::OnStart|, decremented by
  // |ReadContext::OnComplete|. Accessed by |RemoveConfirmedData|.
  // All accesses take place on sequenced_task_runner_.
  int32_t active_read_operations_ = 0;

  // Upload timer (active only if options_.upload_period() is not 0 and not
  // infinity).
  base::RepeatingTimer upload_timer_;

  // Check back after upload timer (activated after upload has been started
  // and options_.upload_retry_delay() is not 0). If already started, it will
  // be reset to the new delay.
  base::RetainingOneShotTimer check_back_timer_;

  // Upload provider callback.
  const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;

  // Encryption module.
  scoped_refptr<EncryptionModuleInterface> encryption_module_;

  // Compression module.
  scoped_refptr<CompressionModule> compression_module_;

  // Test only: records callback to be invoked. It will be called with operation
  // kind and seq id, and will return Status (non-OK status indicates the
  // failure to be injected). In production code must be null.
  test::ErrorInjectionHandlerType test_injection_handler_{base::NullCallback()};

  // Weak pointer factory (must be last member in class).
  base::WeakPtrFactory<StorageQueue> weakptr_factory_{this};
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_QUEUE_H_
