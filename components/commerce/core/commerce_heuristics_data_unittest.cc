// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_heuristics_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commerce_heuristics {

namespace {
const char kHintHeuristicsJSONData[] = R"###(
      {
          "foo.com": {
              "merchant_name": "Foo",
              "cart_url": "foo.com/cart"
          },
          "bar.com": {
              "merchant_name": "Bar"
          },
          "baz.com": {}
      }
  )###";
const char kGlobalHeuristicsJSONData[] = R"###(
      {
        "sensitive_product_regex": "\\b\\B",
        "rule_discount_partner_merchant_regex": "foo",
        "coupon_discount_partner_merchant_regex": "bar"
      }
  )###";
}  // namespace

class CommerceHeuristicsDataTest : public testing::Test {
 public:
  CommerceHeuristicsDataTest() = default;

  base::Value::Dict* GetHintHeuristics() {
    return &commerce_heuristics::CommerceHeuristicsData::GetInstance()
                .hint_heuristics_;
  }

  base::Value::Dict* GetGlobalHeuristics() {
    return &commerce_heuristics::CommerceHeuristicsData::GetInstance()
                .global_heuristics_;
  }
};

TEST_F(CommerceHeuristicsDataTest, TestPopulateHintHeuristics_Success) {
  ASSERT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent(kHintHeuristicsJSONData,
                                             kGlobalHeuristicsJSONData, "",
                                             ""));
  auto* hint_heuristics = GetHintHeuristics();
  ASSERT_EQ(hint_heuristics->size(), 3u);
  ASSERT_TRUE(hint_heuristics->contains("foo.com"));
  ASSERT_TRUE(hint_heuristics->contains("bar.com"));
  ASSERT_TRUE(hint_heuristics->contains("baz.com"));
  ASSERT_EQ(*hint_heuristics->FindDict("foo.com")->FindString("merchant_name"),
            "Foo");
  ASSERT_EQ(*hint_heuristics->FindDict("foo.com")->FindString("cart_url"),
            "foo.com/cart");
  ASSERT_EQ(*hint_heuristics->FindDict("bar.com")->FindString("merchant_name"),
            "Bar");
  auto* global_heuristics = GetGlobalHeuristics();
  ASSERT_EQ(global_heuristics->size(), 3u);
  ASSERT_TRUE(global_heuristics->contains("sensitive_product_regex"));
  ASSERT_EQ(*global_heuristics->FindString("sensitive_product_regex"),
            "\\b\\B");
  ASSERT_TRUE(
      global_heuristics->contains("rule_discount_partner_merchant_regex"));
  ASSERT_EQ(
      *global_heuristics->FindString("rule_discount_partner_merchant_regex"),
      "foo");
  ASSERT_TRUE(
      global_heuristics->contains("coupon_discount_partner_merchant_regex"));
  ASSERT_EQ(
      *global_heuristics->FindString("coupon_discount_partner_merchant_regex"),
      "bar");
}

TEST_F(CommerceHeuristicsDataTest, TestPopulateHeuristics_Failure) {
  const char* broken_hint_json_string = R"###(
      {
          "foo.com": {
              "merchant_name": "Foo"
          },
          "bar.com": {
              "merchant_name": "Bar"
  )###";

  ASSERT_FALSE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                   .PopulateDataFromComponent(broken_hint_json_string,
                                              kGlobalHeuristicsJSONData, "",
                                              ""));
  ASSERT_FALSE(
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .PopulateDataFromComponent(kHintHeuristicsJSONData, "{", "", ""));
}

TEST_F(CommerceHeuristicsDataTest, TestGetMerchantName) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(*data.GetMerchantName("foo.com"), "Foo");
  ASSERT_EQ(*data.GetMerchantName("bar.com"), "Bar");
  ASSERT_FALSE(data.GetMerchantName("baz.com").has_value());
  ASSERT_FALSE(data.GetMerchantName("xyz.com").has_value());
}

TEST_F(CommerceHeuristicsDataTest, TestGetMerchantCartURL) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(*data.GetMerchantCartURL("foo.com"), "foo.com/cart");
  ASSERT_FALSE(data.GetMerchantCartURL("baz.com").has_value());
}

TEST_F(CommerceHeuristicsDataTest, TestGetProductSkipPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetProductSkipPattern()->pattern(), "\\b\\B");
}

TEST_F(CommerceHeuristicsDataTest, TestGetRuleDiscountPartnerMerchantPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetRuleDiscountPartnerMerchantPattern()->pattern(), "foo");
}

TEST_F(CommerceHeuristicsDataTest,
       TestGetCouponDiscountPartnerMerchantPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetCouponDiscountPartnerMerchantPattern()->pattern(), "bar");
}
}  // namespace commerce_heuristics
