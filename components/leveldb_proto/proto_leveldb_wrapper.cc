// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_leveldb_wrapper.h"

namespace leveldb_proto {

namespace {

void RunInitCallback(typename ProtoLevelDBWrapper::InitCallback callback,
                     const bool* success) {
  std::move(callback).Run(*success);
}

inline void InitFromTaskRunner(LevelDB* database,
                               const base::FilePath& database_dir,
                               const leveldb_env::Options& options,
                               bool* success) {
  DCHECK(success);

  // TODO(cjhopman): Histogram for database size.
  *success = database->Init(database_dir, options);
}

void RunDestroyCallback(typename ProtoLevelDBWrapper::DestroyCallback callback,
                        const bool* success) {
  std::move(callback).Run(*success);
}

inline void DestroyFromTaskRunner(LevelDB* database, bool* success) {
  CHECK(success);

  *success = database->Destroy();
}

void RunLoadKeysCallback(
    typename ProtoLevelDBWrapper::LoadKeysCallback callback,
    std::unique_ptr<bool> success,
    std::unique_ptr<std::vector<std::string>> keys) {
  std::move(callback).Run(*success, std::move(keys));
}

inline void LoadKeysFromTaskRunner(LevelDB* database,
                                   std::vector<std::string>* keys,
                                   bool* success) {
  DCHECK(success);
  DCHECK(keys);
  keys->clear();
  *success = database->LoadKeys(keys);
}

}  // namespace

ProtoLevelDBWrapper::ProtoLevelDBWrapper(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : task_runner_(task_runner) {}

ProtoLevelDBWrapper::~ProtoLevelDBWrapper() = default;

void ProtoLevelDBWrapper::InitWithDatabase(
    LevelDB* database,
    const base::FilePath& database_dir,
    const leveldb_env::Options& options,
    typename ProtoLevelDBWrapper::InitCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!db_);
  DCHECK(database);
  db_ = database;
  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(InitFromTaskRunner, base::Unretained(db_), database_dir,
                     options, success),
      base::BindOnce(RunInitCallback, std::move(callback),
                     base::Owned(success)));
}

void ProtoLevelDBWrapper::Destroy(
    typename ProtoLevelDBWrapper::DestroyCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(db_);

  bool* success = new bool(false);
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(DestroyFromTaskRunner, base::Unretained(db_), success),
      base::BindOnce(RunDestroyCallback, std::move(callback),
                     base::Owned(success)));
}

void ProtoLevelDBWrapper::LoadKeys(
    typename ProtoLevelDBWrapper::LoadKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto success = std::make_unique<bool>(false);
  auto keys = std::make_unique<std::vector<std::string>>();
  auto load_task = base::BindOnce(LoadKeysFromTaskRunner, base::Unretained(db_),
                                  keys.get(), success.get());
  task_runner_->PostTaskAndReply(
      FROM_HERE, std::move(load_task),
      base::BindOnce(RunLoadKeysCallback, std::move(callback),
                     std::move(success), std::move(keys)));
}

bool ProtoLevelDBWrapper::GetApproximateMemoryUse(uint64_t* approx_mem_use) {
  return db_->GetApproximateMemoryUse(approx_mem_use);
}

const scoped_refptr<base::SequencedTaskRunner>&
ProtoLevelDBWrapper::task_runner() {
  return task_runner_;
}

}  // namespace leveldb_proto