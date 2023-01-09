// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/proto_database_impl.h"

#include <memory>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace leveldb_proto {

void RunUpdateCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::UpdateCallback callback,
    bool success) {
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), success));
}

void RunLoadKeysCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::LoadKeysCallback callback,
    bool success,
    std::unique_ptr<KeyVector> keys) {
  callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success, std::move(keys)));
}

void RunDestroyCallback(
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner,
    Callbacks::DestroyCallback callback,
    bool success) {
  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), success));
}
}  // namespace leveldb_proto
