// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_feature_list.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/buildflag.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
#if !BUILDFLAG(IS_ANDROID)
std::map<std::string, std::string> kRulePartnerMerchantParams = {
    {"partner-merchant-pattern", "foo"},
    {"discount-fetch-delay", "6h"}};
std::map<std::string, std::string> kCouponPartnerMerchantParams = {
    {"coupon-partner-merchant-pattern", "bar"}};
const char kGlobalHeuristicsJSONData[] = R"###(
      {
        "rule_discount_partner_merchant_regex": "baz",
        "coupon_discount_partner_merchant_regex": "qux",
        "no_discount_merchant_regex": "corge",
        "discount_fetch_delay": "10h"
      }
  )###";
const char kRuleFeatureParamPartnerMerchantURL[] = "https://www.foo.com";
const char kCouponFeatureParamPartnerMerchantURL[] = "https://www.bar.com";
const char kRuleComponentPartnerMerchantURL[] = "https://www.baz.com";
const char kCouponComponentPartnerMerchantURL[] = "https://www.qux.com";
const char kNoDiscountMerchantURL[] = "https://www.corge.com";
#endif  //! BUILDFLAG(IS_ANDROID)
}  // namespace

class CommerceFeatureListTest : public testing::Test {
 public:
  void TearDown() override { features_.Reset(); }

 protected:
  base::test::ScopedFeatureList features_;
  base::HistogramTester histogram_tester_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(CommerceFeatureListTest, TestRulePartnerMerchant_FromFeatureParam) {
  features_.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpChromeCartModule, kRulePartnerMerchantParams},
       {commerce::kRetailCoupons, kCouponPartnerMerchantParams}},
      {});

  ASSERT_TRUE(commerce::IsRuleDiscountPartnerMerchant(
      GURL(kRuleFeatureParamPartnerMerchantURL)));
  ASSERT_FALSE(commerce::IsRuleDiscountPartnerMerchant(
      GURL(kCouponFeatureParamPartnerMerchantURL)));
  ASSERT_TRUE(commerce::IsCouponDiscountPartnerMerchant(
      GURL(kCouponFeatureParamPartnerMerchantURL)));
  ASSERT_FALSE(commerce::IsCouponDiscountPartnerMerchant(
      GURL(kRuleFeatureParamPartnerMerchantURL)));
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.PartnerMerchantPatternSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
          FROM_FEATURE_PARAMETER,
      2);
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.PartnerMerchantPatternSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT, 0);
}

TEST_F(CommerceFeatureListTest, TestRulePartnerMerchant_FromComponent) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
  ASSERT_TRUE(
      data.PopulateDataFromComponent("{}", kGlobalHeuristicsJSONData, "", ""));

  ASSERT_TRUE(commerce::IsRuleDiscountPartnerMerchant(
      GURL(kRuleComponentPartnerMerchantURL)));
  ASSERT_FALSE(commerce::IsRuleDiscountPartnerMerchant(
      GURL(kCouponComponentPartnerMerchantURL)));
  ASSERT_TRUE(commerce::IsCouponDiscountPartnerMerchant(
      GURL(kCouponComponentPartnerMerchantURL)));
  ASSERT_FALSE(commerce::IsCouponDiscountPartnerMerchant(
      GURL(kRuleComponentPartnerMerchantURL)));
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.PartnerMerchantPatternSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
          FROM_FEATURE_PARAMETER,
      0);
  histogram_tester_.ExpectBucketCount(
      "Commerce.Heuristics.PartnerMerchantPatternSource",
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT, 2);
}

TEST_F(CommerceFeatureListTest, TestCouponPartnerMerchant_Priority) {
  features_.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpChromeCartModule, kRulePartnerMerchantParams},
       {commerce::kRetailCoupons, kCouponPartnerMerchantParams}},
      {});

  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
  ASSERT_TRUE(
      data.PopulateDataFromComponent("{}", kGlobalHeuristicsJSONData, "", ""));

  ASSERT_TRUE(commerce::IsRuleDiscountPartnerMerchant(
      GURL(kRuleFeatureParamPartnerMerchantURL)));
  ASSERT_FALSE(commerce::IsRuleDiscountPartnerMerchant(
      GURL(kRuleComponentPartnerMerchantURL)));
  ASSERT_TRUE(commerce::IsCouponDiscountPartnerMerchant(
      GURL(kCouponFeatureParamPartnerMerchantURL)));
  ASSERT_FALSE(commerce::IsCouponDiscountPartnerMerchant(
      GURL(kCouponComponentPartnerMerchantURL)));
}

