// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_UTIL_PERSISTENT_PROTO_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_UTIL_PERSISTENT_PROTO_H_

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"

namespace app_list {
namespace {

template <class T>
class MessageStorage;

template <class T>
class RootMessageStorage;

template <class T>
class EmbeddedMessageStorage;

}  // namespace

// API ------------------------------------------------------------------------

// The result of reading a backing file from disk. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class ReadStatus {
  kOk = 0,
  kMissing = 1,
  kReadError = 2,
  kParseError = 3,
  // Provided when an embedded message completes a no-op 'read'.
  kNoop = 4,
  kMaxValue = kNoop,
};

// The result of writing a backing file to disk. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class WriteStatus {
  kOk = 0,
  kWriteError = 1,
  kSerializationError = 2,
  kMaxValue = kSerializationError,
};

using PersistentProtoReadCallback = base::OnceCallback<void(ReadStatus)>;
using PersistentProtoWriteCallback = base::RepeatingCallback<void(WriteStatus)>;

// PersistentProto wraps a proto class and persists it to disk. Usage summary:
//
//  - PProtos are constructed with a file path and a write delay. No logic
//    happens at construction, instead Init must be called.
//
//  - Callbacks registered with RegisterOnRead and RegisterOnWrite will be
//    called when appropriate. It is recommended to register these before
//    calling Init.
//
//  - Init is asynchronous, and usage before RegisterOnRead callbacks are
//    called will crash.
//
//  - After initialization, a PProto acts identically to a regular proto. Use
//    my_pproto->method() to access methods.
//
//  - Call QueueWrite or StartWrite to write to disk.
//
// READING
// The backing file is read asynchronously from disk once at initialization,
// and RegisterOnRead callbacks are run once this is done. Until they are
// called, has_value is false and get() will always return nullptr. If no proto
// file exists on disk, or it is invalid, a blank proto is constructed and
// immediately written to disk.
//
// WRITING
// Writes must be triggered manually. Two methods are available:
//  - QueueWrite delays writing to disk for |write_delay| time, in order to
//    batch successive writes.
//  - StartWrite writes to disk as soon as the task scheduler allows.
// RegisterOnWrite callbacks are run each time a write has completed.
//
// EMBEDDED MESSAGES
// To pass around a sub-message of a PProto that can do writes, use the Wrap
// method as follows.
//
//   PersistentProto<MyMessage> my_message =
//       pproto.Wrap(pproto->mutable_my_message());
//
// These are a drop-in replacement for a regular PProto, except that:
//
//  - Calling Init is not necessary, because a Wrap'd PProto must have been
//    created from an existing, initialized PProto. If Init is called, it
//    immediately calls all RegisterOnRead callbacks and is otherwise a
//    no-op.
//
//  - The root PProto and embedded message PProtos form a tree. Writes called
//    on any node of the tree trickle up to the root, and cause a write of the
//    entire tree. This will trigger write callbacks that were registered from
//    anywhere in the tree. Take care that the data in the embedded message is
//    safe to be serialized at any moment.
//
// Warning: The top-level PProto object must outlive all PProtos generated with
// Wrap.
//
// Warning: Because a Wrap'd PProto object only retains a pointer to a message,
// reassigning that field in the parent PProto will silently invalidate it.
// This includes calling Purge.
//
// Warning: It is possible to Wrap a message that is part of another proto, eg.
// pproto_1.Wrap(proto_2->...). Don't do this!
template <class T>
class PersistentProto {
 public:
  PersistentProto<T>(const base::FilePath& path,
                     const base::TimeDelta write_delay)
      : storage_(std::make_unique<RootMessageStorage<T>>(path, write_delay)) {}
  ~PersistentProto<T>() {}

  PersistentProto(PersistentProto&& other) {
    storage_ = std::move(other.storage_);
  }

  PersistentProto& operator=(PersistentProto&& other) {
    storage_ = std::move(other.storage_);
  }

  PersistentProto(const PersistentProto&) = delete;
  PersistentProto& operator=(const PersistentProto&) = delete;

  template <class U>
  PersistentProto<U> Wrap(U* message) {
    DCHECK(initialized());
    return PersistentProto<U>(
        std::make_unique<EmbeddedMessageStorage<U>>(storage_.get(), message));
  }

  // Initialization.
  void Init() { storage_->Init(); }
  void RegisterOnRead(PersistentProtoReadCallback on_read) {
    storage_->RegisterOnRead(std::move(on_read));
  }
  void RegisterOnWrite(PersistentProtoWriteCallback on_write) {
    storage_->RegisterOnWrite(std::move(on_write));
  }

  // Getters.
  T* get() { return storage_->get(); }
  T* operator->() { return storage_->get(); }
  const T* operator->() const { return storage_->get(); }
  T operator*() { return *(storage_->get()); }

