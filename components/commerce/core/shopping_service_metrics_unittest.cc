// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;
using UkmEntry = ukm::builders::Shopping_PDPStateWithLocalInfo;

namespace {
const char kProductUrl[] = "http://example.com/";
const char kImageUrl[] = "http://example.com/image.png";
const char kImageUrl2[] = "http://example.com/second_image.png";
}  // namespace

namespace commerce {

class ShoppingServiceMetricsTest : public ShoppingServiceTestBase {
 public:
  ShoppingServiceMetricsTest() {
    og_response_with_image_ =
        opt_guide_->BuildPriceTrackingResponse("", kImageUrl, 0, 0, "us");

    og_response_no_image_ =
        opt_guide_->BuildPriceTrackingResponse("", "", 0, 0, "us");
  }
  ShoppingServiceMetricsTest(const ShoppingServiceMetricsTest&) = delete;
  ShoppingServiceMetricsTest operator=(const ShoppingServiceMetricsTest&) =
      delete;
  ~ShoppingServiceMetricsTest() override = default;

  void SetUp() override {
    ShoppingServiceTestBase::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void CheckUkmEvent(metrics::ShoppingPDPDetectionMethod method) {
    const auto& entries = ukm_recorder_->GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    ukm_recorder_->ExpectEntryMetric(entries[0], UkmEntry::kPDPStateName,
                                     static_cast<int64_t>(method));
  }

  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  OptimizationMetadata og_response_with_image_;
  OptimizationMetadata og_response_no_image_;
};

TEST_F(ShoppingServiceMetricsTest,
       TestImageAvailabilityServerDisabledLocalEnabled) {
  test_features_.InitWithFeatures({kShoppingList, kCommerceAllowLocalImages},
                                  {kCommerceAllowServerImages});

  auto result = base::Value::Dict();
  result.Set("image", std::string(kImageUrl2));
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_with_image_);

  DidNavigatePrimaryMainFrame(&web);

  // We should be able to access the cached data.
  std::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_NE(kImageUrl, cached_info->image_url);

  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  // After the page has loaded and the on-page js has run, we should have the
  // on-page image.
  cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl2, cached_info->image_url.spec());

  histogram_tester_->ExpectBucketCount(kImageAvailabilityHistogramName,
                                       ProductImageAvailability::kBothAvailable,
                                       1);
  histogram_tester_->ExpectTotalCount(kImageAvailabilityHistogramName, 1);
}

TEST_F(ShoppingServiceMetricsTest,
       TestImageAvailabilityServerEnabledLocalDisabled) {
  test_features_.InitWithFeatures({kShoppingList, kCommerceAllowServerImages},
                                  {kCommerceAllowLocalImages});

  auto result = base::Value::Dict();
  result.Set("image", std::string(kImageUrl2));
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_with_image_);

  DidNavigatePrimaryMainFrame(&web);

  // We should be able to access the cached data.
  std::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl, cached_info->image_url.spec());

  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  // After the page has loaded and the on-page js has run, we should have the
  // on-page image.
  cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl, cached_info->image_url.spec());

  histogram_tester_->ExpectBucketCount(kImageAvailabilityHistogramName,
                                       ProductImageAvailability::kBothAvailable,
                                       1);
  histogram_tester_->ExpectTotalCount(kImageAvailabilityHistogramName, 1);
}

TEST_F(ShoppingServiceMetricsTest, TestImageAvailabilityNoServerImage) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  auto result = base::Value::Dict();
  result.Set("image", std::string(kImageUrl2));
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_no_image_);

  DidNavigatePrimaryMainFrame(&web);

  // We should be able to access the cached data.
  std::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_NE(kImageUrl, cached_info->image_url);

  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  // After the page has loaded and the on-page js has run, we should have the
  // on-page image.
  cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl2, cached_info->image_url.spec());

  histogram_tester_->ExpectBucketCount(kImageAvailabilityHistogramName,
                                       ProductImageAvailability::kLocalOnly, 1);
  histogram_tester_->ExpectTotalCount(kImageAvailabilityHistogramName, 1);
}

TEST_F(ShoppingServiceMetricsTest, TestImageAvailabilityNoLocalImage) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  auto result = base::Value::Dict();
  result.Set("irrelevant", std::string("value"));
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_with_image_);

  DidNavigatePrimaryMainFrame(&web);

  // We should be able to access the cached data.
  std::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl, cached_info->image_url);

  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  // After the page has loaded and the on-page js has run, we should not have
  // detected another image and report "server only".
  histogram_tester_->ExpectBucketCount(kImageAvailabilityHistogramName,
                                       ProductImageAvailability::kServerOnly,
                                       1);
  histogram_tester_->ExpectTotalCount(kImageAvailabilityHistogramName, 1);
}

// In order for the metric to be recorded, the on-page javascripts needs to have
// run, otherwise we don't record as accurate of data. This test ensures that
// the metric isn't recorded if the javascript hasn't run.
TEST_F(ShoppingServiceMetricsTest, TestImageAvailabilityNoRecordIfJSNotRun) {
  test_features_.InitWithFeatures(
      {kShoppingList, kCommerceAllowLocalImages, kCommerceAllowServerImages},
      {});

  MockWebWrapper web(GURL(kProductUrl), false);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_with_image_);

  DidNavigatePrimaryMainFrame(&web);

  // We should be able to access the cached data.
  std::optional<ProductInfo> cached_info =
      shopping_service_->GetAvailableProductInfoForUrl(GURL(kProductUrl));
  ASSERT_EQ(kImageUrl, cached_info->image_url.spec());

  histogram_tester_->ExpectTotalCount(kImageAvailabilityHistogramName, 0);
}

