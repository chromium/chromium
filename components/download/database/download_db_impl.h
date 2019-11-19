// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_IMPL_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_IMPL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/download/database/download_db.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace download_pb {
class DownloadDBEntry;
}

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace download {

// A protodb Implementation of DownloadDB.
class DownloadDBImpl : public DownloadDB {
 public:
  DownloadDBImpl(DownloadNamespace download_namespace,
                 const base::FilePath& database_dir,
                 leveldb_proto::ProtoDatabaseProvider* db_provider);
  DownloadDBImpl(
      DownloadNamespace download_namespace,
      std::unique_ptr<
          leveldb_proto::ProtoDatabase<download_pb::DownloadDBEntry>> db);
  ~DownloadDBImpl() override;

  // DownloadDB implementation.
  void Initialize(DownloadDBCallback callback) override;
  void AddOrReplace(const DownloadDBEntry& entry) override;
  void AddOrReplaceEntries(const std::vector<DownloadDBEntry>& entries,
                           DownloadDBCallback callback) override;
  void LoadEntries(LoadEntriesCallback callback) override;
  void Remove(const std::string& guid) override;

 private:
  friend class DownloadDBTest;

  bool IsInitialized();

  void DestroyAndReinitialize(DownloadDBCallback callback);

  // Returns the key of the db entry.
  std::string GetEntryKey(const std::string& guid) const;

  // Called when database is initialized.
  void OnDatabaseInitialized(DownloadDBCallback callback,
                             leveldb_proto::Enums::InitStatus status);

  // Called when database is destroyed.
  void OnDatabaseDestroyed(DownloadDBCallback callback, bool success);

  // Called when entry is removed.
  void OnRemoveDone(bool success);

  // Called when all database entries are loaded.
  void OnAllEntriesLoaded(
      LoadEntriesCallback callback,
      bool success,
      std::unique_ptr<std::vector<download_pb::DownloadDBEntry>> entries);

  // Proto db for storing all the entries.
  std::unique_ptr<leveldb_proto::ProtoDatabase<download_pb::DownloadDBEntry>>
      db_;

  // Whether the object is initialized.
  bool is_initialized_ = false;

  // Namespace of this db.
  DownloadNamespace download_namespace_;

  // Number of initialize attempts.
  int num_initialize_attempts_ = 0;

  base::WeakPtrFactory<DownloadDBImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DownloadDBImpl);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_IN_PROGRESS_DOWNLOAD_DB_IMPL_H_
