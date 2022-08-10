// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/price_tracking_utils.h"

#include <memory>

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"

namespace commerce {

namespace {

// Update the bookmarks affected by the subscribe or unsubscribe event if it was
// successful.
void UpdateBookmarksForSubscriptionsResult(
    base::WeakPtr<bookmarks::BookmarkModel> model,
    base::OnceCallback<void(bool)> callback,
    bool enabled,
    uint64_t cluster_id,
    bool success) {
  if (success) {
    std::vector<const bookmarks::BookmarkNode*> results;
    power_bookmarks::PowerBookmarkQueryFields query;
    query.type = power_bookmarks::PowerBookmarkType::SHOPPING;
    power_bookmarks::GetBookmarksMatchingProperties(model.get(), query, -1,
                                                    &results);

    for (const auto* node : results) {
      std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
          power_bookmarks::GetNodePowerBookmarkMeta(model.get(), node);

      if (!meta)
        continue;

      power_bookmarks::ShoppingSpecifics* specifics =
          meta->mutable_shopping_specifics();

      if (!specifics || specifics->product_cluster_id() != cluster_id)
        continue;

      specifics->set_is_price_tracked(enabled);

      power_bookmarks::SetNodePowerBookmarkMeta(model.get(), node,
                                                std::move(meta));
    }
  }

  std::move(callback).Run(success);
}

}  // namespace

bool IsBookmarkPriceTracked(bookmarks::BookmarkModel* model,
                            const bookmarks::BookmarkNode* node) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  return meta && meta->has_shopping_specifics() &&
         meta->shopping_specifics().is_price_tracked();
}

void SetPriceTrackingStateForBookmark(ShoppingService* service,
                                      bookmarks::BookmarkModel* model,
                                      const bookmarks::BookmarkNode* node,
                                      bool enabled,
                                      base::OnceCallback<void(bool)> callback) {
  if (!service || !model || !node)
    return;

  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      power_bookmarks::GetNodePowerBookmarkMeta(model, node);

  if (!meta || !meta->has_shopping_specifics())
    return;

  power_bookmarks::ShoppingSpecifics* specifics =
      meta->mutable_shopping_specifics();

  if (!specifics || !specifics->has_product_cluster_id())
    return;

  std::unique_ptr<std::vector<CommerceSubscription>> subs =
      std::make_unique<std::vector<CommerceSubscription>>();

  CommerceSubscription sub(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(specifics->product_cluster_id()),
      ManagementType::kUserManaged);

  subs->push_back(std::move(sub));

  base::OnceCallback<void(bool)> update_bookmarks_callback = base::BindOnce(
      &UpdateBookmarksForSubscriptionsResult, model->AsWeakPtr(),
      std::move(callback), enabled, specifics->product_cluster_id());

  if (enabled) {
    service->Subscribe(std::move(subs), std::move(update_bookmarks_callback));
  } else {
    service->Unsubscribe(std::move(subs), std::move(update_bookmarks_callback));
  }
}

std::vector<const bookmarks::BookmarkNode*> GetBookmarksWithClusterId(
    bookmarks::BookmarkModel* model,
    uint64_t cluster_id) {
  std::vector<const bookmarks::BookmarkNode*> results;
  power_bookmarks::PowerBookmarkQueryFields query;
  query.type = power_bookmarks::PowerBookmarkType::SHOPPING;
  power_bookmarks::GetBookmarksMatchingProperties(model, query, -1, &results);

  std::vector<const bookmarks::BookmarkNode*> bookmarks_with_cluster;
  for (const auto* node : results) {
    std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
        power_bookmarks::GetNodePowerBookmarkMeta(model, node);

    if (!meta)
      continue;

    power_bookmarks::ShoppingSpecifics* specifics =
        meta->mutable_shopping_specifics();

    if (!specifics || specifics->product_cluster_id() != cluster_id)
      continue;

    bookmarks_with_cluster.push_back(node);
  }

  return bookmarks_with_cluster;
}

}  // namespace commerce
