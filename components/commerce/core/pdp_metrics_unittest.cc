// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/shopping_service_test_base.h"
#include "components/optimization_guide/core/new_optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationGuideDecisionCallback;
using optimization_guide::OptimizationMetadata;
using optimization_guide::proto::Any;
using optimization_guide::proto::OptimizationType;

namespace commerce {

class PDPMetricsTest : public ShoppingServiceTestBase {
 public:
  PDPMetricsTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
    test_features_.InitAndEnableFeature(commerce::kShoppingPDPMetrics);
  }
  PDPMetricsTest(const PDPMetricsTest&) = delete;
  PDPMetricsTest operator=(const PDPMetricsTest&) = delete;
  ~PDPMetricsTest() override = default;

  base::test::ScopedFeatureList test_features_;
};

// Test that PDP metrics for the page are recorded.
TEST_F(PDPMetricsTest, TestPDPIsRecorded) {
  std::string url = "http://example.com";

  OptimizationMetadata meta =
      opt_guide_->BuildPriceTrackingResponse("title", url, 123, 456, "US");

  base::HistogramTester histogram_tester;

  opt_guide_->SetResponse(GURL(url), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  MockWebWrapper web(GURL(url), false);

  DidNavigatePrimaryMainFrame(&web);

  histogram_tester.ExpectBucketCount(
      metrics::kPDPStateHistogramName,
      metrics::ShoppingPDPState::kIsPDPWithClusterId, 1);
  histogram_tester.ExpectBucketCount(metrics::kPDPStateHistogramName,
                                     metrics::ShoppingPDPState::kNotPDP, 0);
}

// Test that PDP metrics for an incognito page are not recorded.
TEST_F(PDPMetricsTest, TestIncognitoPDPIsNotRecorded) {
  std::string url = "http://example.com";

  OptimizationMetadata meta =
      opt_guide_->BuildPriceTrackingResponse("title", url, 123, 456, "US");

  base::HistogramTester histogram_tester;

  opt_guide_->SetResponse(GURL(url), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kTrue, meta);

  MockWebWrapper web(GURL(url), true);

  DidNavigatePrimaryMainFrame(&web);

  histogram_tester.ExpectBucketCount(
      metrics::kPDPStateHistogramName,
      metrics::ShoppingPDPState::kIsPDPWithClusterId, 0);
  histogram_tester.ExpectBucketCount(metrics::kPDPStateHistogramName,
                                     metrics::ShoppingPDPState::kNotPDP, 0);
}

// Test that a page that isn't considered a PDP is recorded.
TEST_F(PDPMetricsTest, TestFalseOptGuideResponseIsRecorded) {
  std::string url = "http://example.com";

  OptimizationMetadata meta =
      opt_guide_->BuildPriceTrackingResponse("title", url, 123, 456, "US");

  base::HistogramTester histogram_tester;

  opt_guide_->SetResponse(GURL(url), OptimizationType::PRICE_TRACKING,
                          OptimizationGuideDecision::kFalse, meta);

  MockWebWrapper web(GURL(url), false);

  DidNavigatePrimaryMainFrame(&web);

  histogram_tester.ExpectBucketCount(
      metrics::kPDPStateHistogramName,
      metrics::ShoppingPDPState::kIsPDPWithClusterId, 0);
  histogram_tester.ExpectBucketCount(metrics::kPDPStateHistogramName,
                                     metrics::ShoppingPDPState::kNotPDP, 1);
}

}  // namespace commerce
