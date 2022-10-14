// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_heuristics_data.h"
#include "base/strings/string_util.h"
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
              "purchase_url_regex" : "baz.com/([^/]+/)?purchase",
              "skip_add_to_cart_regex": "dummy-request-url"
          }
      }
  )###";
const char kGlobalHeuristicsJSONData[] = R"###(
      {
        "sensitive_product_regex": "\\b\\B",
        "rule_discount_partner_merchant_regex": "foo",
        "coupon_discount_partner_merchant_regex": "bar",
        "no_discount_merchant_regex": "baz",
        "cart_page_url_regex": "cart",
        "checkout_page_url_regex": "checkout",
        "purchase_button_text_regex": "purchase",
        "add_to_cart_request_regex": "add_to_cart",
        "discount_fetch_delay": "10h"
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
  EXPECT_EQ(
      commerce_heuristics::CommerceHeuristicsData::GetInstance().GetVersion(),
      "2022.1.1.1200");

  base::Version version2("2022.2.1.1300");
  commerce_heuristics::CommerceHeuristicsData::GetInstance().UpdateVersion(
      version2);
  EXPECT_EQ(
      commerce_heuristics::CommerceHeuristicsData::GetInstance().GetVersion(),
      "2022.2.1.1300");
}

TEST_F(CommerceHeuristicsDataTest, TestPopulateHintHeuristics_Success) {
  ASSERT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .PopulateDataFromComponent(kHintHeuristicsJSONData,
                                             kGlobalHeuristicsJSONData, "",
                                             ""));
  auto* hint_heuristics = GetHintHeuristics();
  EXPECT_EQ(hint_heuristics->size(), 3u);
  EXPECT_TRUE(hint_heuristics->contains("foo.com"));
  EXPECT_TRUE(hint_heuristics->contains("bar.com"));
  EXPECT_TRUE(hint_heuristics->contains("baz.com"));
  EXPECT_EQ(*hint_heuristics->FindDict("foo.com")->FindString("merchant_name"),
            "Foo");
  EXPECT_EQ(*hint_heuristics->FindDict("foo.com")->FindString("cart_url"),
            "foo.com/cart");
  EXPECT_EQ(*hint_heuristics->FindDict("foo.com")->FindString("cart_url_regex"),
            "foo.com/([^/]+/)?cart");
  EXPECT_EQ(*hint_heuristics->FindDict("bar.com")->FindString("merchant_name"),
            "Bar");
  EXPECT_EQ(
      *hint_heuristics->FindDict("bar.com")->FindString("checkout_url_regex"),
      "bar.com/([^/]+/)?checkout");
  EXPECT_EQ(
      *hint_heuristics->FindDict("baz.com")->FindString("purchase_url_regex"),
      "baz.com/([^/]+/)?purchase");
  EXPECT_EQ(*hint_heuristics->FindDict("baz.com")->FindString(
                "skip_add_to_cart_regex"),
            "dummy-request-url");
  auto* global_heuristics = GetGlobalHeuristics();
  EXPECT_EQ(global_heuristics->size(), 9u);
  EXPECT_TRUE(global_heuristics->contains("sensitive_product_regex"));
  EXPECT_EQ(*global_heuristics->FindString("sensitive_product_regex"),
            "\\b\\B");
  EXPECT_TRUE(
      global_heuristics->contains("rule_discount_partner_merchant_regex"));
  EXPECT_EQ(
      *global_heuristics->FindString("rule_discount_partner_merchant_regex"),
      "foo");
  EXPECT_TRUE(
      global_heuristics->contains("coupon_discount_partner_merchant_regex"));
  EXPECT_EQ(
      *global_heuristics->FindString("coupon_discount_partner_merchant_regex"),
      "bar");
  EXPECT_TRUE(global_heuristics->contains("no_discount_merchant_regex"));
  EXPECT_EQ(*global_heuristics->FindString("no_discount_merchant_regex"),
            "baz");
  EXPECT_TRUE(global_heuristics->contains("cart_page_url_regex"));
  EXPECT_EQ(*global_heuristics->FindString("cart_page_url_regex"), "cart");
  EXPECT_TRUE(global_heuristics->contains("checkout_page_url_regex"));
  EXPECT_EQ(*global_heuristics->FindString("checkout_page_url_regex"),
            "checkout");
  EXPECT_TRUE(global_heuristics->contains("purchase_button_text_regex"));
  EXPECT_EQ(*global_heuristics->FindString("purchase_button_text_regex"),
            "purchase");
  EXPECT_TRUE(global_heuristics->contains("add_to_cart_request_regex"));
  EXPECT_EQ(*global_heuristics->FindString("add_to_cart_request_regex"),
            "add_to_cart");
  EXPECT_EQ(*global_heuristics->FindString("discount_fetch_delay"), "10h");
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

  EXPECT_FALSE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                   .PopulateDataFromComponent(broken_hint_json_string,
                                              kGlobalHeuristicsJSONData, "",
                                              ""));
  EXPECT_FALSE(
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .PopulateDataFromComponent(kHintHeuristicsJSONData, "{", "", ""));
}

