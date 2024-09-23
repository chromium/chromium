// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_TEST_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_TEST_UTILS_H_

#include <optional>
#include <string>

#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;
class PrefRegistrySimple;
class TestingPrefServiceSimple;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace {
const char kDate[] = "2023-06-01";
const int64_t kTypicalLowPriceMicros = 1;
const int64_t kTypicalHighPriceMicros = 10;
const int64_t kTypicalPriceMicros = 5;
}  // namespace

namespace commerce {

// A matcher that checks that a
// std::unique_ptr<std::vector<CommerceSubscription>> contains a subscription
// ID that matches the provided string.
MATCHER_P(VectorHasSubscriptionWithId, expected_id, "") {
  for (CommerceSubscription& sub : *arg.get()) {
    if (sub.id == expected_id) {
      return true;
    }
  }
  return false;
}

// A matcher that checks that a CommerceSubscription contains a subscription ID
// that matches the provided string.
MATCHER_P(SubscriptionWithId, expected_id, "") {
  return arg.id == expected_id;
}

// Create a product bookmark with the specified cluster ID and place it in the
// "other" bookmarks folder.
const bookmarks::BookmarkNode* AddProductBookmark(
    bookmarks::BookmarkModel* bookmark_model,
    const std::u16string& title,
    const GURL& url,
    uint64_t cluster_id,
    bool is_price_tracked = false,
    const int64_t price_micros = 0L,
    const std::string& currency_code = "usd",
    const std::optional<int64_t>& last_subscription_change_time = std::nullopt);

// Add product information to an existing bookmark node.
void AddProductInfoToExistingBookmark(
    bookmarks::BookmarkModel* bookmark_model,
    const bookmarks::BookmarkNode* bookmark_node,
    const std::u16string& title,
    uint64_t cluster_id,
    bool is_price_tracked = false,
    const int64_t price_micros = 0L,
    const std::string& currency_code = "usd",
    const std::optional<int64_t>& last_subscription_change_time = std::nullopt);

// Sets the state of the enterprise policy for the shopping list feature for
// testing.
void SetShoppingListEnterprisePolicyPref(TestingPrefServiceSimple* prefs,
                                         bool enabled);

// Register prefs related to commerce in the provided prefs.
void RegisterCommercePrefs(PrefRegistrySimple* registry);

// Set the tab compare enterprise policy for testing.
void SetTabCompareEnterprisePolicyPref(TestingPrefServiceSimple* prefs,
                                       int enabled_state);

std::optional<PriceInsightsInfo> CreateValidPriceInsightsInfo(
    bool has_price_range_data = false,
    bool has_price_history_data = false,
    PriceBucket price_bucket = PriceBucket::kUnknown);

DiscountInfo CreateValidDiscountInfo(
    const std::string& detail,
    const std::string& terms_and_conditions,
    const std::string& value_in_text,
    const std::string& discount_code,
    int64_t id,
    bool is_merchant_wide,
    double expiry_time_sec,
    DiscountClusterType cluster_type = DiscountClusterType::kOfferLevel);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_TEST_UTILS_H_
