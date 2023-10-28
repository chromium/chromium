// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_PERSISTENT_PROTO_H_
#define COMPONENTS_METRICS_STRUCTURED_PERSISTENT_PROTO_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

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

// PersistentProto wraps a proto class and persists it to disk. Usage summary.
//  - Init is asynchronous, usage before |on_read| is called will crash.
//  - pproto->Method() will call Method on the underlying proto.
//  - Call QueueWrite() to write to disk.
//
// Reading. The backing file is read asynchronously from disk once at
// initialization, and the |on_read| callback is run once this is done. Until
// |on_read| is called, has_value is false and get() will always return nullptr.
// If no proto file exists on disk, or it is invalid, a blank proto is
// constructed and immediately written to disk.
//
// Writing. Writes must be triggered manually. Two methods are available:
//  - QueueWrite() delays writing to disk for |write_delay| time, in order to
//    batch successive writes.
//  - StartWrite() writes to disk as soon as the task scheduler allows.
// The |on_write| callback is run each time a write has completed.
//
// WARNING. Every proto this class can be used with needs to be listed at the
// bottom of the cc file.
template <class T>
class PersistentProto {
 public:
  using ReadCallback = base::OnceCallback<void(ReadStatus)>;
  using WriteCallback = base::RepeatingCallback<void(WriteStatus)>;

  PersistentProto(const base::FilePath& path,
                  base::TimeDelta write_delay,
                  typename PersistentProto<T>::ReadCallback on_read,
                  typename PersistentProto<T>::WriteCallback on_write);
  ~PersistentProto();

  PersistentProto(const PersistentProto&) = delete;
  PersistentProto& operator=(const PersistentProto&) = delete;

  T* get() { return proto_.get(); }

  T* operator->() {
    CHECK(proto_);
    return proto_.get();
  }

  T operator*() {
    CHECK(proto_);
    return *proto_;
  }

  constexpr bool has_value() const { return proto_.get() != nullptr; }

  constexpr explicit operator bool() const { return has_value(); }

  // Write the backing proto to disk after |save_delay_ms_| has elapsed.
  void QueueWrite();

  // Write the backing proto to disk 'now'.
  void StartWrite();

  // Safely clear this proto from memory and disk. This is preferred to clearing
  // the proto, because it ensures the proto is purged even if called before the
  // backing file is read from disk. In this case, the file is overwritten after
  // it has been read. In either case, the file is written as soon as possible,
  // skipping the |save_delay_ms_| wait time.
  void Purge();

 private:
  void OnReadComplete(std::pair<ReadStatus, std::unique_ptr<T>> result);
  void OnWriteComplete(WriteStatus status);
  void OnQueueWrite();

  // Path on disk to read from and write to.
  const base::FilePath path_;

  // How long to delay writing to disk for on a call to QueueWrite.
  const base::TimeDelta write_delay_;

  // Whether or not a write is currently scheduled.
  bool write_is_queued_ = false;

  // Whether we should immediately clear the proto after reading it.
  bool purge_after_reading_ = false;

  // Run when the cache finishes reading from disk, if provided.
  ReadCallback on_read_;

  // Run when the cache finishes writing to disk, if provided.
  WriteCallback on_write_;

  // The proto itself.
  std::unique_ptr<T> proto_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<PersistentProto> weak_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_PERSISTENT_PROTO_H_