TEST_F(CommerceHeuristicsDataTest, TestGetMerchantName) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(*data.GetMerchantName("foo.com"), "Foo");
  EXPECT_EQ(*data.GetMerchantName("bar.com"), "Bar");
  EXPECT_FALSE(data.GetMerchantName("baz.com").has_value());
  EXPECT_FALSE(data.GetMerchantName("xyz.com").has_value());
}

TEST_F(CommerceHeuristicsDataTest, TestGetMerchantCartURL) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(*data.GetMerchantCartURL("foo.com"), "foo.com/cart");
  EXPECT_FALSE(data.GetMerchantCartURL("baz.com").has_value());
}

TEST_F(CommerceHeuristicsDataTest, TestGetProductSkipPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetProductSkipPattern()->pattern(), "\\b\\B");
}

TEST_F(CommerceHeuristicsDataTest, TestGetRuleDiscountPartnerMerchantPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetRuleDiscountPartnerMerchantPattern()->pattern(), "foo");
}

TEST_F(CommerceHeuristicsDataTest,
       TestGetCouponDiscountPartnerMerchantPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetCouponDiscountPartnerMerchantPattern()->pattern(), "bar");
}

TEST_F(CommerceHeuristicsDataTest, TestGetNoDiscountMerchantPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetNoDiscountMerchantPattern()->pattern(), "baz");
}

TEST_F(CommerceHeuristicsDataTest, TestGetCartPageURLPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetCartPageURLPattern()->pattern(), "cart");
}

TEST_F(CommerceHeuristicsDataTest, TestGetCheckoutPageURLPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetCheckoutPageURLPattern()->pattern(), "checkout");
}

TEST_F(CommerceHeuristicsDataTest, TestGetPurchaseButtonTextPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetPurchaseButtonTextPattern()->pattern(), "purchase");
}

TEST_F(CommerceHeuristicsDataTest, TestGetAddToCartRequestPattern) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetAddToCartRequestPattern()->pattern(), "add_to_cart");
}

TEST_F(CommerceHeuristicsDataTest, TestGetCartPageURLPatternForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetCartPageURLPatternForDomain("foo.com")->pattern(),
            "foo.com/([^/]+/)?cart");
}

TEST_F(CommerceHeuristicsDataTest, TestGetCheckoutPageURLPatternForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetCheckoutPageURLPatternForDomain("bar.com")->pattern(),
            "bar.com/([^/]+/)?checkout");
}

TEST_F(CommerceHeuristicsDataTest, TestGetPurchasePageURLPatternForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetPurchasePageURLPatternForDomain("baz.com")->pattern(),
            "baz.com/([^/]+/)?purchase");
}

TEST_F(CommerceHeuristicsDataTest, TestGetSkipAddToCartPatternForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetSkipAddToCartPatternForDomain("baz.com")->pattern(),
            "dummy-request-url");
}

