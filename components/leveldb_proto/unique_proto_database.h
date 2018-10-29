// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_
#define COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_

#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_leveldb_wrapper.h"

namespace leveldb_proto {

// An implementation of ProtoDatabase<T> that manages the lifecycle of a unique
// LevelDB instance.
template <typename T>
class UniqueProtoDatabase : public ProtoDatabase<T> {
 public:
  UniqueProtoDatabase(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner)
      : ProtoDatabase<T>(task_runner) {}

  virtual void Init(const char* client_name,
                    const base::FilePath& database_dir,
                    const leveldb_env::Options& options,
                    typename ProtoDatabase<T>::InitCallback callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    db_ = std::make_unique<LevelDB>(client_name);
    ProtoDatabase<T>::InitWithDatabase(db_.get(), database_dir, options,
                                       std::move(callback));
  }

  virtual ~UniqueProtoDatabase() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    if (db_.get() &&
        !this->db_wrapper_->task_runner()->DeleteSoon(FROM_HERE, db_.release()))
      DLOG(WARNING) << "Proto database will not be deleted.";
  }

 private:
  THREAD_CHECKER(thread_checker_);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<LevelDB> db_;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_UNIQUE_PROTO_DATABASE_H_
