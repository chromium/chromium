// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/group_data_store.h"

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/data_sharing/internal/group_data_proto_utils.h"
#include "components/data_sharing/internal/protocol/group_data_db.pb.h"
#include "components/data_sharing/public/group_data.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "sql/database.h"

namespace data_sharing {

namespace {

constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

constexpr char kGroupEntitiesTableName[] = "group_data_entities";
constexpr int kCurrentSchemaVersion = 1;
// TODO(crbug.com/301390275): make this consistent with other places where
// amount of groups are limited once numbers are known.
constexpr size_t kMaxNumEntriesInDB = 20000;

GroupDataStore::DBInitStatus InitOnDBSequence(
    base::FilePath db_path,
    sql::Database* db,
    sqlite_proto::ProtoTableManager* table_manager,
    sqlite_proto::KeyValueData<data_sharing_pb::GroupEntity>*
        group_entity_data) {
  CHECK(db);
  CHECK(table_manager);
  CHECK(group_entity_data);

  if (!db->Open(db_path)) {
    return GroupDataStore::DBInitStatus::kFailure;
  }

  table_manager->InitializeOnDbSequence(
      db, std::vector<std::string>{kGroupEntitiesTableName},
      kCurrentSchemaVersion);
  group_entity_data->InitializeOnDBSequence();
  return GroupDataStore::DBInitStatus::kSuccess;
}

}  // namespace

GroupDataStore::GroupDataStore(const base::FilePath& db_path,
                               DBLoadedCallback db_loaded_callback)
    : db_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kDBTaskTraits)),
      db_(std::make_unique<sql::Database>(sql::DatabaseOptions{})),
      proto_table_manager_(
          base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
              db_task_runner_)),
      group_entity_table_(
          std::make_unique<
              sqlite_proto::KeyValueTable<data_sharing_pb::GroupEntity>>(
              kGroupEntitiesTableName)),
      group_entity_data_(
          std::make_unique<
              sqlite_proto::KeyValueData<data_sharing_pb::GroupEntity>>(
              proto_table_manager_,
              group_entity_table_.get(),
              kMaxNumEntriesInDB,
              /*flush_delay=*/base::TimeDelta())) {
  // Unretained() for DB objects is safe here, because `this` destructor ensures
  // that these objects outlive any task posted to DB sequence.
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitOnDBSequence, db_path, base::Unretained(db_.get()),
                     base::Unretained(proto_table_manager_.get()),
                     base::Unretained(group_entity_data_.get())),
      base::BindOnce(&GroupDataStore::OnDBReady, weak_ptr_factory_.GetWeakPtr(),
                     std::move(db_loaded_callback)));
}

GroupDataStore::~GroupDataStore() {
  // Shutdown `proto_table_manager_` and delete it together with `db_` and
  // KeyValueTable on DB sequence, then deletes KeyValueData and runs
  // `shutdown_callback_` on the main sequence.
  // This ensures that DB objects outlive any other task posted to DB sequence,
  // since their deletion is the very last posted task.
  db_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<sql::Database> db,
             scoped_refptr<sqlite_proto::ProtoTableManager> table_manager,
             auto group_entity_table) { table_manager->WillShutdown(); },
          std::move(db_), std::move(proto_table_manager_),
          std::move(group_entity_table_)),
      base::BindOnce(
          [](auto group_entity_data, base::OnceClosure shutdown_callback) {
            if (shutdown_callback) {
              std::move(shutdown_callback).Run();
            }
          },
          std::move(group_entity_data_), std::move(shutdown_callback_)));
}

void GroupDataStore::StoreGroupData(const VersionToken& version_token,
                                    const GroupData& group_data) {
  CHECK_EQ(db_init_status_, DBInitStatus::kSuccess);

  // TODO(crbug.com/301390275): support batching StoreGroupData() (by setting
  // `flush_delay`?).
  data_sharing_pb::GroupEntity entity;
  entity.mutable_metadata()->set_last_processed_version_token(
      version_token.value());
  *entity.mutable_data() = GroupDataToProto(group_data);
  group_entity_data_->UpdateData(group_data.group_token.group_id.value(),
                                 entity);
}

void GroupDataStore::DeleteGroups(const std::vector<GroupId>& groups_ids) {
  CHECK_EQ(db_init_status_, DBInitStatus::kSuccess);
  std::vector<std::string> keys;
  for (const auto& id : groups_ids) {
    keys.push_back(id.value());
  }
  group_entity_data_->DeleteData(keys);
}

std::optional<VersionToken> GroupDataStore::GetGroupVersionToken(
    const GroupId& group_id) const {
  CHECK_EQ(db_init_status_, DBInitStatus::kSuccess);

  data_sharing_pb::GroupEntity entity;
  if (!group_entity_data_->TryGetData(group_id.value(), &entity)) {
    return std::nullopt;
  }

  return VersionToken(entity.metadata().last_processed_version_token());
}

std::optional<GroupData> GroupDataStore::GetGroupData(
    const GroupId& group_id) const {
  CHECK_EQ(db_init_status_, DBInitStatus::kSuccess);

  data_sharing_pb::GroupEntity entity;
  if (!group_entity_data_->TryGetData(group_id.value(), &entity)) {
    return std::nullopt;
  }

  return GroupDataFromProto(entity.data());
}

std::vector<GroupId> GroupDataStore::GetAllGroupIds() const {
  CHECK_EQ(db_init_status_, DBInitStatus::kSuccess);
  // All entities are cached, provided that `max_num_entries` is unset when
  // initializing `group_entity_data_`.
  std::vector<GroupId> result;
  for (const auto& [group_id, _] : group_entity_data_->GetAllCached()) {
    result.emplace_back(group_id);
  }
  return result;
}

void GroupDataStore::OnDBReady(DBLoadedCallback db_loaded_callback,
                               DBInitStatus status) {
  db_init_status_ = status;
  std::move(db_loaded_callback).Run(status);
}

void GroupDataStore::SetShutdownCallbackForTesting(
    base::OnceClosure shutdown_callback) {
  shutdown_callback_ = std::move(shutdown_callback);
}

}  // namespace data_sharing
