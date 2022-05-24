// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;

namespace {
const char kProductUrl[] = "http://example.com/";
const char kTitle[] = "product title";
const char kImageUrl[] = "http://example.com/image.png";
const uint64_t kOfferId = 123;
const uint64_t kClusterId = 456;
const char kCountryCode[] = "US";

const char kMerchantUrl[] = "http://example.com/merchant";
const float kStarRating = 4.5;
const uint32_t kCountRating = 1000;
const char kDetailsPageUrl[] = "http://example.com/merchant_details_page";
const bool kHasReturnPolicy = true;
const bool kContainsSensitiveContent = false;
}  // namespace

namespace commerce {

class ShoppingServiceTest : public ShoppingServiceTestBase {
 public:
  ShoppingServiceTest() = default;
  ShoppingServiceTest(const ShoppingServiceTest&) = delete;
  ShoppingServiceTest operator=(const ShoppingServiceTest&) = delete;
  ~ShoppingServiceTest() override = default;
};

// Test that product info is processed correctly.
TEST_F(ShoppingServiceTest, TestProductInfoResponse) {
  // Ensure a feature that uses product info is enabled. This doesn't
  // necessarily need to be the shopping list.
  base::test::ScopedFeatureList test_features;
  test_features.InitAndEnableFeature(commerce::kShoppingList);

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  bool callback_executed = false;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](bool* callback_executed, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_TRUE(info.has_value());

                               ASSERT_EQ(kTitle, info->title);
                               ASSERT_EQ(kImageUrl, info->image_url);
                               ASSERT_EQ(kOfferId, info->offer_id);
                               ASSERT_EQ(kClusterId, info->product_cluster_id);
                               ASSERT_EQ(kCountryCode, info->country_code);
                               *callback_executed = true;
                             },
                             &callback_executed));

  // Make sure the callback was actually run. In testing the callback is run
  // immediately, this check ensures that is actually the case.
  ASSERT_TRUE(callback_executed);
}

// Test that no object is provided for a negative optimization guide response.
TEST_F(ShoppingServiceTest, TestProductInfoResponse_OptGuideFalse) {
  base::test::ScopedFeatureList test_features;
  test_features.InitAndEnableFeature(commerce::kShoppingList);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse,
                          OptimizationMetadata());

  bool callback_executed = false;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](bool* callback_executed, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_FALSE(info.has_value());
                               *callback_executed = true;
                             },
                             &callback_executed));

  ASSERT_TRUE(callback_executed);
}

// Test that merchant info is processed correctly.
TEST_F(ShoppingServiceTest, TestMerchantInfoResponse) {
  // Ensure a feature that uses merchant info is enabled.
  base::test::ScopedFeatureList test_features;
  test_features.InitAndEnableFeature(commerce::kCommerceMerchantViewer);

  OptimizationMetadata meta = opt_guide_->BuildMerchantTrustResponse(
      kStarRating, kCountRating, kDetailsPageUrl, kHasReturnPolicy,
      kContainsSensitiveContent);

  opt_guide_->SetResponse(GURL(kMerchantUrl),
                          OptimizationType::MERCHANT_TRUST_SIGNALS_V2,
                          OptimizationGuideDecision::kTrue, meta);

  bool callback_executed = false;
  shopping_service_->GetMerchantInfoForUrl(
      GURL(kMerchantUrl),
      base::BindOnce(
          [](bool* callback_executed, const GURL& url,
             absl::optional<MerchantInfo> info) {
            ASSERT_EQ(kMerchantUrl, url.spec());
            ASSERT_TRUE(info.has_value());

            ASSERT_EQ(kStarRating, info->star_rating);
            ASSERT_EQ(kCountRating, info->count_rating);
            ASSERT_EQ(kDetailsPageUrl, info->details_page_url.spec());
            ASSERT_EQ(kHasReturnPolicy, info->has_return_policy);
            ASSERT_EQ(kContainsSensitiveContent,
                      info->contains_sensitive_content);
            *callback_executed = true;
          },
          &callback_executed));

  // Make sure the callback was actually run. In testing the callback is run
  // immediately, this check ensures that is actually the case.
  ASSERT_TRUE(callback_executed);
}

}  // namespace commerce
