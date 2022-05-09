// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/proto/price_tracking.pb.h"
#include "components/commerce/core/shopping_service.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/common_types.pb.h"
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
}  // namespace

namespace commerce {

// A mock Optimization Guide decider that allows us to specify the response for
// a particular URL.
class MockOptGuideDecider
    : public optimization_guide::NewOptimizationGuideDecider {
 public:
  MockOptGuideDecider() = default;
  MockOptGuideDecider(const MockOptGuideDecider&) = delete;
  MockOptGuideDecider operator=(const MockOptGuideDecider&) = delete;
  ~MockOptGuideDecider() override = default;

  void RegisterOptimizationTypes(
      const std::vector<OptimizationType>& optimization_types) override {}

  void CanApplyOptimization(
      const GURL& url,
      OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) override {
    bool type_matches = optimization_type_.has_value() &&
                        optimization_type_.value() == optimization_type;
    bool url_matches =
        response_url_.has_value() && url == response_url_.value();

    if (!type_matches || !url_matches) {
      std::move(callback).Run(OptimizationGuideDecision::kUnknown,
                              OptimizationMetadata());
      return;
    }

    std::move(callback).Run(optimization_decision_.value(),
                            optimization_data_.value());
  }

  OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) override {
    // We don't use the synchronous API in the shopping service.
    NOTREACHED();

    return OptimizationGuideDecision::kUnknown;
  }

  void SetResponse(const GURL& url,
                   const OptimizationType type,
                   const OptimizationGuideDecision decision,
                   const OptimizationMetadata& data) {
    response_url_ = url;
    optimization_type_ = type;
    optimization_decision_ = decision;
    optimization_data_ = data;
  }

  OptimizationMetadata BuildPriceTrackingResponse(
      const std::string& title,
      const std::string& image_url,
      const uint64_t offer_id,
      const uint64_t product_cluster_id,
      const std::string& country_code) {
    OptimizationMetadata meta;

    PriceTrackingData price_tracking_data;
    BuyableProduct* buyable_product =
        price_tracking_data.mutable_buyable_product();
    buyable_product->set_title(title);
    buyable_product->set_image_url(image_url);
    buyable_product->set_offer_id(offer_id);
    buyable_product->set_product_cluster_id(product_cluster_id);
    buyable_product->set_country_code(country_code);

    Any any;
    any.set_type_url(price_tracking_data.GetTypeName());
    price_tracking_data.SerializeToString(any.mutable_value());
    meta.set_any_metadata(any);

    return meta;
  }

 private:
  absl::optional<GURL> response_url_;
  absl::optional<OptimizationType> optimization_type_;
  absl::optional<OptimizationGuideDecision> optimization_decision_;
  absl::optional<OptimizationMetadata> optimization_data_;
};

class ShoppingServiceTest : public testing::Test {
 public:
  ShoppingServiceTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        opt_guide_(std::make_unique<MockOptGuideDecider>()) {
    shopping_service_ = std::make_unique<ShoppingService>(bookmark_model_.get(),
                                                          opt_guide_.get());
  }

  ShoppingServiceTest(const ShoppingServiceTest&) = delete;
  ShoppingServiceTest operator=(const ShoppingServiceTest&) = delete;
  ~ShoppingServiceTest() override = default;

  void TestBody() override {}

 protected:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;

  std::unique_ptr<MockOptGuideDecider> opt_guide_;

  std::unique_ptr<ShoppingService> shopping_service_;
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

}  // namespace commerce