  // Testing initialization and value.
  bool initialized() const { return storage_ && storage_->initialized(); }
  constexpr bool has_value() const { return storage_ && storage_->has_value(); }
  constexpr explicit operator bool() const { return has_value(); }

  // Writing and deletion.
  void QueueWrite() { storage_->QueueWrite(); }
  void StartWrite() { storage_->StartWrite(); }
  void Purge() { storage_->Purge(); }

 private:
  template <class U>
  friend class PersistentProto;

  PersistentProto<T>(std::unique_ptr<MessageStorage<T>> storage)
      : storage_(std::move(storage)) {}

  std::unique_ptr<MessageStorage<T>> storage_;
};

namespace {

template <class T>
std::pair<ReadStatus, std::unique_ptr<T>> Read(const base::FilePath& filepath) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (!base::PathExists(filepath))
    return {ReadStatus::kMissing, nullptr};

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str))
    return {ReadStatus::kReadError, nullptr};

  auto proto = std::make_unique<T>();
  if (!proto->ParseFromString(proto_str))
    return {ReadStatus::kParseError, nullptr};

  return {ReadStatus::kOk, std::move(proto)};
}

WriteStatus Write(const base::FilePath& filepath,
                  const std::string& proto_str) {
  const auto directory = filepath.DirName();
  if (!base::DirectoryExists(directory))
    base::CreateDirectory(directory);

  bool write_result;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        filepath, proto_str, "AppListPersistentProto");
  }

  if (!write_result)
    return WriteStatus::kWriteError;
  return WriteStatus::kOk;
}

// STORAGE INTERFACES ----------------------------------------------------------

// All methods that don't require knowing the underlying proto's type.
class GenericMessageStorage {
 public:
  virtual bool initialized() const = 0;
  virtual constexpr bool has_value() const = 0;
  virtual constexpr explicit operator bool() const = 0;

  virtual void RegisterOnRead(PersistentProtoReadCallback on_read) = 0;
  virtual void RegisterOnWrite(PersistentProtoWriteCallback on_write) = 0;

  virtual void Init() = 0;
  virtual void QueueWrite() = 0;
  virtual void StartWrite() = 0;
  virtual void Purge() = 0;
};

// Storage base class that abstracts over both root protos and embedded
// messages.
template <class T>
class MessageStorage : public GenericMessageStorage {
 public:
  MessageStorage() {}
  virtual ~MessageStorage() {}

  MessageStorage(const MessageStorage&) = delete;
  MessageStorage& operator=(const MessageStorage&) = delete;

  virtual T* get() = 0;
  virtual T* operator->() = 0;
  virtual const T* operator->() const = 0;
  virtual T operator*() = 0;
};

// ROOT MESSAGE STORAGE --------------------------------------------------------

// Storage for a top-level proto. This actually controls reading and writing.
template <class T>
class RootMessageStorage : public MessageStorage<T> {
 public:
  RootMessageStorage(const base::FilePath& path,
                     const base::TimeDelta write_delay)
      : path_(path),
        write_delay_(write_delay),
        task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

  ~RootMessageStorage() override {}

