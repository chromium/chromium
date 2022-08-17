// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/shopping_service.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/power_bookmarks/core/power_bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "components/power_bookmarks/core/proto/shopping_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;

namespace commerce {

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

// Create a product bookmark with the specified cluster ID and place it in the
// "other" bookmarks folder.
const bookmarks::BookmarkNode* AddProductBookmark(
    bookmarks::BookmarkModel* bookmark_model,
    const std::u16string& title,
    const GURL& url,
    uint64_t cluster_id,
    bool is_price_tracked) {
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmark_model->other_node(), 0, title, url);
  std::unique_ptr<power_bookmarks::PowerBookmarkMeta> meta =
      std::make_unique<power_bookmarks::PowerBookmarkMeta>();
  meta->set_type(power_bookmarks::PowerBookmarkType::SHOPPING);
  power_bookmarks::ShoppingSpecifics* specifics =
      meta->mutable_shopping_specifics();
  specifics->set_product_cluster_id(cluster_id);
  specifics->set_is_price_tracked(is_price_tracked);

  power_bookmarks::SetNodePowerBookmarkMeta(bookmark_model, node,
                                            std::move(meta));
  return node;
}

}  // namespace

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
  test_features_.InitWithFeatures(
      {commerce::kShoppingList, commerce::kCommerceAllowServerImages}, {});

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
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

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

// Test that the product info cache only keeps track of live tabs.
TEST_F(ShoppingServiceTest, TestProductInfoCacheURLCount) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  std::string url = "http://example.com/foo";
  MockWebWrapper web1(GURL(url), false);
  MockWebWrapper web2(GURL(url), false);

  std::string url2 = "http://example.com/bar";
  MockWebWrapper web3(GURL(url2), false);

  // Ensure navigating to then navigating away clears the cache item.
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigatePrimaryMainFrame(&web1);
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigateAway(&web1, GURL(url));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));

  // Ensure navigating to a URL in multiple "tabs" retains the cached item until
  // both are navigated away.
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigatePrimaryMainFrame(&web1);
  DidNavigatePrimaryMainFrame(&web2);
  ASSERT_EQ(2, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigateAway(&web1, GURL(url));
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url)));
  DidNavigateAway(&web2, GURL(url));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));

  // Make sure more than one entry can be in the cache.
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url2)));
  DidNavigatePrimaryMainFrame(&web1);
  DidNavigatePrimaryMainFrame(&web3);
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url)));
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(url2)));

  // Simulate closing the browser to ensure the cache is emptied.
  WebWrapperDestroyed(&web1);
  WebWrapperDestroyed(&web2);
  WebWrapperDestroyed(&web3);

  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url)));
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(url2)));
}

// Test that product info is inserted into the cache without a client
// necessarily querying for it.
TEST_F(ShoppingServiceTest, TestProductInfoCacheFullLifecycle) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  MockWebWrapper web(GURL(kProductUrl), false);

  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, kImageUrl, kOfferId, kClusterId, kCountryCode);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  DidNavigatePrimaryMainFrame(&web);

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  absl::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kTitle, cached_info->title);
  ASSERT_EQ(kImageUrl, cached_info->image_url);
  ASSERT_EQ(kOfferId, cached_info->offer_id);
  ASSERT_EQ(kClusterId, cached_info->product_cluster_id);
  ASSERT_EQ(kCountryCode, cached_info->country_code);

  // The main API should still work.
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

  // Close the "tab" and make sure the cache is empty.
  WebWrapperDestroyed(&web);
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));
}

