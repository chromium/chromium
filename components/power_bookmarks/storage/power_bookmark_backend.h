// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_BACKEND_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_BACKEND_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "components/power_bookmarks/storage/power_bookmark_database.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"

namespace power_bookmarks {

struct SearchParams;
class PowerBookmarkSyncBridge;

// Class responsible for marshalling calls from the browser thread which the
// service is called from and the background thread which the database is
// run on. Calls to this class should be posted on the background task_runner.
class PowerBookmarkBackend {
 public:
  // Constructs the backend, should be called form the browser thread.
  // Subsequent calls to the backend should be posted to the given
  // `task_runner`.
  explicit PowerBookmarkBackend(const base::FilePath& database_dir);
  PowerBookmarkBackend(const PowerBookmarkBackend&) = delete;
  PowerBookmarkBackend& operator=(const PowerBookmarkBackend&) = delete;
  ~PowerBookmarkBackend();

  void Init(bool use_database);
  void Shutdown();

  // Returns a vector of Powers for the given `url`. Use `power_type` to
  // restrict which type is returned or use POWER_TYPE_UNSPECIFIED to return
  // everything.
  std::vector<std::unique_ptr<Power>> GetPowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type);

  // Returns a vector of PowerOverviews for the given `power_type`.
  std::vector<std::unique_ptr<PowerOverview>> GetPowerOverviewsForType(
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type);

  // Returns a vector of Powers matching the given `search_params`.
  std::vector<std::unique_ptr<Power>> Search(const SearchParams& search_params);

  // Create the given `power` in the database. If it already exists, then it
  // will be updated. Returns whether the operation was successful.
  bool CreatePower(std::unique_ptr<Power> power);
  // Update the given `power` in the database. If it doesn't exist, then it
  // will be created instead. Returns whether the operation was successful.
  bool UpdatePower(std::unique_ptr<Power> power);
  // Delete the given `guid` in the database, if it exists. Returns whether
  // the operation was successful.
  bool DeletePower(const base::GUID& guid);
  // Delete all powers for the given `url`. Success of the operation is
  // returned through the given `callback`. Use `power_type` to restrict which
  // type is deleted or use POWER_TYPE_UNSPECIFIED to delete everything.
  bool DeletePowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type);

 private:
  const base::FilePath database_dir_;

  std::unique_ptr<PowerBookmarkDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Sync Bridge implementation. Only initialized when the sqlite database is
  // used.
  std::unique_ptr<PowerBookmarkSyncBridge> bridge_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_BACKEND_H_
