// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_

#include <vector>

#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/power_bookmarks/core/power_bookmark_data_provider.h"

namespace power_bookmarks {

class PowerBookmarkService : public KeyedService,
                             public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit PowerBookmarkService(bookmarks::BookmarkModel* model);
  ~PowerBookmarkService() override;

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

  std::vector<PowerBookmarkDataProvider*> data_providers_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWER_BOOKMARK_SERVICE_H_