// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_power_bookmark_data_provider.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"

using power_bookmarks::PowerBookmarkService;

namespace commerce {

ShoppingPowerBookmarkDataProvider::ShoppingPowerBookmarkDataProvider(
    bookmarks::BookmarkModel* bookmark_model,
    PowerBookmarkService* power_bookmark_service,
    ShoppingService* shopping_service)
    : bookmark_model_(bookmark_model),
      power_bookmark_service_(power_bookmark_service),
      shopping_service_(shopping_service) {
  power_bookmark_service_->AddDataProvider(this);
}

ShoppingPowerBookmarkDataProvider::~ShoppingPowerBookmarkDataProvider() {
  power_bookmark_service_->RemoveDataProvider(this);
}

void ShoppingPowerBookmarkDataProvider::AttachMetadataForNewBookmark(
    const bookmarks::BookmarkNode* node,
    power_bookmarks::PowerBookmarkMeta* meta) {
  absl::optional<commerce::ProductInfo> info =
      shopping_service_->GetAvailableProductInfoForUrl(node->url());

  if (info.has_value()) {
    shopping_service_->IsClusterIdTrackedByUser(
        info->product_cluster_id,
        base::BindOnce(
            [](base::WeakPtr<bookmarks::BookmarkModel> model,
               uint64_t bookmark_id, bool is_tracked) {
              if (!is_tracked)
                return;
              const bookmarks::BookmarkNode* existing_node =
                  bookmarks::GetBookmarkNodeByID(model.get(), bookmark_id);

              CHECK(existing_node);
              std::unique_ptr<power_bookmarks::PowerBookmarkMeta>
                  existing_meta = power_bookmarks::GetNodePowerBookmarkMeta(
                      model.get(), existing_node);

              existing_meta->mutable_shopping_specifics()->set_is_price_tracked(
                  true);

              power_bookmarks::SetNodePowerBookmarkMeta(
                  model.get(), existing_node, std::move(existing_meta));
            },
            bookmark_model_->AsWeakPtr(), node->id()));
    bool changed = PopulateOrUpdateBookmarkMetaIfNeeded(meta, info.value());

    // The bookmark info should always change for new bookmarks.
    CHECK(changed);
  }
}

}  // namespace commerce
