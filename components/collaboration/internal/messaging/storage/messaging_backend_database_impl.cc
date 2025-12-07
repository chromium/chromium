// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/messaging/storage/messaging_backend_database_impl.h"

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"

namespace collaboration::messaging {
namespace {
constexpr size_t kMaxNumEntriesInDB = 20000;
constexpr int kCurrentSchemaVersion = 1;

constexpr char kCollaborationMessageTableName[] = "collaboration_messages";

constexpr base::TaskTraits kDBTaskTraits = {
    base::MayBlock(), base::TaskPriority::BEST_EFFORT,
    base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN};

bool InitOnDBSequence(
    base::FilePath profile_dir,
    sql::Database* db,
    sqlite_proto::ProtoTableManager* table_manager,
    sqlite_proto::KeyValueData<collaboration_pb::Message>* message_data) {
  CHECK(db);
  CHECK(table_manager);
  CHECK(message_data);

  base::FilePath db_dir =
      profile_dir.Append(FILE_PATH_LITERAL("Collaboration"));
  if (!base::CreateDirectory(db_dir)) {
    LOG(ERROR) << "Failed to create or open DB directory: " << db_dir;
    return false;
  }

  const base::FilePath db_path = db_dir.Append(FILE_PATH_LITERAL("MessageDB"));
  if (!db->Open(db_path)) {
    LOG(ERROR) << "Failed to open DB " << db_path << ": "
               << db->GetErrorMessage();
    return false;
  }

  table_manager->InitializeOnDbSequence(
      db, std::vector<std::string>{kCollaborationMessageTableName},
      kCurrentSchemaVersion);
  message_data->InitializeOnDBSequence();
  return true;
}

}  // namespace

MessagingBackendDatabaseImpl::MessagingBackendDatabaseImpl(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      db_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner(kDBTaskTraits)),
      db_(std::make_unique<sql::Database>(
          sql::Database::Tag("CollaborationMessageStorage"))),
      proto_table_manager_(
          base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
              db_task_runner_)),
      message_table_(std::make_unique<
                     sqlite_proto::KeyValueTable<collaboration_pb::Message>>(
          kCollaborationMessageTableName)),
      message_data_(std::make_unique<
                    sqlite_proto::KeyValueData<collaboration_pb::Message>>(
          proto_table_manager_,
          message_table_.get(),
          kMaxNumEntriesInDB,
          /*flush_delay=*/base::TimeDelta())) {}

void MessagingBackendDatabaseImpl::Initialize(
    DBLoadedCallback db_loaded_callback) {
  // Unretained() for DB objects is safe here, because `this` destructor ensures
  // that these objects outlive any task posted to DB sequence.
  db_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&InitOnDBSequence, profile_path_,
                     base::Unretained(db_.get()),
                     base::Unretained(proto_table_manager_.get()),
                     base::Unretained(message_data_.get())),
      base::BindOnce(&MessagingBackendDatabaseImpl::OnDBReady,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(db_loaded_callback)));
}

MessagingBackendDatabaseImpl::~MessagingBackendDatabaseImpl() {
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
             auto message_table) { table_manager->WillShutdown(); },
          std::move(db_), std::move(proto_table_manager_),
          std::move(message_table_)),
      base::BindOnce(
          [](auto message_data, base::OnceClosure shutdown_callback) {
            if (shutdown_callback) {
              std::move(shutdown_callback).Run();
            }
          },
          std::move(message_data_), std::move(shutdown_callback_)));
}

void MessagingBackendDatabaseImpl::Update(
    const collaboration_pb::Message& message) {
  CHECK(load_success_.has_value() && load_success_.value());
  message_data_->UpdateData(message.uuid(), message);
}

void MessagingBackendDatabaseImpl::Delete(
    const std::vector<std::string>& message_uuids) {
  CHECK(load_success_.has_value() && load_success_.value());
  message_data_->DeleteData(message_uuids);
}

void MessagingBackendDatabaseImpl::DeleteAllData() {
  message_data_->DeleteAllData();
}

void MessagingBackendDatabaseImpl::OnDBReady(
    DBLoadedCallback db_loaded_callback,
    bool success) {
  load_success_ = success;
  std::move(db_loaded_callback).Run(success, message_data_->GetAllCached());
}

void MessagingBackendDatabaseImpl::SetShutdownCallbackForTesting(
    base::OnceClosure shutdown_callback) {
  shutdown_callback_ = std::move(shutdown_callback);
}

std::optional<collaboration_pb::Message>
MessagingBackendDatabaseImpl::GetMessageForTesting(
    const std::string message_uuid) {
  CHECK(load_success_.has_value() && load_success_.value());
  collaboration_pb::Message message;
  if (!message_data_->TryGetData(message_uuid, &message)) {
    return std::nullopt;
  }
  return message;
}

}  // namespace collaboration::messaging
