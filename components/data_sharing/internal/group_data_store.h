// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_STORE_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_STORE_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/strong_alias.h"
#include "components/data_sharing/internal/protocol/group_data_db.pb.h"
#include "components/data_sharing/public/group_data.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "sql/database.h"

namespace data_sharing {

// TODO(crbug.com/301390275): figure out what precisely this should be
// (ConsistencyToken, timestamp, etc.).
using VersionToken = base::StrongAlias<class VersionTokenTag, std::string>;

// In-memory cache and persistent storage for GroupData.
class GroupDataStore {
 public:
  enum class DBInitStatus {
    kSuccess,
    kFailure,
    kNotLoaded,
  };

  using DBLoadedCallback = base::OnceCallback<void(DBInitStatus)>;

  // Public methods must not be called until `db_loaded_callback` is invoked
  // with kSuccess.
  GroupDataStore(const base::FilePath& db_path,
                 DBLoadedCallback db_loaded_callback);

  GroupDataStore(const GroupDataStore& other) = delete;
  GroupDataStore& operator=(const GroupDataStore& other) = delete;

  GroupDataStore(GroupDataStore&& other) = delete;
  GroupDataStore& operator=(GroupDataStore&& other) = delete;

  ~GroupDataStore();

  void StoreGroupData(const VersionToken& version_token,
                      const GroupData& group_data);
  void DeleteGroups(const std::vector<GroupId>& groups_ids);

  std::optional<VersionToken> GetGroupVersionToken(
      const GroupId& group_id) const;
  std::optional<GroupData> GetGroupData(const GroupId& group_id) const;
  std::vector<GroupId> GetAllGroupIds() const;

  // Allows test to wait until DB shutdown tasks are complete.
  void SetShutdownCallbackForTesting(base::OnceClosure shutdown_callback);

 private:
  void OnDBReady(DBLoadedCallback db_loaded_callback, DBInitStatus init_status);

  // The following fields hold objects to work with SQLite database. `db_`,
  // `proto_table_manager_` are deleted on db sequence; `group_entity_data_` and
  // `group_entity_table_` are deleted on the main thread, however only after
  // deletion of the rest.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  std::unique_ptr<sql::Database> db_;
  scoped_refptr<sqlite_proto::ProtoTableManager> proto_table_manager_;
  // Entities are keyed by GroupID.
  std::unique_ptr<sqlite_proto::KeyValueTable<data_sharing_pb::GroupEntity>>
      group_entity_table_;
  std::unique_ptr<sqlite_proto::KeyValueData<data_sharing_pb::GroupEntity>>
      group_entity_data_;

  DBInitStatus db_init_status_ = DBInitStatus::kNotLoaded;

  // Used only for tests to notify that shutdown tasks are completed on the DB
  // sequence.
  base::OnceClosure shutdown_callback_;

  base::WeakPtrFactory<GroupDataStore> weak_ptr_factory_{this};
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_GROUP_DATA_STORE_H_
