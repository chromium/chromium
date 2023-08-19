// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_bookmark_model_observer.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace commerce {

ShoppingBookmarkModelObserver::ShoppingBookmarkModelObserver(
    bookmarks::BookmarkModel* model,
    ShoppingService* shopping_service,
    SubscriptionsManager* subscriptions_manager)
    : shopping_service_(shopping_service),
      subscriptions_manager_(subscriptions_manager) {
  scoped_observation_.Observe(model);
}

ShoppingBookmarkModelObserver::~ShoppingBookmarkModelObserver() = default;

void ShoppingBookmarkModelObserver::BookmarkModelChanged() {}

void ShoppingBookmarkModelObserver::OnWillChangeBookmarkNode(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  // Since the node is about to change, map its current known URL.
  node_to_url_map_[node->id()] = node->url();

  // Specifically track changes to parent and title for the shopping collection.
  if (IsShoppingCollectionBookmarkFolder(node)) {
    shopping_collection_name_before_change_ = node->GetTitle();
  }
}

void ShoppingBookmarkModelObserver::BookmarkNodeChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (IsShoppingCollectionBookmarkFolder(node) &&
      shopping_collection_name_before_change_.value() != node->GetTitle()) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.NameChanged"));

    shopping_collection_name_before_change_.reset();
  }

  if (node_to_url_map_[node->id()] != node->url()) {
    // If the URL did change, clear the power bookmark shopping meta and
    // unsubscribe if needed.
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(model, node);

    if (meta && meta->has_shopping_specifics()) {
      power_bookmarks::ShoppingSpecifics* specifics =
          meta->mutable_shopping_specifics();

      uint64_t cluster_id = specifics->product_cluster_id();

      // Unsubscribe using the cluster ID
      if (shopping_service_) {
        std::vector<const bookmarks::BookmarkNode*> bookmarks_with_cluster =
            GetBookmarksWithClusterId(model, cluster_id);

        // If there are no other bookmarks with the node's cluster ID,
        // unsubscribe.
        if (bookmarks_with_cluster.size() <= 1) {
          SetPriceTrackingStateForBookmark(shopping_service_, model, node,
                                           false,
                                           base::BindOnce([](bool success) {}));
        }
      }

      meta->clear_shopping_specifics();
      power_bookmarks::SetNodePowerBookmarkMeta(model, node, std::move(meta));
    }
  }

  node_to_url_map_.erase(node->id());
}

void ShoppingBookmarkModelObserver::BookmarkNodeAdded(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  const bookmarks::BookmarkNode* node = parent->children()[index].get();

  if (IsShoppingCollectionBookmarkFolder(node)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.Created"));
  }

  // TODO(b:287289351): We should consider listening to metadata changes
  //                    instead. Presumably, shopping data is primarily being
  //                    added to new bookmarks, so we could potentially use the
  //                    node change event.
  if (added_by_user &&
      base::FeatureList::IsEnabled(kShoppingListTrackByDefault)) {
    SetPriceTrackingStateForBookmark(shopping_service_, model, node, true,
                                     base::DoNothing());
  }
}

void ShoppingBookmarkModelObserver::BookmarkNodeMoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();
  if (IsShoppingCollectionBookmarkFolder(node)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.ParentChanged"));
  }
}

void ShoppingBookmarkModelObserver::BookmarkNodeRemoved(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  if (IsShoppingCollectionBookmarkFolder(node)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.Deleted"));
  }

  // If the number of bookmarks with the node's cluster ID is now 0, unsubscribe
  // from the product.
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  if (!meta || !meta->has_shopping_specifics())
    return;

  power_bookmarks::ShoppingSpecifics* specifics =
      meta->mutable_shopping_specifics();

  std::vector<const bookmarks::BookmarkNode*> bookmarks_with_cluster =
      GetBookmarksWithClusterId(model, specifics->product_cluster_id());

  // If there are other bookmarks with the node's cluster ID, do nothing.
  if (!bookmarks_with_cluster.empty())
    return;

  SetPriceTrackingStateForBookmark(shopping_service_, model, node, false,
                                   base::BindOnce([](bool success) {}));
}

void ShoppingBookmarkModelObserver::BookmarkMetaInfoChanged(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  absl::optional<int64_t> last_subscription_change_time =
      GetBookmarkLastSubscriptionChangeTime(model, node);
  if (last_subscription_change_time.has_value() && subscriptions_manager_) {
    subscriptions_manager_->CheckTimestampOnBookmarkChange(
        last_subscription_change_time.value());
  }
}

}  // namespace commerce