  void Init() override {
    DCHECK(!initialized_);
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&Read<T>, path_),
        base::BindOnce(&RootMessageStorage<T>::OnReadComplete,
                       weak_factory_.GetWeakPtr()));
  }

  void RegisterOnRead(PersistentProtoReadCallback on_read) override {
    read_callbacks_.push_back(std::move(on_read));
  }

  void RegisterOnWrite(PersistentProtoWriteCallback on_write) override {
    write_callbacks_.push_back(std::move(on_write));
  }

  T* get() override { return proto_.get(); }

  T* operator->() override {
    CHECK(proto_);
    return proto_.get();
  }

  const T* operator->() const override {
    CHECK(proto_);
    return proto_.get();
  }

  T operator*() override {
    CHECK(proto_);
    return *proto_;
  }

  bool initialized() const override { return initialized_; }

  constexpr bool has_value() const override { return proto_.get() != nullptr; }

  constexpr explicit operator bool() const override { return has_value(); }

  // Write the backing proto to disk after |save_delay_ms_| has elapsed.
  void QueueWrite() override {
    DCHECK(proto_);
    if (!proto_)
      return;

    // If a save is already queued, do nothing.
    if (write_is_queued_)
      return;
    write_is_queued_ = true;

    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RootMessageStorage<T>::OnQueueWrite,
                       weak_factory_.GetWeakPtr()),
        write_delay_);
  }

  // Write the backing proto to disk 'now'.
  void StartWrite() override {
    DCHECK(proto_);
    if (!proto_)
      return;

    // Serialize the proto outside of the posted task, because otherwise we need
    // to pass a proto pointer into the task. This causes a rare race condition
    // during destruction where the proto can be destroyed before serialization,
    // causing a crash.
    std::string proto_str;
    if (!proto_->SerializeToString(&proto_str))
      OnWriteComplete(WriteStatus::kSerializationError);

    // The SequentialTaskRunner ensures the writes won't trip over each other,
    // so we can schedule without checking whether another write is currently
    // active.
    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&Write, path_, proto_str),
        base::BindOnce(&RootMessageStorage<T>::OnWriteComplete,
                       weak_factory_.GetWeakPtr()));
  }

  // Safely clear this proto from memory and disk. This is preferred to clearing
  // the proto, because it ensures the proto is purged even if called before the
  // backing file is read from disk. In this case, the file is overwritten after
  // it has been read. In either case, the file is written as soon as possible,
  // skipping the |save_delay_ms_| wait time.
  void Purge() override {
    if (proto_) {
      proto_.reset();
      proto_ = std::make_unique<T>();
      StartWrite();
    } else {
      purge_after_reading_ = true;
    }
  }

 private:
  void OnReadComplete(std::pair<ReadStatus, std::unique_ptr<T>> result) {
    base::UmaHistogramEnumeration("Apps.AppList.PersistentProto.ReadStatus",
                                  result.first);
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

    initialized_ = true;
    for (auto& cb : read_callbacks_) {
      std::move(cb).Run(result.first);
    }
  }

  void OnWriteComplete(const WriteStatus status) {
    base::UmaHistogramEnumeration("Apps.AppList.PersistentProto.WriteStatus",
                                  status);
    for (auto& cb : write_callbacks_) {
      cb.Run(status);
    }
  }

  void OnQueueWrite() {
    // Reset the queued flag before posting the task. Last-moment updates to
    // |proto_| will post another task to write the proto, avoiding race
    // conditions.
    write_is_queued_ = false;
    StartWrite();
  }

  // Path on disk to read from and write to.
  base::FilePath path_;

  // How long to delay writing to disk for on a call to QueueWrite.
  base::TimeDelta write_delay_;

  // Whether the proto has finished reading from disk. |proto_| will be empty
  // before |initialized_| is true.
  bool initialized_ = false;

  // Whether or not a write is currently scheduled.
  bool write_is_queued_ = false;

  // Whether we should immediately clear the proto after reading it.
  bool purge_after_reading_ = false;

  // Run when the cache finishes reading from disk.
  std::vector<PersistentProtoReadCallback> read_callbacks_;

  // Run when the cache finishes writing to disk.
  std::vector<PersistentProtoWriteCallback> write_callbacks_;

  // The proto itself.
  std::unique_ptr<T> proto_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<RootMessageStorage> weak_factory_{this};
};

// EMBEDDED MESSAGE STORAGE ----------------------------------------------------

// Storage for an embedded message, which delegates writes to another storage
// object.
template <class T>
class EmbeddedMessageStorage : public MessageStorage<T> {
 public:
  EmbeddedMessageStorage(GenericMessageStorage* storage, T* message)
      : storage_(storage), message_(message) {}

  ~EmbeddedMessageStorage() override {}

  void Init() override {
    // Initialization is a no-op for embedded messages because, to be
    // constructed, the root message must have already been initialized. So
    // immediately call all read callbacks.
    for (auto& cb : read_callbacks_) {
      std::move(cb).Run(ReadStatus::kNoop);
    }
  }

  void RegisterOnRead(PersistentProtoReadCallback on_read) override {
    read_callbacks_.push_back(std::move(on_read));
  }

  void RegisterOnWrite(PersistentProtoWriteCallback on_write) override {
    // Delegate calling the write callback to the storage.
    storage_->RegisterOnWrite(std::move(on_write));
  }

  T* get() override { return message_; }
  T* operator->() override { return message_; }
  const T* operator->() const override { return message_; }
  T operator*() override { return *message_; }

  // For this have been constructed, the parent proto must have been initialized
  // and have a value. So all init-querying methods can trivially return true.
  bool initialized() const override { return true; }
  constexpr bool has_value() const override { return true; }
  constexpr explicit operator bool() const override { return true; }

  void QueueWrite() override { storage_->QueueWrite(); }
  void StartWrite() override { storage_->StartWrite(); }

  void Purge() override {
    message_->Clear();
    StartWrite();
  }

 private:
  GenericMessageStorage* storage_;
  T* message_;

  // All callbacks passed to |RegisterOnRead|.
  std::vector<PersistentProtoReadCallback> read_callbacks_;
};

}  // namespace
}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_UTIL_PERSISTENT_PROTO_H_
