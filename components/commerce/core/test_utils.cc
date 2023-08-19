// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/test_utils.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {

const bookmarks::BookmarkNode* AddProductBookmark(
    bookmarks::BookmarkModel* bookmark_model,
    const std::u16string& title,
    const GURL& url,
    uint64_t cluster_id,
    bool is_price_tracked,
    const int64_t price_micros,
    const std::string& currency_code,
    const absl::optional<int64_t>& last_subscription_change_time) {
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->other_node(), 0, title, url,
                             nullptr, absl::nullopt, absl::nullopt, true);

  AddProductInfoToExistingBookmark(
      bookmark_model, node, title, cluster_id, is_price_tracked, price_micros,
      currency_code, last_subscription_change_time);

  return node;
}

void AddProductInfoToExistingBookmark(
    bookmarks::BookmarkModel* bookmark_model,
    const bookmarks::BookmarkNode* bookmark_node,
    const std::u16string& title,
    uint64_t cluster_id,
    bool is_price_tracked,
    const int64_t price_micros,
    const std::string& currency_code,
    const absl::optional<int64_t>& last_subscription_change_time) {
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  power_bookmarks::ShoppingSpecifics* specifics =
      meta->mutable_shopping_specifics();
  specifics->set_title(base::UTF16ToUTF8(title));
  specifics->set_product_cluster_id(cluster_id);
  specifics->set_is_price_tracked(is_price_tracked);

  specifics->mutable_current_price()->set_currency_code(currency_code);
  specifics->mutable_current_price()->set_amount_micros(price_micros);

  if (last_subscription_change_time.has_value()) {
    specifics->set_last_subscription_change_time(
        last_subscription_change_time.value());
  }

  power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model, bookmark_node,
                                            std::move(meta));
}

CommerceSubscription CreateUserTrackedSubscription(uint64_t cluster_id) {
  return CommerceSubscription(
      SubscriptionType::kPriceTrack, IdentifierType::kProductClusterId,
      base::NumberToString(cluster_id), ManagementType::kUserManaged);
}

void SetShoppingListEnterprisePolicyPref(PrefService* prefs, bool enabled) {
  prefs->SetBoolean(kShoppingListEnabledPrefName, enabled);
}

absl::optional<PriceInsightsInfo> CreateValidPriceInsightsInfo(
    bool has_price_range_data,
    bool has_price_history_data,
    PriceBucket price_bucket) {
  assert(has_price_history_data || has_price_range_data);

  absl::optional<PriceInsightsInfo> info;
  info.emplace();
  if (has_price_range_data) {
    info->typical_low_price_micros.emplace(kTypicalLowPriceMicros);
    info->typical_high_price_micros.emplace(kTypicalHighPriceMicros);
  }
  if (has_price_history_data) {
    int64_t price;
    switch (price_bucket) {
      case PriceBucket::kLowPrice:
        price = kTypicalLowPriceMicros;
        break;
      case PriceBucket::kTypicalPrice:
        price = kTypicalPriceMicros;
        break;
      case PriceBucket::kHighPrice:
        price = kTypicalHighPriceMicros;
        break;
      default:
        price = kTypicalPriceMicros;
    }
    std::vector<std::tuple<std::string, int64_t>> history_prices{
        std::tuple<std::string, int64_t>{kDate, price}};
    info->catalog_history_prices = history_prices;
  }
  info->price_bucket = price_bucket;

  return info;
}

}  // namespace commerce
