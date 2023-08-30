// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_CLIENT_UTILS_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_CLIENT_UTILS_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "components/sync/base/model_type.h"

namespace password_manager {
class PasswordStoreInterface;
}  // namespace password_manager

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace reading_list {
class DualReadingListModel;
}  // namespace reading_list

namespace syncer {
struct LocalDataDescription;
}  // namespace syncer

namespace browser_sync::sync_client_utils {
// A struct to handle passing local and account stores/models to the util
// methods.
struct DataTypeModels {
  // For PASSWORDS.
  raw_ptr<password_manager::PasswordStoreInterface> profile_password_store =
      nullptr;
  raw_ptr<password_manager::PasswordStoreInterface> account_password_store =
      nullptr;

  // For BOOKMARKS.
  raw_ptr<bookmarks::BookmarkModel> local_bookmark_model = nullptr;
  raw_ptr<bookmarks::BookmarkModel> account_bookmark_model = nullptr;

  // For READING_LIST.
  raw_ptr<reading_list::DualReadingListModel> dual_reading_list_model = nullptr;
};

void GetLocalDataDescriptions(
    syncer::ModelTypeSet types,
    base::OnceCallback<void(
        std::map<syncer::ModelType, syncer::LocalDataDescription>)> callback,
    DataTypeModels local_and_account_models);

void TriggerLocalDataMigration(syncer::ModelTypeSet types,
                               DataTypeModels local_and_account_models);

}  // namespace browser_sync::sync_client_utils

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_CLIENT_UTILS_H_