TEST_F(CommerceFeatureListTest, TestGetDiscountFetchDelay_FromFeatureParam) {
  commerce_heuristics::CommerceHeuristicsData::GetInstance()
      .PopulateDataFromComponent("{}", "{}", "", "");
  features_.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpChromeCartModule, kRulePartnerMerchantParams},
       {commerce::kRetailCoupons, kCouponPartnerMerchantParams}},
      {});

  ASSERT_EQ(commerce::GetDiscountFetchDelay(), base::Hours(6));
}

TEST_F(CommerceFeatureListTest, TestGetDiscountFetchDelay_FromComponent) {
  features_.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpChromeCartModule, kRulePartnerMerchantParams},
       {commerce::kRetailCoupons, kCouponPartnerMerchantParams}},
      {});

  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
  ASSERT_TRUE(
      data.PopulateDataFromComponent("{}", kGlobalHeuristicsJSONData, "", ""));

  ASSERT_EQ(commerce::GetDiscountFetchDelay(), base::Hours(10));
}

TEST_F(CommerceFeatureListTest, TestNoDiscountMerchant) {
  auto& data = commerce_heuristics::CommerceHeuristicsData::GetInstance();
  ASSERT_TRUE(
      data.PopulateDataFromComponent("{}", kGlobalHeuristicsJSONData, "", ""));

  ASSERT_TRUE(commerce::IsNoDiscountMerchant(GURL(kNoDiscountMerchantURL)));
  ASSERT_FALSE(
      commerce::IsNoDiscountMerchant(GURL(kCouponComponentPartnerMerchantURL)));
  // Matching host only.
  ASSERT_FALSE(
      commerce::IsNoDiscountMerchant(GURL("https://www.qux.com/corge")));
}
#endif  //! BUILDFLAG(IS_ANDROID)

// This test assumes that, at bare minimum, "US" is an allowed country and
// "en-us" is an allowed locale for the US.
TEST_F(CommerceFeatureListTest, TestEnabledForCountryAndLocale) {
  // Check the known success cases with different character cases.
  ASSERT_TRUE(commerce::IsEnabledForCountryAndLocale(
      commerce::kShoppingPDPMetricsRegionLaunched, "US", "en-us"));
  ASSERT_TRUE(commerce::IsEnabledForCountryAndLocale(
      commerce::kShoppingPDPMetricsRegionLaunched, "us", "en-US"));

  // Test allowed country with disallowed (fake) locale.
  ASSERT_FALSE(commerce::IsEnabledForCountryAndLocale(
      commerce::kShoppingPDPMetricsRegionLaunched, "us", "zz-zz"));

  // Test allowed locale in a disallowed (fake) country.
  ASSERT_FALSE(commerce::IsEnabledForCountryAndLocale(
      commerce::kShoppingPDPMetricsRegionLaunched, "zz", "en-us"));

  // Ensure empty values don't crash.
  ASSERT_FALSE(commerce::IsEnabledForCountryAndLocale(
      commerce::kShoppingPDPMetricsRegionLaunched, "", ""));
}

TEST_F(CommerceFeatureListTest, IsShoppingListEnabled) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(commerce::kShoppingListEnabledPrefName,
                                        true);

  EXPECT_TRUE(commerce::IsShoppingListAllowedForEnterprise(&prefs));

  prefs.SetUserPref(commerce::kShoppingListEnabledPrefName, base::Value(false));
  EXPECT_TRUE(commerce::IsShoppingListAllowedForEnterprise(&prefs));

  prefs.SetManagedPref(commerce::kShoppingListEnabledPrefName,
                       base::Value(true));
  EXPECT_TRUE(commerce::IsShoppingListAllowedForEnterprise(&prefs));

  prefs.SetManagedPref(commerce::kShoppingListEnabledPrefName,
                       base::Value(false));
  EXPECT_FALSE(commerce::IsShoppingListAllowedForEnterprise(&prefs));
}
