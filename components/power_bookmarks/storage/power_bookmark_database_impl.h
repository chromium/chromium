// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_IMPL_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_IMPL_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "components/power_bookmarks/common/power.h"
#include "components/power_bookmarks/common/power_overview.h"
#include "components/power_bookmarks/storage/power_bookmark_database.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "url/gurl.h"

namespace power_bookmarks {

struct SearchParams;
class PowerBookmarkSyncMetadataDatabase;

constexpr base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("PowerBookmarks.db");

// Holds the SQL connection for the main Power Bookmarks tables.
class PowerBookmarkDatabaseImpl : public PowerBookmarkDatabase {
 public:
  explicit PowerBookmarkDatabaseImpl(const base::FilePath& database_dir);
  PowerBookmarkDatabaseImpl(const PowerBookmarkDatabaseImpl&) = delete;
  PowerBookmarkDatabaseImpl& operator=(const PowerBookmarkDatabaseImpl&) =
      delete;

  ~PowerBookmarkDatabaseImpl() override;

  // PowerBookmarkDatabase implementation
  bool Init() override;
  bool IsOpen() override;
  std::vector<std::unique_ptr<Power>> GetPowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) override;
  std::vector<std::unique_ptr<PowerOverview>> GetPowerOverviewsForType(
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type) override;
  std::vector<std::unique_ptr<Power>> GetPowersForSearchParams(
      const SearchParams& search_params) override;
  std::vector<std::unique_ptr<PowerOverview>> GetPowerOverviewsForSearchParams(
      const SearchParams& search_params) override;
  bool CreatePower(std::unique_ptr<Power> power) override;
  std::unique_ptr<Power> UpdatePower(std::unique_ptr<Power> power) override;
  bool DeletePower(const base::Uuid& guid) override;
  bool DeletePowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
      std::vector<std::string>* deleted_guids = nullptr) override;
  std::vector<std::unique_ptr<Power>> GetAllPowers() override;
  std::vector<std::unique_ptr<Power>> GetPowersForGUIDs(
      const std::vector<std::string>& guids) override;
  std::unique_ptr<Power> GetPowerForGUID(const std::string& guid) override;
  bool CreateOrMergePowerFromSync(const Power& power) override;
  bool DeletePowerFromSync(const std::string& guid) override;
  PowerBookmarkSyncMetadataDatabase* GetSyncMetadataDatabase() override;
  std::unique_ptr<Transaction> BeginTransaction() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PowerBookmarkDatabaseImplTest,
                           InitDatabaseWithErrorCallback);

  // Called by the database to report errors.
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // Creates or migrates to the new schema if needed.
  bool InitSchema();
  bool CreateSchema();

  std::optional<sync_pb::PowerBookmarkSpecifics> DeserializeOrDelete(
      const std::string& data,
      const base::Uuid& id);

  std::vector<std::string> GetGUIDsForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type);

  bool CreatePowerInternal(const Power& power);

  bool UpdatePowerInternal(const Power& power);

  bool DeletePowerInternal(const base::Uuid& guid);

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<PowerBookmarkSyncMetadataDatabase> sync_db_;

  const base::FilePath database_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_IMPL_H_
