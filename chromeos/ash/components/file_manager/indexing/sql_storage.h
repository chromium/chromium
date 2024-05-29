// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_SQL_STORAGE_H_

#include <optional>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/file_manager/indexing/file_info.h"
#include "chromeos/ash/components/file_manager/indexing/file_info_table.h"
#include "chromeos/ash/components/file_manager/indexing/index_storage.h"
#include "chromeos/ash/components/file_manager/indexing/posting_list_table.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"
#include "chromeos/ash/components/file_manager/indexing/term_table.h"
#include "chromeos/ash/components/file_manager/indexing/token_table.h"
#include "chromeos/ash/components/file_manager/indexing/url_table.h"
#include "sql/database.h"
#include "url/gurl.h"

namespace sql {
class Statement;
}  // namespace sql

namespace ash::file_manager {

// Represents an inverted index storage implemented on top of SQL database.
// Use this in production environments. Typical use it to create an instance
// of the FileIndexService class via its factory. If you need to create it
// manually, you would need to run:
//
// base::FilePath db_path("path/to/where/db/is/stored/dbname.db");
// SqlStorage storage(db_path, "uma_unique_db_tag");
// CHECK(storage.Init());
//
// Once successfully initialized, the storage is ready to use. Use it to
// store associations between terms and files, using public method of this
// class.
class COMPONENT_EXPORT(FILE_MANAGER) SqlStorage : public IndexStorage {
 public:
  SqlStorage(base::FilePath db_path, const std::string& uma_tag);
  ~SqlStorage() override;

  SqlStorage(const SqlStorage&) = delete;
  SqlStorage& operator=(const SqlStorage&) = delete;

  // SQL implementation of IndexStorage methods.

  // Lifecycle methods.
  [[nodiscard]] bool Init() override;
  bool Close() override;

  // Inverted index and plain index functions.
  const std::set<int64_t> GetUrlIdsForTermId(int64_t term_id) const override;
  const std::set<int64_t> GetTermIdsForUrl(int64_t url_id) const override;

  // Posting list support.
  size_t AddToPostingList(int64_t term_id, int64_t url_id) override;
  size_t DeleteFromPostingList(int64_t term_id, int64_t url_id) override;

  // Term ID management.
  int64_t GetTermId(const Term& term) const override;
  int64_t GetOrCreateTermId(const Term& term) override;

  // Token ID management.
  int64_t GetTokenId(const std::string& token_bytes) const override;
  int64_t GetOrCreateTokenId(const std::string& token0_bytes) override;

  // URL ID management.
  int64_t GetUrlId(const GURL& url) const override;
  int64_t GetOrCreateUrlId(const GURL& url) override;
  int64_t MoveUrl(const GURL& from, const GURL& to) override;
  int64_t DeleteUrl(const GURL& url) override;

  // FileInfo management.
  int64_t PutFileInfo(const FileInfo& file_info) override;
  std::optional<FileInfo> GetFileInfo(int64_t url_id) const override;
  int64_t DeleteFileInfo(int64_t url_id) override;

  // Miscellaneous.
  size_t AddTermIdsForUrl(const std::set<int64_t>& term_ids,
                          int64_t url_id) override;
  size_t DeleteTermIdsForUrl(const std::set<int64_t>& term_ids,
                             int64_t url_id) override;

  // Access to the actual SQL database. Tests only.
  sql::Database* GetDbForTests() { return &db_; }

 private:
  // Error callback set on the database.
  void OnErrorCallback(int error, sql::Statement* stmt);

  // Executes the code that initializes all tables owned by this storage.
  bool InitTables();

  // Resets the database to empty state. Only call after catastrophic error.s
  void Restart();

  // The User Metric Analysis (uma) tag for recording events related to SQL
  // storage.
  const std::string uma_tag_;

  // The full path to the database (folder and name).
  base::FilePath db_path_;

  // The actual SQL Lite database.
  sql::Database db_;

  // The table that holds a mapping from tokens to token IDs.
  TokenTable token_table_;

  // The table that holds a mapping from terms to their IDs.
  TermTable term_table_;

  // The table that holds a mapping from URLs to URL IDs.
  UrlTable url_table_;

  // The table that holds a mapping from URL IDs to FileInfo objects.
  FileInfoTable file_info_table_;

  // The table that holds associations between term IDs and
  // URL IDs. It also maintains indexes that allow fast retrieval of all
  // URL IDs associated with the given term ID and all term IDs present
  // in a file with the given URL ID.
  PostingListTable posting_list_table_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_SQL_STORAGE_H_
