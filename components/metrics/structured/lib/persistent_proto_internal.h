// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_INTERNAL_H_
#define COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_INTERNAL_H_

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

  // This function will be used to create an empty proto to populate or reset
  // the proto if Purge() is called.
  virtual std::unique_ptr<google::protobuf::MessageLite> BuildEmptyProto() = 0;

  google::protobuf::MessageLite* get() { return proto_.get(); }
  const google::protobuf::MessageLite* get() const { return proto_.get(); }

  // Queues a write task on the current task runner.
  void QueueWrite();

  // Purges the proto by resetting |proto_| and triggering a write. If called
  // before |proto_| is ready, |proto_| will be purged once it becomes ready.
  void Purge();

  constexpr bool has_value() const { return proto_.get() != nullptr; }

  constexpr explicit operator bool() const { return has_value(); }

  // base::ImportantFileWriter::DataSerializer:
  std::optional<std::string> SerializeData() override;

  // Schedules a write to be executed immediately. Only to be used for tests.
  void StartWriteForTesting();

 private:
  // Callback when the file has been loaded into a file.
  void OnReadComplete(base::expected<std::string, ReadStatus> read_status);

  // Called after |proto_file_| has attempted to write with the write status
  // captured in |write_successful|.
  void OnWriteAttempt(bool write_successful);

  // Called after OnWriteAttempt() or if the write was unsuccessful earlier.
  void OnWriteComplete(WriteStatus status);

  // Whether we should immediately clear the proto after reading it.
  bool purge_after_reading_ = false;

  // Run when the cache finishes reading from disk, if provided.
  ReadCallback on_read_;

  // Run when the cache finishes writing to disk, if provided.
  WriteCallback on_write_;

  // The proto itself.
  std::unique_ptr<google::protobuf::MessageLite> proto_;

  // Task runner for reads and writes to be queued.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Persistence for |proto_|.
  base::ImportantFileWriter proto_file_;

  base::WeakPtrFactory<PersistentProtoInternal> weak_factory_{this};
};

}  // namespace internal
}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_LIB_PERSISTENT_PROTO_INTERNAL_H_