TEST_F(ShoppingServiceMetricsTest, TestLocalPDPDetection_NoLocal) {
  test_features_.InitWithFeatures({kShoppingList, kCommerceLocalPDPDetection},
                                  {});

  //  Set the type as a non-product.
  auto result = base::Value::Dict();
  result.Set(std::string(kOgType), "article");
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_with_image_);

  DidNavigatePrimaryMainFrame(&web);
  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  histogram_tester_->ExpectBucketCount(
      metrics::kPDPStateWithLocalMetaName,
      metrics::ShoppingPDPDetectionMethod::kPDPServerOnly, 1);
  histogram_tester_->ExpectTotalCount(metrics::kPDPStateWithLocalMetaName, 1);

  CheckUkmEvent(metrics::ShoppingPDPDetectionMethod::kPDPServerOnly);
}

TEST_F(ShoppingServiceMetricsTest, TestLocalPDPDetection_BothServerAndLocal) {
  test_features_.InitWithFeatures({kShoppingList, kCommerceLocalPDPDetection},
                                  {});

  auto result = base::Value::Dict();
  result.Set(std::string(commerce::kOgType), commerce::kOgTypeOgProduct);
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_with_image_);

  DidNavigatePrimaryMainFrame(&web);
  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  histogram_tester_->ExpectBucketCount(
      metrics::kPDPStateWithLocalMetaName,
      metrics::ShoppingPDPDetectionMethod::kPDPServerAndLocalMeta, 1);
  histogram_tester_->ExpectTotalCount(metrics::kPDPStateWithLocalMetaName, 1);

  CheckUkmEvent(metrics::ShoppingPDPDetectionMethod::kPDPServerAndLocalMeta);
}

TEST_F(ShoppingServiceMetricsTest, TestLocalPDPDetection_NoServer) {
  test_features_.InitWithFeatures({kShoppingList, kCommerceLocalPDPDetection},
                                  {});

  auto result = base::Value::Dict();
  result.Set(commerce::kOgType, commerce::kOgTypeOgProduct);
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse,
                          OptimizationMetadata());

  DidNavigatePrimaryMainFrame(&web);
  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  histogram_tester_->ExpectBucketCount(
      metrics::kPDPStateWithLocalMetaName,
      metrics::ShoppingPDPDetectionMethod::kPDPLocalMetaOnly, 1);
  histogram_tester_->ExpectTotalCount(metrics::kPDPStateWithLocalMetaName, 1);

  CheckUkmEvent(metrics::ShoppingPDPDetectionMethod::kPDPLocalMetaOnly);
}

TEST_F(ShoppingServiceMetricsTest, TestLocalPDPDetection_IllegalScheme) {
  test_features_.InitWithFeatures({kShoppingList, kCommerceLocalPDPDetection},
                                  {});

  auto result = base::Value::Dict();
  result.Set(kOgType, kOgTypeOgProduct);
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL("chrome://internal-page"), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse,
                          OptimizationMetadata());

  DidNavigatePrimaryMainFrame(&web);
  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  histogram_tester_->ExpectTotalCount(metrics::kPDPStateWithLocalMetaName, 0);
}

TEST_F(ShoppingServiceMetricsTest,
       TestLocalPDPDetection_NoServer_NoExperimentFlag) {
  test_features_.InitWithFeatures({kShoppingList},
                                  {kCommerceLocalPDPDetection});

  auto result = base::Value::Dict();
  result.Set(commerce::kOgType, commerce::kOgTypeOgProduct);
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse,
                          OptimizationMetadata());

  DidNavigatePrimaryMainFrame(&web);
  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  histogram_tester_->ExpectTotalCount(metrics::kPDPStateWithLocalMetaName, 0);
}

TEST_F(ShoppingServiceMetricsTest, TestProductInfoJsExecutionTime) {
  test_features_.InitWithFeatures({kShoppingList}, {});

  // The content of the javascript result only needs to be json for this text,
  // the actual fields don't matter.
  auto result = base::Value::Dict();
  result.Set("success", "true");
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue,
                          og_response_no_image_);

  DidNavigatePrimaryMainFrame(&web);
  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  histogram_tester_->ExpectTotalCount(kProductInfoLocalExtractionTime, 1);
}

TEST_F(ShoppingServiceMetricsTest, TestNoExtractionForNonShoppingPage) {
  test_features_.InitWithFeatures({kCommerceLocalPDPDetection}, {});
  opt_guide_->SetDefaultShoppingPage(false);

  auto result = base::Value::Dict();
  result.Set(kOgType, kOgTypeOgProduct);
  base::Value js_result(std::move(result));
  MockWebWrapper web(GURL(kProductUrl), false, &js_result);

  opt_guide_->SetResponse(GURL(kProductUrl), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse,
                          OptimizationMetadata());

  DidNavigatePrimaryMainFrame(&web);
  DidFinishLoad(&web);
  SimulateProductInfoLocalExtractionTaskFinished();

  histogram_tester_->ExpectTotalCount(metrics::kPDPStateWithLocalMetaName, 0);
}

}  // namespace commerce
