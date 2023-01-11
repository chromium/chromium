// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/shared_proto_database_provider.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace leveldb_proto {

SharedProtoDatabaseProvider::SharedProtoDatabaseProvider(
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    base::WeakPtr<ProtoDatabaseProvider> provider_weak_ptr)
    : client_task_runner_(client_task_runner),
      provider_weak_ptr_(std::move(provider_weak_ptr)) {}

SharedProtoDatabaseProvider::~SharedProtoDatabaseProvider() = default;

void SharedProtoDatabaseProvider::GetDBInstance(
    GetSharedDBInstanceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ProtoDatabaseProvider::GetSharedDBInstance,
                                provider_weak_ptr_, std::move(callback),
                                std::move(callback_task_runner)));
}

}  // namespace leveldb_proto
