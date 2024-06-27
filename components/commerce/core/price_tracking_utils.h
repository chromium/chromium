// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"

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
std::optional<int64_t> GetBookmarkLastSubscriptionChangeTime(
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

// Gets the user preference for price drop notifications. If not set, the
// default value will be returned.
bool GetEmailNotificationPrefValue(PrefService* pref_service);

// Gets whether the price drop email notification preference has been explicitly
// set by the user or is still in the default state.
bool IsEmailNotificationPrefSetByUser(PrefService* pref_service);

// Builds a user-managed price tracking subscription object for the provided
// cluster ID. This does not change the state of the subscription, it only
// creates the object representing the subscription.
CommerceSubscription BuildUserSubscriptionForClusterId(uint64_t cluster_id);

// Returns whether price tracking can be initiated given either a ProductInfo
// or a ShoppingSpecifics object.
bool CanTrackPrice(const ProductInfo& info);
bool CanTrackPrice(const std::optional<ProductInfo>& info);
bool CanTrackPrice(const power_bookmarks::ShoppingSpecifics& specifics);

// If `url` is bookmarked, returns the name of the parent folder; otherwise
// returns an empty string.
std::optional<std::u16string> GetBookmarkParentName(
    bookmarks::BookmarkModel* model,
    const GURL& url);

// Gets the explicit "shopping collection" bookmark folder. There can only be
// one shopping collection per profile.
const bookmarks::BookmarkNode* GetShoppingCollectionBookmarkFolder(
    bookmarks::BookmarkModel* model,
    bool create_if_needed = false);

// Returns whether the provided node is the shopping collection folder.
bool IsShoppingCollectionBookmarkFolder(const bookmarks::BookmarkNode* node);

// Gets the product cluster ID for the bookmark represented by the provided URL.
// If there is no bookmark or the bookmark doesn't have a cluster ID,
// std::nullopt is returned.
std::optional<uint64_t> GetProductClusterIdFromBookmark(
    const GURL& url,
    bookmarks::BookmarkModel* model);

// Removes any subscriptions the user might have that are not tied to at least
// one bookmark. The count of the number of dangling subscriptions will be
// returned as part of the optionally provided callback.
void RemoveDanglingSubscriptions(
    ShoppingService* shopping_service,
    bookmarks::BookmarkModel* bookmark_model,
    base::OnceCallback<void(size_t)> completed_callback =
        base::DoNothingAs<void(size_t)>());

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_PRICE_TRACKING_UTILS_H_
