// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_PENDING_FILE_TRANSFER_QUEUE_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_PENDING_FILE_TRANSFER_QUEUE_H_

#include <cstdint>
#include <queue>

#include "base/functional/callback.h"

namespace data_migration {

// A queue of files (identified by their payload id) that the remote device
// wants to transfer but hasn't yet. The remote device has sent a
// "Request-To-Send" message for each file in this queue and is awaiting a
// "Clear-To-Send" message to be sent back before the transfer actually can
// commence. "Clear-To-Send" is sent after a pending file is popped from the
// queue and registered with the NC library.
class PendingFileTransferQueue {
 public:
  PendingFileTransferQueue();
  PendingFileTransferQueue(const PendingFileTransferQueue&) = delete;
  PendingFileTransferQueue& operator=(const PendingFileTransferQueue&) = delete;
  ~PendingFileTransferQueue();

  // Adds file with the given `payload_id` to the queue.
  void Push(int64_t payload_id);

  // Pops a pending file from the queue, returning the file's `payload_id` to
  // the caller. The `completion_cb` may be run synchronously in some cases if
  // there are pending files already waiting in the queue when this method is
  // called. There can only be one active `Pop()` call at any given time.
  void Pop(base::OnceCallback<void(int64_t)> completion_cb);

 private:
  void RunPendingPopCallback();

  std::queue<int64_t> pending_payload_ids_;
  base::OnceCallback<void(int64_t)> pending_pop_cb_;
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_PENDING_FILE_TRANSFER_QUEUE_H_
