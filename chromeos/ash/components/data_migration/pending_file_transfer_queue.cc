// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/pending_file_transfer_queue.h"

#include <utility>

#include "base/check.h"

namespace data_migration {

PendingFileTransferQueue::PendingFileTransferQueue() = default;

PendingFileTransferQueue::~PendingFileTransferQueue() = default;

void PendingFileTransferQueue::Push(int64_t payload_id) {
  // TODO(esum): Add a maximum size to `pending_payload_ids_` to prevent it from
  // consuming an indefinite amount of memory.
  pending_payload_ids_.push(payload_id);
  if (pending_pop_cb_) {
    RunPendingPopCallback();
  }
}

void PendingFileTransferQueue::Pop(
    base::OnceCallback<void(int64_t)> completion_cb) {
  CHECK(completion_cb);
  CHECK(!pending_pop_cb_);
  pending_pop_cb_ = std::move(completion_cb);
  if (!pending_payload_ids_.empty()) {
    RunPendingPopCallback();
  }
}

void PendingFileTransferQueue::RunPendingPopCallback() {
  CHECK(pending_pop_cb_);
  int64_t popped_payload_id = pending_payload_ids_.front();
  pending_payload_ids_.pop();
  std::move(pending_pop_cb_).Run(popped_payload_id);
}

}  // namespace data_migration
