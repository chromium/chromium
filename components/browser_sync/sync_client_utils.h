// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_CLIENT_UTILS_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_CLIENT_UTILS_H_

#include <list>
#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "components/sync/base/model_type.h"

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

namespace reading_list {
class DualReadingListModel;
}  // namespace reading_list

namespace sync_bookmarks {
class BookmarkSyncService;
}  // namespace sync_bookmarks

namespace syncer {
struct LocalDataDescription;
}  // namespace syncer

namespace browser_sync {

// Helper class to query information about existing local data (like count,
// domains etc.) for requested data types.
// TODO(crbug.com/1489660): Look into reducing code duplicacy between
// LocalDataQueryHelper and LocalDataMigrationHelper.
class LocalDataQueryHelper {
 public:
  LocalDataQueryHelper(
      password_manager::PasswordStoreInterface* profile_password_store,
      password_manager::PasswordStoreInterface* account_password_store,
      sync_bookmarks::BookmarkSyncService* local_bookmark_sync_service,
      sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
      reading_list::DualReadingListModel* dual_reading_list_model);
  ~LocalDataQueryHelper();

  // Queries the count and description/preview of existing local data for
  // `types` data types. This is an asynchronous method which returns the result
  // via the callback `callback` once the information for all the data types in
  // `types` is available.
  void Run(
      syncer::ModelTypeSet types,
      base::OnceCallback<void(
          std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback);

 private:
  class LocalDataQueryRequest;

  void OnRequestComplete(
      LocalDataQueryRequest* request,
      base::OnceCallback<void(
          std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback);

  // To keep track of all ongoing requests.
  std::list<std::unique_ptr<LocalDataQueryRequest>> request_list_;

  // For PASSWORDS.
  raw_ptr<password_manager::PasswordStoreInterface> profile_password_store_;
  raw_ptr<password_manager::PasswordStoreInterface> account_password_store_;
  // For BOOKMARKS.
  raw_ptr<sync_bookmarks::BookmarkSyncService> local_bookmark_sync_service_;
  raw_ptr<sync_bookmarks::BookmarkSyncService> account_bookmark_sync_service_;
  // For READING_LIST.
  raw_ptr<reading_list::DualReadingListModel> dual_reading_list_model_;
};

// Helper class to move all local data to account for the requested data types.
class LocalDataMigrationHelper {
 public:
  LocalDataMigrationHelper(
      password_manager::PasswordStoreInterface* profile_password_store,
      password_manager::PasswordStoreInterface* account_password_store,
      sync_bookmarks::BookmarkSyncService* local_bookmark_sync_service,
      sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
      reading_list::DualReadingListModel* dual_reading_list_model);
  ~LocalDataMigrationHelper();

  // Requests sync service to move all local data to account for `types` data
  // types. This is an asynchronous method which moves the local data for all
  // `types` to the account store locally. Upload to the server will happen as
  // part of the regular commit process, and is NOT part of this method.
  void Run(syncer::ModelTypeSet types);

 private:
  class LocalDataMigrationRequest;

  void OnRequestComplete(LocalDataMigrationRequest* request);

  // To keep track of all ongoing requests.
  std::list<std::unique_ptr<LocalDataMigrationRequest>> request_list_;

  // For PASSWORDS.
  raw_ptr<password_manager::PasswordStoreInterface> profile_password_store_;
  raw_ptr<password_manager::PasswordStoreInterface> account_password_store_;
  // For BOOKMARKS.
  raw_ptr<sync_bookmarks::BookmarkSyncService> local_bookmark_sync_service_;
  raw_ptr<sync_bookmarks::BookmarkSyncService> account_bookmark_sync_service_;
  // For READING_LIST.
  raw_ptr<reading_list::DualReadingListModel> dual_reading_list_model_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_CLIENT_UTILS_H_
