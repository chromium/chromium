// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_bookmark_model_observer.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "build/buildflag.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/commerce/core/subscriptions/subscriptions_manager.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"

namespace commerce {

namespace {
bool IsTrackByDefaultEnabled() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(kTrackByDefaultOnMobile);
#else
  return true;
#endif
}
}  // namespace

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
    const bookmarks::BookmarkNode* node) {
  // Since the node is about to change, map its current known URL.
  node_to_url_map_[node->uuid()] = node->url();

  // Specifically track changes to parent and title for the shopping collection.
  if (IsShoppingCollectionBookmarkFolder(node)) {
    shopping_collection_name_before_change_ = node->GetTitle();
  }
}

void ShoppingBookmarkModelObserver::BookmarkNodeChanged(
    const bookmarks::BookmarkNode* node) {
  bookmarks::BookmarkModel* model = scoped_observation_.GetSource();
  CHECK(model);

  if (IsShoppingCollectionBookmarkFolder(node) &&
      shopping_collection_name_before_change_.value() != node->GetTitle()) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.NameChanged"));

    shopping_collection_name_before_change_.reset();
  }

  if (node_to_url_map_[node->uuid()] != node->url()) {
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

  node_to_url_map_.erase(node->uuid());
}

void ShoppingBookmarkModelObserver::BookmarkNodeAdded(
    const bookmarks::BookmarkNode* parent,
    size_t index,
    bool added_by_user) {
  const bookmarks::BookmarkNode* node = parent->children()[index].get();
  bookmarks::BookmarkModel* model = scoped_observation_.GetSource();
  CHECK(model);

  if (IsShoppingCollectionBookmarkFolder(node)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.Created"));
  }

  if (model->IsLocalOnlyNode(*node)) {
    return;
  }

  // TODO(b:287289351): We should consider listening to metadata changes
  //                    instead. Presumably, shopping data is primarily being
  //                    added to new bookmarks, so we could potentially use the
  //                    node change event.
  if (added_by_user && IsTrackByDefaultEnabled()) {
    SetPriceTrackingStateForBookmark(shopping_service_, model, node, true,
                                     base::DoNothing());
  }
}

void ShoppingBookmarkModelObserver::BookmarkNodeMoved(
    const bookmarks::BookmarkNode* old_parent,
    size_t old_index,
    const bookmarks::BookmarkNode* new_parent,
    size_t new_index) {
  const bookmarks::BookmarkNode* node = new_parent->children()[new_index].get();
  if (IsShoppingCollectionBookmarkFolder(node)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.ParentChanged"));
  }

  // Record movement of bookmarks into or out of the collection.
  if (IsShoppingCollectionBookmarkFolder(old_parent)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.MovedFromCollection"));
  } else if (IsShoppingCollectionBookmarkFolder(new_parent)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.MovedToCollection"));
  }
}

void ShoppingBookmarkModelObserver::OnWillRemoveBookmarks(
    const bookmarks::BookmarkNode* parent,
    size_t old_index,
    const bookmarks::BookmarkNode* node,
    const base::Location& location) {
  if (node->is_folder()) {
    std::set<uint64_t> unsubscribed_ids;
    HandleFolderDeletion(node, &unsubscribed_ids);
  } else {
    HandleNodeDeletion(node, nullptr, nullptr);
  }
}

void ShoppingBookmarkModelObserver::HandleFolderDeletion(
    const bookmarks::BookmarkNode* node,
    std::set<uint64_t>* unsubscribed_ids) {
  CHECK(node && node->is_folder());

  if (IsShoppingCollectionBookmarkFolder(node)) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.ShoppingCollection.Deleted"));
  }

  for (const auto& child : node->children()) {
    if (child->is_folder()) {
      HandleFolderDeletion(child.get(), unsubscribed_ids);
    } else {
      HandleNodeDeletion(child.get(), node, unsubscribed_ids);
    }
  }
}

void ShoppingBookmarkModelObserver::HandleNodeDeletion(
    const bookmarks::BookmarkNode* node,
    const bookmarks::BookmarkNode* folder_being_deleted,
    std::set<uint64_t>* unsubscribed_ids) {
  CHECK(node && !node->is_folder());
  CHECK(!folder_being_deleted || (folder_being_deleted && unsubscribed_ids));

  bookmarks::BookmarkModel* model = scoped_observation_.GetSource();
  CHECK(model);

  // If the number of bookmarks with the node's cluster ID is 1, we can
  // unsubscribe from the product since deleting this node will result in 0.
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  if (!meta || !meta->has_shopping_specifics()) {
    return;
  }

  power_bookmarks::ShoppingSpecifics* specifics =
      meta->mutable_shopping_specifics();

  std::vector<const bookmarks::BookmarkNode*> bookmarks_with_cluster =
      GetBookmarksWithClusterId(model, specifics->product_cluster_id());

  // Determine the number of duplicates that are in the folder being deleted.
  // Skip any previously unsubscribed IDs.
  if (folder_being_deleted &&
      !base::Contains(*unsubscribed_ids, specifics->product_cluster_id())) {
    size_t duplicates_in_deleted_folder = 0;
    for (const bookmarks::BookmarkNode* duplicate : bookmarks_with_cluster) {
      if (bookmarks::IsDescendantOf(duplicate, folder_being_deleted)) {
        duplicates_in_deleted_folder++;
      }
    }

    // If all the duplicates for a product aren't in the folder being deleted,
    // do nothing.
    if (bookmarks_with_cluster.size() > duplicates_in_deleted_folder) {
      return;
    }
  } else if (bookmarks_with_cluster.size() > 1) {
    return;
  }

  if (unsubscribed_ids) {
    unsubscribed_ids->insert(specifics->product_cluster_id());
  }
  SetPriceTrackingStateForBookmark(shopping_service_, model, node, false,
                                   base::BindOnce([](bool success) {}));
}

void ShoppingBookmarkModelObserver::BookmarkMetaInfoChanged(
    const bookmarks::BookmarkNode* node) {
  bookmarks::BookmarkModel* model = scoped_observation_.GetSource();
  CHECK(model);

  std::optional<int64_t> last_subscription_change_time =
      GetBookmarkLastSubscriptionChangeTime(model, node);
  if (last_subscription_change_time.has_value() && subscriptions_manager_) {
    subscriptions_manager_->CheckTimestampOnBookmarkChange(
        last_subscription_change_time.value());
  }
}

}  // namespace commerce
