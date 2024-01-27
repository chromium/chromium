// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_power_bookmark_data_provider.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"

using power_bookmarks::PowerBookmarkService;

namespace commerce {

ShoppingPowerBookmarkDataProvider::ShoppingPowerBookmarkDataProvider(
    PowerBookmarkService* power_bookmark_service,
    ShoppingService* shopping_service)
    : power_bookmark_service_(power_bookmark_service),
      shopping_service_(shopping_service) {
  power_bookmark_service_->AddDataProvider(this);
}

ShoppingPowerBookmarkDataProvider::~ShoppingPowerBookmarkDataProvider() {
  power_bookmark_service_->RemoveDataProvider(this);
}

void ShoppingPowerBookmarkDataProvider::AttachMetadataForNewBookmark(
    const bookmarks::BookmarkNode* node,
    power_bookmarks::PowerBookmarkMeta* meta) {
  std::optional<commerce::ProductInfo> info =
      shopping_service_->GetAvailableProductInfoForUrl(node->url());

  if (info.has_value()) {
    bool changed = PopulateOrUpdateBookmarkMetaIfNeeded(meta, info.value());

    // The bookmark info should always change for new bookmarks.
    CHECK(changed);
  }
}

}  // namespace commerce
