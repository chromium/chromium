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
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/protocol/power_bookmark_specifics.pb.h"

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

namespace power_bookmarks {
class PowerBookmarkDataProvider;

// Provides a public API surface for power bookmarks.
class PowerBookmarkService : public KeyedService,
                             public bookmarks::BaseBookmarkModelObserver {
 public:
  // `model` is an instance of BookmarkModel, used to query bookmarks and
  // register as an observer.
  explicit PowerBookmarkService(bookmarks::BookmarkModel* model);
  PowerBookmarkService(const PowerBookmarkService&) = delete;
  PowerBookmarkService& operator=(const PowerBookmarkService&) = delete;

  ~PowerBookmarkService() override;

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

 private:
  raw_ptr<bookmarks::BookmarkModel> model_;
  std::vector<raw_ptr<PowerBookmarkDataProvider, VectorExperimental>>
      data_providers_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      model_observation_{this};
  base::WeakPtrFactory<PowerBookmarkService> weak_ptr_factory_{this};
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_
