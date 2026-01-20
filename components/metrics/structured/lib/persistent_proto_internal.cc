// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/persistent_proto_internal.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace metrics::structured::internal {

namespace {

// Attempts to read from |filepath| and returns a string with the file content
// if successful.
base::expected<std::string, ReadStatus> Read(const base::FilePath& filepath) {
  if (!base::PathExists(filepath)) {
    return base::unexpected(ReadStatus::kMissing);
  }

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str)) {
    return base::unexpected(ReadStatus::kReadError);
  }

  return base::ok(std::move(proto_str));
}

}  // namespace

PersistentProtoInternal::PersistentProtoInternal(
    const base::FilePath& path,
    base::TimeDelta write_delay,
    PersistentProtoInternal::ReadCallback on_read,
    PersistentProtoInternal::WriteCallback on_write)
    : on_write_(std::move(on_write)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      proto_file_(std::make_unique<base::ImportantFileWriter>(
          path,
          task_runner_,
          write_delay,
          "StructuredMetricsPersistentProto")) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&Read, proto_file_->path()),
      base::BindOnce(&PersistentProtoInternal::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(on_read)));
}

PersistentProtoInternal::~PersistentProtoInternal() = default;

void PersistentProtoInternal::OnReadComplete(
    ReadCallback callback,
    base::expected<std::string, ReadStatus> read_status) {
  ReadStatus status;

  // If the path was updated then we may not need to update the pointer.
  if (proto_ == nullptr) {
    proto_ = GetProto();
  }

  if (read_status.has_value()) {
    status = ReadStatus::kOk;

    // Parses the value of |read_status| into |proto_| but attempts to preserve
    // any existing content. Optional values will be overwritten and repeated
    // fields will be appended with new values.
    if (!proto_->MergeFromString(read_status.value())) {
      status = ReadStatus::kParseError;
      QueueWrite();
    }
  } else {
    status = read_status.error();
  }

  // If there was an error, write an empty proto.
  if (status != ReadStatus::kOk) {
    QueueWrite();
  }

  // Purge the read proto if |purge_after_reading_|.
  if (purge_after_reading_) {
    Purge();
    purge_after_reading_ = false;
  }

  std::move(callback).Run(std::move(status));
}

void PersistentProtoInternal::QueueWrite() {
  // Read |updating_path_| to check if we are actively updating the path of this
  // proto.
  if (updating_path_.load()) {
    return;
  }

  // |proto_| will be null if OnReadComplete() has not finished executing. It is
  // up to the user to verify that OnReadComplete() has finished with callback
  // |on_read_| before calling QueueWrite().
  CHECK(proto_);

  // Serialize the proto into a buffer for write to occur. Because the IO
  // happens on a separate sequence, serialization must happen on this sequence
  // since PersistentProto is not thread-safe.
  SerializeProtoForWrite();

  proto_file_->ScheduleWrite(this);
}

void PersistentProtoInternal::OnWriteAttempt(bool write_successful) {
  write_buffer_.clear();

  if (write_successful) {
    OnWriteComplete(WriteStatus::kOk);
  } else {
    OnWriteComplete(WriteStatus::kWriteError);
  }
}

void PersistentProtoInternal::OnWriteComplete(const WriteStatus status) {
  on_write_.Run(status);
}

void PersistentProtoInternal::Purge() {
  if (proto_) {
    proto_->Clear();
    QueueFileDelete();
  } else {
    purge_after_reading_ = true;
  }
}

std::optional<std::string> PersistentProtoInternal::SerializeData() {
  proto_file_->RegisterOnNextWriteCallbacks(
      base::BindOnce(base::IgnoreResult(&base::CreateDirectory),
                     proto_file_->path().DirName()),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&PersistentProtoInternal::OnWriteAttempt,
                         weak_factory_.GetWeakPtr())));
  return write_buffer_;
}

void PersistentProtoInternal::StartWriteForTesting() {
  SerializeProtoForWrite();

  proto_file_->ScheduleWrite(this);
  proto_file_->DoScheduledWrite();
}

void PersistentProtoInternal::UpdatePath(const base::FilePath& path,
                                         ReadCallback on_read,
                                         bool remove_existing) {
  updating_path_.store(true);

  // Clean up the state of the current |proto_file_|.
  FlushQueuedWrites();

  // If the previous file should be cleaned up then schedule the cleanup on
  // separate thread.
  if (remove_existing) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                          proto_file_->path()));
  }

  // Overwrite the ImportantFileWriter with a new one at the new path.
  proto_file_ = std::make_unique<base::ImportantFileWriter>(
      path, task_runner_, proto_file_->commit_interval(),
      "StructuredMetricsPersistentProto");

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&Read, proto_file_->path()),
      base::BindOnce(&PersistentProtoInternal::OnReadComplete,
                     weak_factory_.GetWeakPtr(), std::move(on_read)));

  updating_path_.store(false);

  // Write the content of the proto back to the path in case it has changed. If
  // an error occurs while reading |path| then 2 write can occur.
  //
  // It is possible in tests that the profile is added before the pre-profile
  // events have been loaded, which initializes the proto. In this case, we do
  // not want to queue a write.
  if (proto_) {
    QueueWrite();
  }
}

void PersistentProtoInternal::DeallocProto() {
  FlushQueuedWrites();
  proto_ = nullptr;
}

void PersistentProtoInternal::QueueFileDelete() {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                        proto_file_->path()));
}

void PersistentProtoInternal::FlushQueuedWrites() {
  if (proto_file_->HasPendingWrite()) {
    proto_file_->DoScheduledWrite();
  }
}

void PersistentProtoInternal::SerializeProtoForWrite() {
  // If the write buffer is not empty, it means that a write is already in
  // progress.
  if (!write_buffer_.empty()) {
    return;
  }

  if (!proto_->SerializeToString(&write_buffer_)) {
    write_buffer_.clear();
    OnWriteComplete(WriteStatus::kSerializationError);
    return;
  }
}

}  // namespace metrics::structured::internal
