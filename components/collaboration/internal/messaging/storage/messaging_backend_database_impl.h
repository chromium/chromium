// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_DATABASE_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_DATABASE_IMPL_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/collaboration/internal/messaging/storage/messaging_backend_database.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "sql/database.h"

namespace collaboration::messaging {

// Database implementation using sqlite_proto.
class MessagingBackendDatabaseImpl : public MessagingBackendDatabase {
 public:
  explicit MessagingBackendDatabaseImpl(const base::FilePath& profile_dir);
  MessagingBackendDatabaseImpl(const MessagingBackendDatabaseImpl&) = delete;
  MessagingBackendDatabaseImpl& operator=(const MessagingBackendDatabaseImpl&) =
      delete;
  ~MessagingBackendDatabaseImpl() override;

  void Initialize(DBLoadedCallback db_loaded_callback) override;

  void Update(const collaboration_pb::Message& message) override;

  void Delete(const std::vector<std::string>& message_uuids) override;

  void DeleteAllData() override;

  void SetShutdownCallbackForTesting(base::OnceClosure shutdown_callback);

  std::optional<collaboration_pb::Message> GetMessageForTesting(
      const std::string message_uuid);

 private:
  void OnDBReady(DBLoadedCallback db_loaded_callback, bool success);

  base::FilePath profile_path_;

  // The following fields hold objects to work with SQLite database. `db_`,
  // `proto_table_manager_` are deleted on db sequence; `message_data_` and
  // `message_table_` are deleted on the main thread, however only after
  // deletion of the rest.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  std::unique_ptr<sql::Database> db_;
  scoped_refptr<sqlite_proto::ProtoTableManager> proto_table_manager_;
  std::unique_ptr<sqlite_proto::KeyValueTable<collaboration_pb::Message>>
      message_table_;
  std::unique_ptr<sqlite_proto::KeyValueData<collaboration_pb::Message>>
      message_data_;

  base::OnceClosure shutdown_callback_;

  // Whether database is loaded successfully, set when database loaded
  // successfully or failed.
  std::optional<bool> load_success_;

  base::WeakPtrFactory<MessagingBackendDatabaseImpl> weak_ptr_factory_{this};
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_MESSAGING_BACKEND_DATABASE_IMPL_H_
