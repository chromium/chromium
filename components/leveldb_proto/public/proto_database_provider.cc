// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/public/proto_database_provider.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "components/leveldb_proto/internal/shared_proto_database.h"

namespace leveldb_proto {

namespace {

const char kSharedProtoDatabaseClientName[] = "SharedProtoDB";
const char kSharedProtoDatabaseDirectory[] = "shared_proto_db";

}  // namespace

ProtoDatabaseProvider::ProtoDatabaseProvider(const base::FilePath& profile_dir,
                                             bool is_in_memory)
    : profile_dir_(profile_dir),
      is_in_memory_(is_in_memory),
      client_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

ProtoDatabaseProvider::~ProtoDatabaseProvider() {
  base::AutoLock lock(get_db_lock_);
  if (db_)
    db_->Shutdown();
}

void ProtoDatabaseProvider::GetSharedDBInstance(
    GetSharedDBInstanceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_task_runner) {
  {
    base::AutoLock lock(get_db_lock_);
    if (!db_) {
      db_ = base::WrapRefCounted(new SharedProtoDatabase(
          kSharedProtoDatabaseClientName, profile_dir_.AppendASCII(std::string(
                                              kSharedProtoDatabaseDirectory))));
    }
  }

  callback_task_runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), db_));
}

void ProtoDatabaseProvider::SetSharedDBDeleteObsoleteDelayForTesting(
    base::TimeDelta delay) {
  base::AutoLock lock(get_db_lock_);
  if (db_)
    db_->SetDeleteObsoleteDelayForTesting(delay);  // IN-TEST
}

}  // namespace leveldb_proto
