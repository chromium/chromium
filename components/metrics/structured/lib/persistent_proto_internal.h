// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_INTERNAL_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_INTERNAL_H_

#include <atomic>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace metrics::structured {

// The result of reading a backing file from disk.
enum class ReadStatus {
  kOk = 0,
  kMissing = 1,
  kReadError = 2,
  kParseError = 3,
};

// The result of writing a backing file to disk.
enum class WriteStatus {
  kOk = 0,
  kWriteError = 1,
  kSerializationError = 2,
};

namespace internal {

// Implementation to be used for PersistentProto. Refer to persistent_proto.h
// for more details.
class PersistentProtoInternal
    : public base::ImportantFileWriter::DataSerializer {
 public:
  using ReadCallback = base::OnceCallback<void(ReadStatus)>;
  using WriteCallback = base::RepeatingCallback<void(WriteStatus)>;

  PersistentProtoInternal(const base::FilePath& path,
                          base::TimeDelta write_delay,
                          PersistentProtoInternal::ReadCallback on_read,
                          PersistentProtoInternal::WriteCallback on_write);

  PersistentProtoInternal(const PersistentProtoInternal&) = delete;
  PersistentProtoInternal& operator=(const PersistentProtoInternal&) = delete;

  ~PersistentProtoInternal() override;

  // Retrieves the underlying proto. Must never be null.
  virtual google::protobuf::MessageLite* GetProto() = 0;

  google::protobuf::MessageLite* get() { return proto_; }
  const google::protobuf::MessageLite* get() const { return proto_; }

  // Queues a write task on the current task runner.
  void QueueWrite();

  // Purges the proto by resetting |proto_| and triggering a write. If called
  // before |proto_| is ready, |proto_| will be purged once it becomes ready.
  void Purge();

  constexpr bool has_value() const { return proto_ != nullptr; }

  constexpr explicit operator bool() const { return has_value(); }

  const base::FilePath& path() { return proto_file_->path(); }

  // base::ImportantFileWriter::DataSerializer:
  std::optional<std::string> SerializeData() override;

  // Schedules a write to be executed immediately. Only to be used for tests.
  void StartWriteForTesting();

  // Updates the path of this persistent proto to a new file. The contents at
  // |path| will be merged with existing content of |proto_|. Optional fields
  // are overwritten and repeated fields are appended.
  // |on_read| is called once the read of the new path is complete.
  // |remove_existing| specifies if the existing file should be removed.
  void UpdatePath(const base::FilePath& path,
                  ReadCallback on_read,
                  bool remove_existing = false);

 protected:
  // Cleans up the in-memory proto.
  void DeallocProto();

 private:
  // Queues a task to delete the backing file.
  void QueueFileDelete();

  // Completes a write if there is a queued one.
  //
  // This is needed because it needs to be called by the class that owns the
  // proto. If this is called in PersistentProtoInternal dtor the owning proto
  // has already been destructed.
  void FlushQueuedWrites();

  // Serializes |proto_| to |write_buffer_|.
  void SerializeProtoForWrite();

  // Callback when the file has been loaded into a file.
  void OnReadComplete(ReadCallback callback,
                      base::expected<std::string, ReadStatus> read_status);

  // Called after |proto_file_| has attempted to write with the write status
  // captured in |write_successful|.
  void OnWriteAttempt(bool write_successful);

  // Called after OnWriteAttempt() or if the write was unsuccessful earlier.
  void OnWriteComplete(WriteStatus status);

  // Whether we should immediately clear the proto after reading it.
  bool purge_after_reading_ = false;

  // Run when the cache finishes writing to disk, if provided.
  WriteCallback on_write_;

  // Boolean to flag whether the path is being updated.
  //
  // If the path is being updated queuing a write needs to be blocked.
  std::atomic_bool updating_path_ = false;

  // Buffer to be used for flushing |proto_| contents into |proto_file_|. When
  // it is time to flush |proto_| into disk, a string copy will be stored in
  // |write_buffer_| to be flushed to avoid race conditions. The buffer will be
  // flushed when the write is complete.
  std::string write_buffer_;

  // The proto itself.
  raw_ptr<google::protobuf::MessageLite> proto_ = nullptr;

  // Task runner for reads and writes to be queued.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Persistence for |proto_|.
  std::unique_ptr<base::ImportantFileWriter> proto_file_;

  base::WeakPtrFactory<PersistentProtoInternal> weak_factory_{this};
};

}  // namespace internal
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_INTERNAL_H_
