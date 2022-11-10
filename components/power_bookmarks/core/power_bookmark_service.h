// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/guid.h"
#include "base/threading/sequence_bound.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/power_bookmarks/core/proto/power_bookmark_specifics.pb.h"

namespace bookmarks {
class BookmarkModel;
class BaseBookmarkModelObserver;
}  // namespace bookmarks

namespace power_bookmarks {

class Power;
class PowerOverview;
class PowerBookmarkDataProvider;
class PowerBookmarkBackend;

using PowersCallback =
    base::OnceCallback<void(std::vector<std::unique_ptr<Power>> powers)>;
using PowerOverviewsCallback = base::OnceCallback<void(
    std::vector<std::unique_ptr<PowerOverview>> power_overviews)>;
using SuccessCallback = base::OnceCallback<void(bool success)>;

class PowerBookmarkService : public KeyedService,
                             public bookmarks::BaseBookmarkModelObserver {
 public:
  PowerBookmarkService(
      bookmarks::BookmarkModel* model,
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  PowerBookmarkService(const PowerBookmarkService&) = delete;
  PowerBookmarkService& operator=(const PowerBookmarkService&) = delete;

  ~PowerBookmarkService() override;

  void InitPowerBookmarkDatabase();

  // Returns a vector of Powers for the given `url` through the given
  // `callback`. Use `power_type` to restrict which type is returned or use
  // POWER_TYPE_UNSPECIFIED to return everything.
  void GetPowersForURL(const GURL& url,
                       const PowerType& power_type,
                       PowersCallback callback);

  // Returns a vector of PowerOverviews for the given `power_type` through the
  // given `callback`.
  void GetPowerOverviewsForType(const PowerType& power_type,
                                PowerOverviewsCallback callback);

  // Create the given `power` in the database. If it already exists, then it
  // will be updated. Success of the operation is returned through the given
  // `callback`.
  void CreatePower(std::unique_ptr<Power> power, SuccessCallback callback);

  // Update the given `power` in the database. If it doesn't exist, then it
  // will be created instead. Success of the operation is returned through the
  // given `callback`.
  void UpdatePower(std::unique_ptr<Power> power, SuccessCallback callback);

  // Delete the given `guid` in the database, if it exists. Success of the
  // operation is returned through the given `callback`.
  // TODO(crbug.com/1378793): Encapsulate the storage key if possible.
  void DeletePower(const base::GUID& guid, SuccessCallback callback);

  // Delete all powers for the given `url`. Success of the operation is
  // returned through the given `callback`. Use `power_type` to restrict which
  // type is deleted or use POWER_TYPE_UNSPECIFIED to delete everything.
  void DeletePowersForURL(const GURL& url,
                          const PowerType& power_type,
                          SuccessCallback callback);

  // Allow features to receive notification when a bookmark node is created to
  // add extra information. The `data_provider` can be removed with the remove
  // method.
  void AddDataProvider(PowerBookmarkDataProvider* data_provider);
  void RemoveDataProvider(PowerBookmarkDataProvider* data_provider);

  // BaseBookmarkModelObserver
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                         const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool newly_added) override;
  void BookmarkModelChanged() override {}

 private:
  bookmarks::BookmarkModel* model_;
  base::SequenceBound<PowerBookmarkBackend> backend_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  std::vector<PowerBookmarkDataProvider*> data_providers_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_