// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_BOOKMARK_UPDATE_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_BOOKMARK_UPDATE_MANAGER_H_

#include <memory>
#include <queue>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/shopping_service.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/prefs/pref_service.h"

class GURL;

namespace commerce {

// This object manages regular updates of product information stored in
// bookmarks. This object should be tied to the life of the profile and
// outlive the shopping service.
class BookmarkUpdateManager {
 public:
  BookmarkUpdateManager(ShoppingService* service,
                        bookmarks::BookmarkModel* model,
                        PrefService* prefs);
  BookmarkUpdateManager(const BookmarkUpdateManager&) = delete;
  BookmarkUpdateManager& operator=(const BookmarkUpdateManager&) = delete;
  ~BookmarkUpdateManager();

  // Schedule an update for product bookmarks. If the amount of time since the
  // last update is too long, the update will attempt to run as soon as
  // possible. Otherwise, the initial update after this call will be the
  // interval minus the delta since the last update.
  void ScheduleUpdate();

  // Cancel any scheduled updates.
  void CancelUpdates();

 private:
  friend class BookmarkUpdateManagerTest;

  // Execute the logic that will update product bookmarks.
  void RunUpdate();

  // Process the next list in |pending_update_batches_|.
  void StartNextBatch();

  // Handle the response from the shopping service's on-demand API. This will
  // update the corresponding bookmark if there is new information.
  void HandleOnDemandResponse(const int64_t bookmark_id,
                              const GURL& url,
                              std::optional<ProductInfo> info);

  raw_ptr<ShoppingService> shopping_service_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<PrefService> pref_service_;

  // Keep track of the last updated time in memory in case there is a failure in
  // the pref service.
  base::Time last_update_time_;

  // A queue of lists of bookmark IDs that need to be updated. This is needed
  // because there is a hard limit to the number of items the backing update
  // system (optimization guide) can handle at a time.
  std::queue<std::vector<int64_t>> pending_update_batches_;

  // The expected number of bookmark updates for the currently running batch and
  // the number of updates received. The callback pushes updates one at a time,
  // so we need to keep track of how many have been received here so we know
  // when to start the next batch.
  size_t expected_bookmark_updates_;
  size_t received_bookmark_updates_;

  std::unique_ptr<base::CancelableOnceClosure> scheduled_task_;

  base::WeakPtrFactory<BookmarkUpdateManager> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_BOOKMARK_UPDATE_MANAGER_H_