TEST_F(CommerceHeuristicsDataTest, TestRepopulateHintData) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  EXPECT_EQ(data.GetCartPageURLPatternForDomain("foo.com")->pattern(),
            "foo.com/([^/]+/)?cart");
  EXPECT_EQ(data.GetCheckoutPageURLPatternForDomain("bar.com")->pattern(),
            "bar.com/([^/]+/)?checkout");
  EXPECT_EQ(data.GetPurchasePageURLPatternForDomain("baz.com")->pattern(),
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

  EXPECT_FALSE(data.GetCartPageURLPatternForDomain("foo.com"));
  EXPECT_FALSE(data.GetCheckoutPageURLPatternForDomain("bar.com"));
  EXPECT_FALSE(data.GetPurchasePageURLPatternForDomain("baz.com"));
  EXPECT_EQ(data.GetPurchasePageURLPatternForDomain("qux.com")->pattern(),
            "qux.com/([^/]+/)?purchase");
}

TEST_F(CommerceHeuristicsDataTest, TestGetHintHeuristicsJSONForDomain) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
  std::string hint_heuristics_string = R"###(
      {
          "foo.com":{
              "merchant_name":"Foo"
          },
          "bar.com":{}
      }
  )###";
  std::string foo_expected = R"###(
      {
          "foo.com":{
              "merchant_name":"Foo"
          }
      }
  )###";
  foo_expected = base::CollapseWhitespaceASCII(foo_expected, true);

  ASSERT_TRUE(
      data.PopulateDataFromComponent(hint_heuristics_string, "{}", "", ""));

  auto foo_heuristics =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetHintHeuristicsJSONForDomain("foo.com");
  EXPECT_TRUE(foo_heuristics.has_value());
  EXPECT_EQ(*foo_heuristics, foo_expected);
  auto bar_heuristics =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetHintHeuristicsJSONForDomain("bar.com");
  EXPECT_FALSE(bar_heuristics.has_value());
}

TEST_F(CommerceHeuristicsDataTest, TestGetGlobalHeuristicsJSON) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
  std::string global_heuristics_string = "{\"cart_pattern\" : \"foo\"}";

  ASSERT_TRUE(
      data.PopulateDataFromComponent("{}", global_heuristics_string, "", ""));

  auto global_heuristics_optional =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetGlobalHeuristicsJSON();
  EXPECT_TRUE(global_heuristics_optional.has_value());
  EXPECT_EQ(*global_heuristics_optional, global_heuristics_string);
}

TEST_F(CommerceHeuristicsDataTest, TestGetProductIDExtractionJSON) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent("{}", "{}", "", ""));
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .GetProductIDExtractionJSON()
                  .empty());

  ASSERT_TRUE(data.PopulateDataFromComponent("{}", "{}", "foo", "bar"));
  EXPECT_EQ("foo", commerce_heuristics::CommerceHeuristicsData::GetInstance()
                       .GetProductIDExtractionJSON());
}

TEST_F(CommerceHeuristicsDataTest, TestGetCartProductExtractionScript) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent("{}", "{}", "", ""));
  EXPECT_TRUE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                  .GetCartProductExtractionScript()
                  .empty());

  ASSERT_TRUE(data.PopulateDataFromComponent("{}", "{}", "foo", "bar"));
  EXPECT_EQ("bar", commerce_heuristics::CommerceHeuristicsData::GetInstance()
                       .GetCartProductExtractionScript());
}

TEST_F(CommerceHeuristicsDataTest, TestGetDiscountFetchDelay) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();

  ASSERT_TRUE(data.PopulateDataFromComponent("{}", "{}", "", ""));
  EXPECT_FALSE(commerce_heuristics::CommerceHeuristicsData::GetInstance()
                   .GetDiscountFetchDelay()
                   .has_value());

  ASSERT_TRUE(data.PopulateDataFromComponent(
      kHintHeuristicsJSONData, kGlobalHeuristicsJSONData, "", ""));

  auto delay_value_optional =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetDiscountFetchDelay();
  EXPECT_TRUE(delay_value_optional.has_value());
  EXPECT_EQ(*delay_value_optional, base::Hours(10));
}
}  // namespace commerce_heuristics
