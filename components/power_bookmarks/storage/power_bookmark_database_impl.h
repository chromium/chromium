// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_IMPL_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_IMPL_H_

#include "base/files/file_path.h"
#include "components/power_bookmarks/core/powers/power.h"
#include "components/power_bookmarks/core/powers/power_overview.h"
#include "components/power_bookmarks/storage/power_bookmark_database.h"
#include "sql/database.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace power_bookmarks {

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
      const PowerType& power_type) override;
  std::vector<std::unique_ptr<PowerOverview>> GetPowerOverviewsForType(
      const PowerType& power_type) override;
  bool CreatePower(std::unique_ptr<Power> power) override;
  bool UpdatePower(std::unique_ptr<Power> power) override;
  bool DeletePower(const base::GUID& guid) override;
  bool DeletePowersForURL(const GURL& url,
                          const PowerType& power_type) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PowerBookmarkDatabaseImplTest,
                           InitDatabaseWithErrorCallback);

  // Called by the database to report errors.
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // Creates or migrates to the new schema if needed.
  bool InitSchema();
  bool CreateSchema();

  absl::optional<PowerBookmarkSpecifics> DeserializeOrDelete(
      const std::string& data,
      const base::GUID& id);

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  const base::FilePath database_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_DATABASE_IMPL_H_
