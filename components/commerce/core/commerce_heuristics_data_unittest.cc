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
              "cart_url": "foo.com/cart",
              "cart_url_regex" : "foo.com/([^/]+/)?cart"
          },
          "bar.com": {
              "merchant_name": "Bar",
              "checkout_url_regex" : "bar.com/([^/]+/)?checkout"
          },
          "baz.com": {
              "purchase_url_regex" : "baz.com/([^/]+/)?purchase"
          }
      }
  )###";
const char kGlobalHeuristicsJSONData[] = R"###(
      {
        "sensitive_product_regex": "\\b\\B",
        "rule_discount_partner_merchant_regex": "foo",
        "coupon_discount_partner_merchant_regex": "bar",
        "cart_page_url_regex": "cart",
        "checkout_page_url_regex": "checkout",
        "purchase_button_text_regex": "purchase",
        "add_to_cart_request_regex": "add_to_cart"
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

TEST_F(CommerceHeuristicsDataTest, TestVersion) {
  base::Version version1("2022.1.1.1200");
  commerce_heuristics::CommerceHeuristicsData::GetInstance().UpdateVersion(
      version1);
  ASSERT_EQ(
      commerce_heuristics::CommerceHeuristicsData::GetInstance().GetVersion(),
      "2022.1.1.1200");

  base::Version version2("2022.2.1.1300");
  commerce_heuristics::CommerceHeuristicsData::GetInstance().UpdateVersion(
      version2);
  ASSERT_EQ(
      commerce_heuristics::CommerceHeuristicsData::GetInstance().GetVersion(),
      "2022.2.1.1300");
}

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
  ASSERT_EQ(*hint_heuristics->FindDict("foo.com")->FindString("cart_url_regex"),
            "foo.com/([^/]+/)?cart");
  ASSERT_EQ(*hint_heuristics->FindDict("bar.com")->FindString("merchant_name"),
            "Bar");
  ASSERT_EQ(
      *hint_heuristics->FindDict("bar.com")->FindString("checkout_url_regex"),
      "bar.com/([^/]+/)?checkout");
  ASSERT_EQ(
      *hint_heuristics->FindDict("baz.com")->FindString("purchase_url_regex"),
      "baz.com/([^/]+/)?purchase");
  auto* global_heuristics = GetGlobalHeuristics();
  ASSERT_EQ(global_heuristics->size(), 7u);
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
  ASSERT_TRUE(global_heuristics->contains("cart_page_url_regex"));
  ASSERT_EQ(*global_heuristics->FindString("cart_page_url_regex"), "cart");
  ASSERT_TRUE(global_heuristics->contains("checkout_page_url_regex"));
  ASSERT_EQ(*global_heuristics->FindString("checkout_page_url_regex"),
            "checkout");
  ASSERT_TRUE(global_heuristics->contains("purchase_button_text_regex"));
  ASSERT_EQ(*global_heuristics->FindString("purchase_button_text_regex"),
            "purchase");
  ASSERT_TRUE(global_heuristics->contains("add_to_cart_request_regex"));
  ASSERT_EQ(*global_heuristics->FindString("add_to_cart_request_regex"),
            "add_to_cart");
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

TEST_F(CommerceHeuristicsDataTest, TestGetCartPageURLPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetCartPageURLPattern()->pattern(), "cart");
}

TEST_F(CommerceHeuristicsDataTest, TestGetCheckoutPageURLPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetCheckoutPageURLPattern()->pattern(), "checkout");
}

TEST_F(CommerceHeuristicsDataTest, TestGetPurchaseButtonTextPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetPurchaseButtonTextPattern()->pattern(), "purchase");
}

TEST_F(CommerceHeuristicsDataTest, TestGetAddToCartRequestPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetAddToCartRequestPattern()->pattern(), "add_to_cart");
}

TEST_F(CommerceHeuristicsDataTest, TestGetCartPageURLPatternForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetCartPageURLPatternForDomain("foo.com")->pattern(),
            "foo.com/([^/]+/)?cart");
}

TEST_F(CommerceHeuristicsDataTest, TestGetCheckoutPageURLPatternForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetCheckoutPageURLPatternForDomain("bar.com")->pattern(),
            "bar.com/([^/]+/)?checkout");
}

TEST_F(CommerceHeuristicsDataTest, TestGetPurchasePageURLPatternForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetPurchasePageURLPatternForDomain("baz.com")->pattern(),
            "baz.com/([^/]+/)?purchase");
}

TEST_F(CommerceHeuristicsDataTest, TestRepopulateHintData) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(data.GetCartPageURLPatternForDomain("foo.com")->pattern(),
            "foo.com/([^/]+/)?cart");
  ASSERT_EQ(data.GetCheckoutPageURLPatternForDomain("bar.com")->pattern(),
            "bar.com/([^/]+/)?checkout");
  ASSERT_EQ(data.GetPurchasePageURLPatternForDomain("baz.com")->pattern(),
            "baz.com/([^/]+/)?purchase");

  ASSERT_TRUE(data.PopulateDataFromComponent(
      R"###(
      {
          "qux.com": {
              "purchase_url_regex" : "qux.com/([^/]+/)?purchase"
          }
      }
  )###",
      kGlobalHeuristicsJSONData, "", ""));

  ASSERT_FALSE(data.GetCartPageURLPatternForDomain("foo.com"));
  ASSERT_FALSE(data.GetCheckoutPageURLPatternForDomain("bar.com"));
  ASSERT_FALSE(data.GetPurchasePageURLPatternForDomain("baz.com"));
  ASSERT_EQ(data.GetPurchasePageURLPatternForDomain("qux.com")->pattern(),
            "qux.com/([^/]+/)?purchase");
}
}  // namespace commerce_heuristics
