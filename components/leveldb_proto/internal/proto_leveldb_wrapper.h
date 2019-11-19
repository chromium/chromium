// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "components/leveldb_proto/internal/proto_leveldb_wrapper_metrics.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "third_party/leveldatabase/env_chromium.h"

namespace base {
class FilePath;
}

namespace leveldb_proto {

class LevelDB;

using KeyValueVector = std::vector<std::pair<std::string, std::string>>;
using KeyValueMap = std::map<std::string, std::string>;
using KeyVector = std::vector<std::string>;
using ValueVector = std::vector<std::string>;

// When the ProtoDatabase instance is deleted, in-progress asynchronous
// operations will be completed and the corresponding callbacks will be called.
// Construction/calls/destruction should all happen on the same thread.
class COMPONENT_EXPORT(LEVELDB_PROTO) ProtoLevelDBWrapper {
 public:
  // Used to destroy database when initialization fails.
  static void Destroy(
      const base::FilePath& db_dir,
      const std::string& client_id,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      Callbacks::DestroyCallback callback);

  // All blocking calls/disk access will happen on the provided |task_runner|.
  ProtoLevelDBWrapper(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ProtoLevelDBWrapper(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      LevelDB* db);

  virtual ~ProtoLevelDBWrapper();

  void UpdateEntries(std::unique_ptr<KeyValueVector> entries_to_save,
                     std::unique_ptr<KeyVector> keys_to_remove,
                     Callbacks::UpdateCallback callback);

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      Callbacks::UpdateCallback callback);

  void UpdateEntriesWithRemoveFilter(
      std::unique_ptr<KeyValueVector> entries_to_save,
      const KeyFilter& delete_key_filter,
      const std::string& target_prefix,
      Callbacks::UpdateCallback callback);

  void LoadEntries(Callbacks::LoadCallback callback);

  void LoadEntriesWithFilter(const KeyFilter& key_filter,
                             Callbacks::LoadCallback callback);

  void LoadEntriesWithFilter(const KeyFilter& key_filter,
                             const leveldb::ReadOptions& options,
                             const std::string& target_prefix,
                             Callbacks::LoadCallback callback);

  void LoadKeysAndEntries(Callbacks::LoadKeysAndEntriesCallback callback);

  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      Callbacks::LoadKeysAndEntriesCallback callback);

  void LoadKeysAndEntriesWithFilter(
      const KeyFilter& filter,
      const leveldb::ReadOptions& options,
      const std::string& target_prefix,
      Callbacks::LoadKeysAndEntriesCallback callback);

  void LoadKeysAndEntriesWhile(const KeyFilter& while_callback,
                               const KeyFilter& filter,
                               const leveldb::ReadOptions& options,
                               const std::string& target_prefix,
                               Callbacks::LoadKeysAndEntriesCallback callback);

  void LoadKeysAndEntriesInRange(
      const std::string& start,
      const std::string& end,
      Callbacks::LoadKeysAndEntriesCallback callback);

  void LoadKeys(Callbacks::LoadKeysCallback callback);
  void LoadKeys(const std::string& target_prefix,
                Callbacks::LoadKeysCallback callback);

  void GetEntry(const std::string& key, Callbacks::GetCallback callback);

  void RemoveKeys(const KeyFilter& filter,
                  const std::string& target_prefix,
                  Callbacks::UpdateCallback callback);

  void Destroy(Callbacks::DestroyCallback callback);

  void RunInitCallback(Callbacks::InitCallback callback,
                       const leveldb::Status* status);

  // Allow callers to provide their own Database implementation.
  void InitWithDatabase(LevelDB* database,
                        const base::FilePath& database_dir,
                        const leveldb_env::Options& options,
                        bool destroy_on_corruption,
                        Callbacks::InitStatusCallback callback);

  void SetMetricsId(const std::string& id);

  bool GetApproximateMemoryUse(uint64_t* approx_mem_use);

  const scoped_refptr<base::SequencedTaskRunner>& task_runner();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Used to run blocking tasks in-order, must be the TaskRunner that |db_|
  // relies on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  LevelDB* db_ = nullptr;

  // The identifier used when recording metrics to determine the source of the
  // LevelDB calls, likely the database client name.
  std::string metrics_id_ = "Default";

  base::WeakPtrFactory<ProtoLevelDBWrapper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ProtoLevelDBWrapper);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_PROTO_LEVELDB_WRAPPER_H_
