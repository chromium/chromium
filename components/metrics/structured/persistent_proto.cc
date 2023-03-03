// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/persistent_proto.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/storage.pb.h"

namespace metrics {
namespace structured {
namespace {

template <class T>
std::pair<ReadStatus, std::unique_ptr<T>> Read(const base::FilePath& filepath) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(filepath)) {
    return {ReadStatus::kMissing, nullptr};
  }

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str)) {
    return {ReadStatus::kReadError, nullptr};
  }

  auto proto = std::make_unique<T>();
  if (!proto->ParseFromString(proto_str)) {
    return {ReadStatus::kParseError, nullptr};
  }

  return {ReadStatus::kOk, std::move(proto)};
}

WriteStatus Write(const base::FilePath& filepath,
                  const std::string& proto_str) {
  const auto directory = filepath.DirName();
  if (!base::DirectoryExists(directory)) {
    base::CreateDirectory(directory);
  }

  bool write_result;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        filepath, proto_str, "StructuredMetricsPersistentProto");
  }

  if (!write_result) {
    return WriteStatus::kWriteError;
  }
  return WriteStatus::kOk;
}

}  // namespace

template <class T>
PersistentProto<T>::PersistentProto(
    const base::FilePath& path,
    const base::TimeDelta write_delay,
    typename PersistentProto<T>::ReadCallback on_read,
    typename PersistentProto<T>::WriteCallback on_write)
    : path_(path),
      write_delay_(write_delay),
      on_read_(std::move(on_read)),
      on_write_(std::move(on_write)) {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&Read<T>, path_),
      base::BindOnce(&PersistentProto<T>::OnReadComplete,
                     weak_factory_.GetWeakPtr()));
}

template <class T>
PersistentProto<T>::~PersistentProto() {
  if (has_value()) {
    std::string proto_str;
    if (!proto_->SerializeToString(&proto_str)) {
      OnWriteComplete(WriteStatus::kSerializationError);
    }
    Write(path_, proto_str);
  }
}

template <class T>
void PersistentProto<T>::OnReadComplete(
    std::pair<ReadStatus, std::unique_ptr<T>> result) {
  if (result.first == ReadStatus::kOk) {
    proto_ = std::move(result.second);
  } else {
    proto_ = std::make_unique<T>();
    QueueWrite();
  }

  if (purge_after_reading_) {
    proto_.reset();
    proto_ = std::make_unique<T>();
    StartWrite();
    purge_after_reading_ = false;
  }

  std::move(on_read_).Run(result.first);
}

template <class T>
void PersistentProto<T>::QueueWrite() {
  DCHECK(proto_);
  if (!proto_) {
    return;
  }

  // If a save is already queued, do nothing.
  if (write_is_queued_) {
    return;
  }
  write_is_queued_ = true;

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PersistentProto<T>::OnQueueWrite,
                     weak_factory_.GetWeakPtr()),
      write_delay_);
}

template <class T>
void PersistentProto<T>::OnQueueWrite() {
  // Reset the queued flag before posting the task. Last-moment updates to
  // |proto_| will post another task to write the proto, avoiding race
  // conditions.
  write_is_queued_ = false;
  StartWrite();
}

template <class T>
void PersistentProto<T>::StartWrite() {
  DCHECK(proto_);
  if (!proto_) {
    return;
  }

  // Serialize the proto outside of the posted task, because otherwise we need
  // to pass a proto pointer into the task. This causes a rare race condition
  // during destruction where the proto can be destroyed before serialization,
  // causing a crash.
  std::string proto_str;
  if (!proto_->SerializeToString(&proto_str)) {
    OnWriteComplete(WriteStatus::kSerializationError);
  }

  // The SequentialTaskRunner ensures the writes won't trip over each other, so
  // we can schedule without checking whether another write is currently active.
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&Write, path_, proto_str),
      base::BindOnce(&PersistentProto<T>::OnWriteComplete,
                     weak_factory_.GetWeakPtr()));
}

template <class T>
void PersistentProto<T>::OnWriteComplete(const WriteStatus status) {
  on_write_.Run(status);
}

template <class T>
void PersistentProto<T>::Purge() {
  if (proto_) {
    proto_.reset();
    proto_ = std::make_unique<T>();
    StartWrite();
  } else {
    purge_after_reading_ = true;
  }
}

// A list of all types that the PersistentProto can be used with.
template class PersistentProto<EventsProto>;
template class PersistentProto<KeyDataProto>;
template class PersistentProto<KeyProto>;

}  // namespace structured
}  // namespace metrics
