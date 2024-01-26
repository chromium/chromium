// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/persistent_proto.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/metrics/structured/histogram_util.h"
#include "components/metrics/structured/lib/proto/key.pb.h"
#include "components/metrics/structured/proto/event_storage.pb.h"

namespace metrics::structured {
namespace {

template <class T>
// Attempts to read from |filepath| and returns a string with the file content
// if successful.
base::expected<std::unique_ptr<T>, ReadStatus> Read(
    const base::FilePath& filepath) {
  if (!base::PathExists(filepath)) {
    return base::unexpected(ReadStatus::kMissing);
  }

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str)) {
    return base::unexpected(ReadStatus::kReadError);
  }

  auto proto = std::make_unique<T>();
  if (!proto->ParseFromString(proto_str)) {
    return base::unexpected(ReadStatus::kParseError);
  }

  return base::ok(std::move(proto));
}

}  // namespace

template <class T>
PersistentProto<T>::PersistentProto(
    const base::FilePath& path,
    const base::TimeDelta write_delay,
    typename PersistentProto<T>::ReadCallback on_read,
    typename PersistentProto<T>::WriteCallback on_write)
    : on_read_(std::move(on_read)),
      on_write_(std::move(on_write)),
      task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      proto_file_(
          base::ImportantFileWriter(path,
                                    task_runner_,
                                    write_delay,
                                    "StructuredMetricsPersistentProto")) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&Read<T>, proto_file_.path()),
      base::BindOnce(&PersistentProto<T>::OnReadComplete,
                     weak_factory_.GetWeakPtr()));
}

template <class T>
PersistentProto<T>::~PersistentProto() {
  // Flush any existing writes that are scheduled.
  if (proto_file_.HasPendingWrite()) {
    proto_file_.DoScheduledWrite();
  }
}

template <class T>
void PersistentProto<T>::OnReadComplete(
    base::expected<std::unique_ptr<T>, ReadStatus> read_status) {
  ReadStatus status;

  if (read_status.has_value()) {
    status = ReadStatus::kOk;
    proto_ = std::move(read_status.value());
  } else {
    // If there was an error, write an empty proto.
    status = read_status.error();
    proto_ = std::make_unique<T>();
    QueueWrite();
  }

  // Purge the read proto if |purge_after_reading_|.
  if (purge_after_reading_) {
    proto_.reset();
    proto_ = std::make_unique<T>();
    QueueWrite();
    purge_after_reading_ = false;
  }

  std::move(on_read_).Run(std::move(status));
}

template <class T>
void PersistentProto<T>::QueueWrite() {
  // |proto_| will be null if OnReadComplete() has not finished executing. It is
  // up to the user to verify that OnReadComplete() has finished with callback
  // |on_read_| before calling QueueWrite().
  CHECK(proto_);
  proto_file_.ScheduleWrite(this);
}

template <class T>
void PersistentProto<T>::OnWriteAttempt(bool write_successful) {
  if (write_successful) {
    OnWriteComplete(WriteStatus::kOk);
  } else {
    OnWriteComplete(WriteStatus::kWriteError);
  }
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
    QueueWrite();
  } else {
    purge_after_reading_ = true;
  }
}

template <class T>
std::optional<std::string> PersistentProto<T>::SerializeData() {
  std::string proto_str;
  if (!proto_->SerializeToString(&proto_str)) {
    OnWriteComplete(WriteStatus::kSerializationError);
    return std::nullopt;
  }
  proto_file_.RegisterOnNextWriteCallbacks(
      base::BindOnce(base::IgnoreResult(&base::CreateDirectory),
                     proto_file_.path().DirName()),
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&PersistentProto<T>::OnWriteAttempt,
                                        weak_factory_.GetWeakPtr())));
  return proto_str;
}

template <class T>
void PersistentProto<T>::StartWriteForTesting() {
  proto_file_.ScheduleWrite(this);
  proto_file_.DoScheduledWrite();
}

// A list of all types that the PersistentProto can be used with.
template class PersistentProto<EventsProto>;
template class PersistentProto<KeyDataProto>;
template class PersistentProto<KeyProto>;

}  // namespace metrics::structured
