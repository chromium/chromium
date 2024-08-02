// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_BACKEND_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_BACKEND_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/power_bookmarks/common/power_bookmark_observer.h"
#include "components/power_bookmarks/storage/power_bookmark_database.h"
#include "components/power_bookmarks/storage/power_bookmark_sync_bridge.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace power_bookmarks {

struct SearchParams;

// Class responsible for marshalling calls from the browser thread which the
// service is called from and the background thread which the database is
// run on. Calls to this class should be posted on the background task_runner.
class PowerBookmarkBackend : public PowerBookmarkSyncBridge::Delegate {
 public:
  // `database_dir` the directory to create the backend database in.
  // `frontend_task_runner` the task runner which the service runs, used to
  // communicate observer events back through the service.
  // `service_observer` the observer the backend uses to plumb events through to
  // observers of PowerBookmarkService.
  PowerBookmarkBackend(
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> frontend_task_runner,
      base::WeakPtr<PowerBookmarkObserver> service_observer);
  PowerBookmarkBackend(const PowerBookmarkBackend&) = delete;
  PowerBookmarkBackend& operator=(const PowerBookmarkBackend&) = delete;
  virtual ~PowerBookmarkBackend();

  void Init(bool use_database);

  // For sync codebase only: gets a weak reference to the sync controller
  // delegate.
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetSyncControllerDelegate();

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
  std::vector<std::unique_ptr<Power>> SearchPowers(
      const SearchParams& search_params);

  // Returns a vector of PowerOverviews matching the given `search_params`.
  std::vector<std::unique_ptr<PowerOverview>> SearchPowerOverviews(
      const SearchParams& search_params);

  // Create the given `power` in the database. If it already exists, then it
  // will be updated. Returns whether the operation was successful.
  bool CreatePower(std::unique_ptr<Power> power);
  // Update the given `power` in the database. If it doesn't exist, then it
  // will be created instead. Returns whether the operation was successful.
  bool UpdatePower(std::unique_ptr<Power> power);
  // Delete the given `guid` in the database, if it exists. Returns whether
  // the operation was successful.
  bool DeletePower(const base::Uuid& guid);
  // Delete all powers for the given `url`. Success of the operation is
  // returned through the given `callback`. Use `power_type` to restrict which
  // type is deleted or use POWER_TYPE_UNSPECIFIED to delete everything.
  bool DeletePowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type);

  // PowerBookmarkSyncBridge::Delegate
  std::vector<std::unique_ptr<Power>> GetAllPowers() override;
  std::vector<std::unique_ptr<Power>> GetPowersForGUIDs(
      const std::vector<std::string>& guids) override;
  std::unique_ptr<Power> GetPowerForGUID(const std::string& guid) override;
  bool CreateOrMergePowerFromSync(const Power& power) override;
  bool DeletePowerFromSync(const std::string& guid) override;
  PowerBookmarkSyncMetadataDatabase* GetSyncMetadataDatabase() override;
  std::unique_ptr<Transaction> BeginTransaction() override;
  void NotifyPowersChanged() override;

 private:
  // Commit the change. If success then notify the observer, otherwise report
  // error to sync.
  bool CommitAndNotify(Transaction& transaction);

  const base::FilePath database_dir_;

  std::unique_ptr<PowerBookmarkDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Sync Bridge implementation. Only initialized when the sqlite database is
  // used.
  std::unique_ptr<PowerBookmarkSyncBridge> bridge_;

  scoped_refptr<base::SequencedTaskRunner> frontend_task_runner_;

  // Observer that serves the frontend of power bookmarks.
  // Needs to be called on the frontend task runner.
  base::WeakPtr<PowerBookmarkObserver> service_observer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_BACKEND_H_