// Test that product info is inserted into the cache without a client
// necessarily querying for it.
TEST_F(ShoppingServiceTest, TestProductInfoCacheFullLifecycleWithFallback) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  MockWebWrapper web(GURL(kProductUrl), false);
  std::string json("{\"image\": \"" + std::string(kImageUrl) + "\"}");
  base::Value js_result(json);
  web.SetMockJavaScriptResult(&js_result);

  // Intentionally exclude the image URL to ensure the javascript fallback
  // works.
  OptimizationMetadata meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, "", kOfferId, kClusterId, kCountryCode);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  DidNavigatePrimaryMainFrame(&web);

  // By this point there should be something in the cache.
  ASSERT_EQ(1, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));

  // We should be able to access the cached data.
  absl::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kTitle, cached_info->title);
  ASSERT_EQ("", cached_info->image_url);
  ASSERT_EQ(kOfferId, cached_info->offer_id);
  ASSERT_EQ(kClusterId, cached_info->product_cluster_id);
  ASSERT_EQ(kCountryCode, cached_info->country_code);

  // The main API should still work.
  bool callback_executed = false;
  shopping_service_->GetProductInfoForUrl(
      GURL(kProductUrl), base::BindOnce(
                             [](bool* callback_executed, const GURL& url,
                                const absl::optional<ProductInfo>& info) {
                               ASSERT_EQ(kProductUrl, url.spec());
                               ASSERT_TRUE(info.has_value());

                               ASSERT_EQ(kTitle, info->title);
                               ASSERT_EQ("", info->image_url);
                               ASSERT_EQ(kOfferId, info->offer_id);
                               ASSERT_EQ(kClusterId, info->product_cluster_id);
                               ASSERT_EQ(kCountryCode, info->country_code);
                               *callback_executed = true;
                             },
                             &callback_executed));

  // Make sure the callback was actually run. In testing the callback is run
  // immediately, this check ensures that is actually the case.
  ASSERT_TRUE(callback_executed);

  // Use a RunLoop here since this functionality depends on a JSON sanitizer
  // running on a different thread internally.
  base::RunLoop run_loop;
  DidFinishLoad(&web);
  run_loop.RunUntilIdle();

  // At this point we should have the image in the cache.
  cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl, cached_info->image_url.spec());

  // Close the "tab" and make sure the cache is empty.
  WebWrapperDestroyed(&web);
  ASSERT_EQ(0, GetProductInfoCacheOpenURLCount(GURL(kProductUrl)));
}

// Test that merchant info is processed correctly.
TEST_F(ShoppingServiceTest, TestMerchantInfoResponse) {
  // Ensure a feature that uses merchant info is enabled.
  test_features_.InitAndEnableFeature(kCommerceMerchantViewer);

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

TEST_F(ShoppingServiceTest, TestGetUpdatedProductInfoForBookmarks) {
  const bookmarks::BookmarkNode* product1 = AddProductBookmark(
      bookmark_model_.get(), u"title", GURL(kProductUrl), kClusterId, false);

  OptimizationMetadata updated_meta = opt_guide_->BuildPriceTrackingResponse(
      kTitle, "", kOfferId, kClusterId, kCountryCode);
  opt_guide_->AddOnDemandShoppingResponse(
      GURL(kProductUrl), OptimizationGuideDecision::kTrue, updated_meta);

  std::vector<int64_t> bookmark_ids;
  bookmark_ids.push_back(product1->id());
  int expected_calls = bookmark_ids.size();

  base::RunLoop run_loop;

  auto callback = base::BindRepeating(
      [](bookmarks::BookmarkModel* model, int* call_count,
         base::RunLoop* run_loop, const int64_t id, const GURL& url,
         absl::optional<ProductInfo> info) {
        const bookmarks::BookmarkNode* node =
            bookmarks::GetBookmarkNodeByID(model, id);
        EXPECT_EQ(url.spec(), node->url().spec());

        (*call_count)--;
        if (*call_count <= 0)
          run_loop->Quit();
      },
      bookmark_model_.get(), &expected_calls, &run_loop);

  shopping_service_->GetUpdatedProductInfoForBookmarks(bookmark_ids, callback);
  run_loop.Run();

  EXPECT_EQ(0, expected_calls);
}

TEST_F(ShoppingServiceTest, TestDataMergeWithLeadImage) {
  ProductInfo info;
  info.image_url = GURL(kImageUrl);

  base::Value::Dict data_map;
  data_map.Set("image", "https://example.com/fallback_image.png");

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kImageUrl, info.image_url);
}

TEST_F(ShoppingServiceTest, TestDataMergeWithNoLeadImage) {
  test_features_.InitWithFeatures(
      {kCommerceAllowLocalImages, kCommerceAllowServerImages}, {});
  ProductInfo info;

  base::Value::Dict data_map;
  data_map.Set("image", kImageUrl);

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kImageUrl, info.image_url.spec());
}

TEST_F(ShoppingServiceTest, TestDataMergeWithTitle) {
  ProductInfo info;
  info.title = kTitle;

  base::Value::Dict data_map;
  data_map.Set("title", "Some other fallback title");

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kTitle, info.title);
}

TEST_F(ShoppingServiceTest, TestDataMergeWithNoTitle) {
  ProductInfo info;

  base::Value::Dict data_map;
  data_map.Set("title", kTitle);

  MergeProductInfoData(&info, data_map);

  EXPECT_EQ(kTitle, info.title);
}

}  // namespace commerce
