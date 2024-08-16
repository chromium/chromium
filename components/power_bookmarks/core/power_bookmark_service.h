// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/power_bookmarks/common/power_bookmark_observer.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace power_bookmarks {

class Power;
class PowerOverview;
class PowerBookmarkDataProvider;
class PowerBookmarkBackend;
struct SearchParams;

using PowersCallback =
    base::OnceCallback<void(std::vector<std::unique_ptr<Power>> powers)>;
using PowerOverviewsCallback = base::OnceCallback<void(
    std::vector<std::unique_ptr<PowerOverview>> power_overviews)>;
using SuccessCallback = base::OnceCallback<void(bool success)>;

// Provides a public API surface for power bookmarks. The storage lives on a
// background thread, all results from there require a callback.
// Callbacks for the result of create/update/delete calls are wrapped so that
// observers can be notified when any changes to the storage occur.
class PowerBookmarkService : public KeyedService,
                             public bookmarks::BaseBookmarkModelObserver,
                             public PowerBookmarkObserver {
 public:
  // `model` is an instance of BookmarkModel, used to query bookmarks and
  // register as an observer.
  // `database_dir` the directory to create the backend database in.
  // `frontend_task_runner` the task runner which the service runs, used by the
  // backend to communicate back to the service. Primarily for observer events.
  // `backend_task_runner` the task runner which the backend runs, used by the
  // service to construct the backend.
  PowerBookmarkService(
      bookmarks::BookmarkModel* model,
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> frontend_task_runner,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  PowerBookmarkService(const PowerBookmarkService&) = delete;
  PowerBookmarkService& operator=(const PowerBookmarkService&) = delete;

  ~PowerBookmarkService() override;

  // For sync codebase only: instantiates a controller delegate to interact with
  // PowerBookmarkSyncBridge. Must be called from the UI thread.
  std::unique_ptr<syncer::DataTypeControllerDelegate>
  CreateSyncControllerDelegate();

  // Returns a vector of Powers for the given `url` through the given
  // `callback`. Use `power_type` to restrict which type is returned or use
  // POWER_TYPE_UNSPECIFIED to return everything.
  void GetPowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
      PowersCallback callback);

  // Returns a vector of PowerOverviews for the given `power_type` through the
  // given `callback`. The PowerOverviews will be sorted based on count, with
  // the highest count overviews coming first. The sample power in each overview
  // will be the most recently modified power.
  void GetPowerOverviewsForType(
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
      PowerOverviewsCallback callback);

  // Returns a vector of Powers matching the given `search_params`. The results
  // are ordered by the url they're associated with.
  void SearchPowers(const SearchParams& search_params, PowersCallback callback);

  // Returns a vector of PowerOverviews matching the given `search_params`. The
  // results:
  //   - are ordered by their url and PowerType.
  //   - have the same `PowerOverview.count` value as if returned by
  //     GetPowerOverviewsForType - which is the number of Powers with the same
  //     url and type.
  //   - have the `PowerOverview.power` that is the most recently modified Power
  //     which matches the `search_params`.
  void SearchPowerOverviews(const SearchParams& search_params,
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
  // TODO(crbug.com/40875199): Encapsulate the storage key if possible.
  void DeletePower(const base::Uuid& guid, SuccessCallback callback);

  // Delete all powers for the given `url`. Success of the operation is
  // returned through the given `callback`. Use `power_type` to restrict which
  // type is deleted or use POWER_TYPE_UNSPECIFIED to delete everything.
  void DeletePowersForURL(
      const GURL& url,
      const sync_pb::PowerBookmarkSpecifics::PowerType& power_type,
      SuccessCallback callback);

  // Registration methods for observers.
  void AddObserver(PowerBookmarkObserver* observer);
  void RemoveObserver(PowerBookmarkObserver* observer);

  // Allow features to receive notification when a bookmark node is created to
  // add extra information. The `data_provider` can be removed with the remove
  // method.
  void AddDataProvider(PowerBookmarkDataProvider* data_provider);
  void RemoveDataProvider(PowerBookmarkDataProvider* data_provider);

  // BaseBookmarkModelObserver implementation.
  void BookmarkNodeAdded(const bookmarks::BookmarkNode* parent,
                         size_t index,
                         bool newly_added) override;
  void BookmarkModelChanged() override {}

  // PowerBookmarkObserver implementation.
  void OnPowersChanged() override;

 private:
  raw_ptr<bookmarks::BookmarkModel> model_;
  std::unique_ptr<PowerBookmarkBackend> backend_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  base::ObserverList<PowerBookmarkObserver>::Unchecked observers_;
  std::vector<raw_ptr<PowerBookmarkDataProvider, VectorExperimental>>
      data_providers_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PowerBookmarkService> weak_ptr_factory_{this};
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_
