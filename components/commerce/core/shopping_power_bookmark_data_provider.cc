// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_power_bookmark_data_provider.h"

#include "components/commerce/core/shopping_service.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
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
  absl::optional<commerce::ProductInfo> info =
      shopping_service_->GetAvailableProductInfoForUrl(node->url());

  if (info.has_value()) {
    meta->mutable_lead_image()->set_url(info->image_url.spec());

    power_bookmarks::ShoppingSpecifics* specifics =
        meta->mutable_shopping_specifics();
    specifics->set_title(info->title);
    specifics->mutable_current_price()->set_amount_micros(info->amount_micros);
    specifics->mutable_current_price()->set_currency_code(info->currency_code);
    specifics->set_product_cluster_id(info->product_cluster_id);
    specifics->set_offer_id(info->offer_id);
    specifics->set_country_code(info->country_code);
  }
}

}  // namespace commerce
