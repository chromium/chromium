// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace power_bookmarks {
class ShoppingSpecifics;
}  // namespace power_bookmarks

namespace commerce {

struct CommerceSubscription;
struct ProductInfo;
class ShoppingService;

// Return whether a bookmark is price tracked. The result is passed to
// |callback|.
void IsBookmarkPriceTracked(ShoppingService* service,
                            bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* node,
                            base::OnceCallback<void(bool)> callback);

// Return whether the |node| is a product bookmark.
bool IsProductBookmark(bookmarks::BookmarkModel* model,
                       const bookmarks::BookmarkNode* node);

// Return the last timestamp when the product is successfully tracked or
// untracked by the user.
absl::optional<int64_t> GetBookmarkLastSubscriptionChangeTime(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node);

// Set the price tracking state for a particular cluster ID. This function
// assumes that a bookmark with the specified cluster ID already exists and
// will search for that bookmark (or the first instance of it). The logic
// performed after that point will be the same as
// SetPriceTrackingStateForBookmark.
void SetPriceTrackingStateForClusterId(ShoppingService* service,
                                       bookmarks::BookmarkModel* model,
                                       const uint64_t cluster_id,
                                       bool enabled,
                                       base::OnceCallback<void(bool)> callback);

// Set the state of price tracking for all bookmarks with the cluster ID of the
// provided bookmark. A subscription update will attempted on the backend and,
// if successful, all bookmarks with the same cluster ID will be updated.
// |callback| will be called with a bool representing whether the operation was
// successful iff all of |service|, |model|, and |node| are non-null and the
// bookmark has been determined to be a product.
void SetPriceTrackingStateForBookmark(
    ShoppingService* service,
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node,
    bool enabled,
    base::OnceCallback<void(bool)> callback,
    bool was_bookmark_created_by_price_tracking = false);

// Get all bookmarks with the specified product cluster ID. If |max_count| is
// specified, this function will return that number of bookmarks at most,
// otherwise all bookmarks with the specified cluster ID will be returned.
std::vector<const bookmarks::BookmarkNode*> GetBookmarksWithClusterId(
    bookmarks::BookmarkModel* model,
    uint64_t cluster_id,
    size_t max_count = 0);

// Gets all bookmarks that are price tracked. This method may make a call to the
// subscriptions backend if the information is stale. The list of price tracked
// bookmarks is provided as a param to the callback passed to this function.
// Ownership of the vector of bookmark nodes is transferred to the caller, but
// the individual bookmarks that the pointers reference are not -- those exist
// only as long as the BookmarkModel does (which is bound to the browser
// context).
void GetAllPriceTrackedBookmarks(
    ShoppingService* shopping_service,
    bookmarks::BookmarkModel* bookmark_model,
    base::OnceCallback<void(std::vector<const bookmarks::BookmarkNode*>)>
        callback);

// Get all shopping bookmarks. The returned vector of BookmarkNodes is owned by
// the caller, but the nodes pointed to are not -- those live for as long as
// the BookmarkModel (|model|) is alive which has the same lifetime as the
// current BrowserContext.
std::vector<const bookmarks::BookmarkNode*> GetAllShoppingBookmarks(
    bookmarks::BookmarkModel* model);

// Populate or update the provided |out_meta| with information from |info|. The
// returned boolean indicated whether any information actually changed.
bool PopulateOrUpdateBookmarkMetaIfNeeded(
    power_bookmarks::PowerBookmarkMeta* out_meta,
    const ProductInfo& info);

// Attempts to enable price email notifications for users. This will only set
// the setting to true if it is the first time being called, after that this is
// a noop.
void MaybeEnableEmailNotifications(PrefService* pref_service);

// Whether the email notification is explicitly disabled by the user. Return
// false if we are using the default preference value.
bool IsEmailDisabledByUser(PrefService* pref_service);

// Build a user-tracked price tracking subscription object for the provided
// cluster ID.
CommerceSubscription BuildUserSubscriptionForClusterId(uint64_t cluster_id);

// Returns whether price tracking can be initiated given either a ProductInfo
// or a ShoppingSpecifics object.
bool CanTrackPrice(const ProductInfo& info);
bool CanTrackPrice(const absl::optional<ProductInfo>& info);
bool CanTrackPrice(const power_bookmarks::ShoppingSpecifics& specifics);

// If `url` is bookmarked, returns the name of the parent folder; otherwise
// returns the name of the Other Bookmarks folder.
const std::u16string& GetBookmarkParentNameOrDefault(
    bookmarks::BookmarkModel* model,
    const GURL& url);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
