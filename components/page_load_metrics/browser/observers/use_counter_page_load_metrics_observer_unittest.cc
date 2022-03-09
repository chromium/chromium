// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include <memory>
#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "url/gurl.h"

namespace {

const char kTestUrl[] = "https://www.google.com";
using WebFeature = blink::mojom::WebFeature;
using CSSSampleId = blink::mojom::CSSSampleId;
using FeatureType = blink::mojom::UseCounterFeatureType;

const char* GetUseCounterHistogramName(
    blink::mojom::UseCounterFeatureType feature_type) {
  switch (feature_type) {
    case FeatureType::kWebFeature:
      return internal::kFeaturesHistogramName;
    case FeatureType::kCssProperty:
      return internal::kCssPropertiesHistogramName;
    case FeatureType::kAnimatedCssProperty:
      return internal::kAnimatedCssPropertiesHistogramName;
    case FeatureType::kPermissionsPolicyViolationEnforce:
      return internal::kPermissionsPolicyViolationHistogramName;
    case FeatureType::kPermissionsPolicyHeader:
      return internal::kPermissionsPolicyHeaderHistogramName;
    case FeatureType::kPermissionsPolicyIframeAttribute:
      return internal::kPermissionsPolicyIframeAttributeHistogramName;
    case FeatureType::kUserAgentOverride:
      return internal::kUserAgentOverrideHistogramName;
  }
}

}  // namespace

class UseCounterPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 public:
  UseCounterPageLoadMetricsObserverTest() {}

  UseCounterPageLoadMetricsObserverTest(
      const UseCounterPageLoadMetricsObserverTest&) = delete;
  UseCounterPageLoadMetricsObserverTest& operator=(
      const UseCounterPageLoadMetricsObserverTest&) = delete;

  void ExpectBucketCount(const blink::UseCounterFeature& feature,
                         size_t count) {
    if (feature.type() == blink::mojom::UseCounterFeatureType::kWebFeature) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramMainFrameName,
          static_cast<base::Histogram::Sample>(feature.value()), count);
    }

    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(feature.type()),
        static_cast<base::Histogram::Sample>(feature.value()), count);
  }

  void HistogramBasicTest(
      const std::vector<blink::UseCounterFeature>& first_features,
      const std::vector<blink::UseCounterFeature>& second_features = {}) {
    NavigateAndCommit(GURL(kTestUrl));

    tester()->SimulateFeaturesUpdate(first_features);
    // Verify that kPageVisits is observed on commit.
    tester()->histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);
    tester()->histogram_tester().ExpectBucketCount(
        internal::kFeaturesHistogramMainFrameName,
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);
    // Verify that page visit is recorded for CSS histograms.
    tester()->histogram_tester().ExpectBucketCount(
        internal::kCssPropertiesHistogramName,
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
    tester()->histogram_tester().ExpectBucketCount(
        internal::kAnimatedCssPropertiesHistogramName,
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);

    for (const auto& feature : first_features)
      ExpectBucketCount(feature, 1ul);

    tester()->SimulateFeaturesUpdate(second_features);

    for (const auto& feature : first_features)
      ExpectBucketCount(feature, 1ul);
    for (const auto& feature : second_features)
      ExpectBucketCount(feature, 1ul);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());
  }
};

TEST_F(UseCounterPageLoadMetricsObserverTest, CountOneFeature) {
  HistogramBasicTest({{blink::mojom::UseCounterFeatureType::kWebFeature, 0}});
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountFeatures) {
  HistogramBasicTest(
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
      },
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 2},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
      });
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountDuplicatedFeatures) {
  HistogramBasicTest(
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 3},
      },
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 2},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
      });
}
