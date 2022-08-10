// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace commerce {

class ShoppingService;

// Return whether a bookmark is price tracked. This does not check the
// subscriptions backend, only the flag in the bookmark meta.
bool IsBookmarkPriceTracked(bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* node);

// Set the state of price tracking for all bookmarks with the cluster ID of the
// provided bookmark. A subscription update will attempted on the backend and,
// if successful, all bookmarks with the same cluster ID will be updated.
// |callback| will be called with a bool representing whether the operation was
// successful iff all of |service|, |model|, and |node| are non-null and the
// bookmark has been determined to be a product.
void SetPriceTrackingStateForBookmark(ShoppingService* service,
                                      bookmarks::BookmarkModel* model,
                                      const bookmarks::BookmarkNode* node,
                                      bool enabled,
                                      base::OnceCallback<void(bool)> callback);

// Get all bookmarks with the specified product cluster ID.
std::vector<const bookmarks::BookmarkNode*> GetBookmarksWithClusterId(
    bookmarks::BookmarkModel* model,
    uint64_t cluster_id);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
