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
#include "components/optimization_guide/core/feature_registry/enterprise_policy_registry.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
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
    const std::optional<int64_t>& last_subscription_change_time) {
  // Prefer account bookmarks if available. `other_node()` is still relevant for
  // tests that continue to exercise the legacy sync-feature-on case.
  const bookmarks::BookmarkNode* parent =
      (bookmark_model->account_other_node() != nullptr)
          ? bookmark_model->account_other_node()
          : bookmark_model->other_node();

  const bookmarks::BookmarkNode* node = bookmark_model->AddURL(
      parent, 0, title, url, nullptr, std::nullopt, std::nullopt, true);

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
    const std::optional<int64_t>& last_subscription_change_time) {
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

void SetShoppingListEnterprisePolicyPref(TestingPrefServiceSimple* prefs,
                                         bool enabled) {
  prefs->SetManagedPref(kShoppingListEnabledPrefName, base::Value(enabled));
}

void RegisterCommercePrefs(PrefRegistrySimple* registry) {
  RegisterPrefs(registry);
  registry->RegisterIntegerPref(
      optimization_guide::prefs::kProductSpecificationsEnterprisePolicyAllowed,
      0);
}

void SetTabCompareEnterprisePolicyPref(TestingPrefServiceSimple* prefs,
                                       int enabled_state) {
  prefs->SetManagedPref(
      optimization_guide::prefs::kProductSpecificationsEnterprisePolicyAllowed,
      base::Value(enabled_state));
}

std::optional<PriceInsightsInfo> CreateValidPriceInsightsInfo(
    bool has_price_range_data,
    bool has_price_history_data,
    PriceBucket price_bucket) {
  assert(has_price_history_data || has_price_range_data);

  std::optional<PriceInsightsInfo> info;
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

DiscountInfo CreateValidDiscountInfo(const std::string& detail,
                                     const std::string& terms_and_conditions,
                                     const std::string& value_in_text,
                                     const std::string& discount_code,
                                     int64_t id,
                                     bool is_merchant_wide,
                                     double expiry_time_sec,
                                     DiscountClusterType cluster_type) {
  DiscountInfo discount_info;

  discount_info.cluster_type = cluster_type;
  discount_info.description_detail = detail;
  discount_info.terms_and_conditions.emplace(terms_and_conditions);
  discount_info.value_in_text = value_in_text;
  discount_info.discount_code = discount_code;
  discount_info.id = id;
  discount_info.is_merchant_wide = is_merchant_wide;
  discount_info.expiry_time_sec = expiry_time_sec;

  return discount_info;
}

}  // namespace commerce
