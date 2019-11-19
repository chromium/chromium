// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/internal/migration_delegate.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace leveldb_proto {

MigrationDelegate::MigrationDelegate() {}
MigrationDelegate::~MigrationDelegate() = default;

void MigrationDelegate::DoMigration(UniqueProtoDatabase* from,
                                    UniqueProtoDatabase* to,
                                    MigrationCallback callback) {
  from->LoadKeysAndEntries(base::BindOnce(
      &MigrationDelegate::OnLoadKeysAndEntries, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), base::Unretained(to)));
}

void MigrationDelegate::OnLoadKeysAndEntries(
    MigrationCallback callback,
    UniqueProtoDatabase* to,
    bool success,
    std::unique_ptr<KeyValueMap> keys_entries) {
  if (!success) {
    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
    current_task_runner->PostTask(FROM_HERE,
                                  base::BindOnce(std::move(callback), false));
    return;
  }

  // Convert the std::map we got back into a vector of std::pairs to be used
  // with UpdateEntries.
  auto kev = std::make_unique<KeyValueVector>();
  for (auto const& key_entry : *keys_entries)
    kev->push_back(key_entry);

  // Save the entries in |to|.
  to->UpdateEntries(
      std::move(kev), std::make_unique<KeyVector>(),
      base::BindOnce(&MigrationDelegate::OnUpdateEntries,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MigrationDelegate::OnUpdateEntries(MigrationCallback callback,
                                        bool success) {
  DCHECK(base::SequencedTaskRunnerHandle::IsSet());
  auto current_task_runner = base::SequencedTaskRunnerHandle::Get();
  current_task_runner->PostTask(FROM_HERE,
                                base::BindOnce(std::move(callback), success));
  // TODO (thildebr): For additional insurance, verify the entries match,
  // although they should if we got a success from UpdateEntries.
}

}  // namespace leveldb_proto
