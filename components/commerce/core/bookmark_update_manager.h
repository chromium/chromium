// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_BOOKMARK_UPDATE_MANAGER_H_
#define COMPONENTS_COMMERCE_CORE_BOOKMARK_UPDATE_MANAGER_H_

#include <memory>

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

  // Handle the response from the shopping service's on-demand API. This will
  // update the corresponding bookmark if there is new information.
  void HandleOnDemandResponse(const int64_t bookmark_id,
                              const GURL& url,
                              absl::optional<ProductInfo> info);

  raw_ptr<ShoppingService> shopping_service_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<PrefService> pref_service_;

  // Keep track of the last updated time in memory in case there is a failure in
  // the pref service.
  base::Time last_update_time_;
  std::unique_ptr<base::CancelableOnceClosure> scheduled_task_;

  base::WeakPtrFactory<BookmarkUpdateManager> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_BOOKMARK_UPDATE_MANAGER_H_
