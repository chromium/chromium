// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include <memory>
#include <vector>

#include "base/macros.h"
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

}  // namespace

class UseCounterPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 public:
  UseCounterPageLoadMetricsObserverTest() {}

  void HistogramBasicTest(
      const page_load_metrics::mojom::PageLoadFeatures& first_features,
      const page_load_metrics::mojom::PageLoadFeatures& second_features =
          page_load_metrics::mojom::PageLoadFeatures()) {
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

    for (auto feature : first_features.features) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
      tester()->histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramMainFrameName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }

    tester()->SimulateFeaturesUpdate(second_features);
    for (auto feature : first_features.features) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
      tester()->histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramMainFrameName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }
    for (auto feature : second_features.features) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramName,
          static_cast<base::Histogram::Sample>(feature), 1);
      tester()->histogram_tester().ExpectBucketCount(
          internal::kFeaturesHistogramMainFrameName,
          static_cast<base::Histogram::Sample>(feature), 1);
    }
  }

  void CssHistogramBasicTest(
      const page_load_metrics::mojom::PageLoadFeatures& first_features,
      const page_load_metrics::mojom::PageLoadFeatures& second_features =
          page_load_metrics::mojom::PageLoadFeatures()) {
    NavigateAndCommit(GURL(kTestUrl));
    tester()->SimulateFeaturesUpdate(first_features);
    // Verify that page visit is recorded for CSS histograms.
    tester()->histogram_tester().ExpectBucketCount(
        internal::kCssPropertiesHistogramName,
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);

    for (auto feature : first_features.css_properties) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kCssPropertiesHistogramName, feature, 1);
    }

    tester()->SimulateFeaturesUpdate(second_features);
    for (auto feature : first_features.css_properties) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kCssPropertiesHistogramName, feature, 1);
    }
    for (auto feature : second_features.css_properties) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kCssPropertiesHistogramName, feature, 1);
    }
  }

  void AnimatedCssHistogramBasicTest(
      const page_load_metrics::mojom::PageLoadFeatures& first_features,
      const page_load_metrics::mojom::PageLoadFeatures& second_features =
          page_load_metrics::mojom::PageLoadFeatures()) {
    NavigateAndCommit(GURL(kTestUrl));
    tester()->SimulateFeaturesUpdate(first_features);
    // Verify that page visit is recorded for CSS histograms.
    tester()->histogram_tester().ExpectBucketCount(
        internal::kAnimatedCssPropertiesHistogramName,
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);

    for (auto feature : first_features.animated_css_properties) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kAnimatedCssPropertiesHistogramName, feature, 1);
    }

    tester()->SimulateFeaturesUpdate(second_features);
    for (auto feature : first_features.animated_css_properties) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kAnimatedCssPropertiesHistogramName, feature, 1);
    }
    for (auto feature : second_features.animated_css_properties) {
      tester()->histogram_tester().ExpectBucketCount(
          internal::kAnimatedCssPropertiesHistogramName, feature, 1);
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UseCounterPageLoadMetricsObserverTest);
};

TEST_F(UseCounterPageLoadMetricsObserverTest, CountOneFeature) {
  std::vector<WebFeature> features({WebFeature::kFetch});
  page_load_metrics::mojom::PageLoadFeatures page_load_features;
  page_load_features.features = features;
  HistogramBasicTest(page_load_features);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1({WebFeature::kWindowFind});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, CountDuplicatedFeatures) {
  std::vector<WebFeature> features_0(
      {WebFeature::kFetch, WebFeature::kFetch, WebFeature::kFetchBodyStream});
  std::vector<WebFeature> features_1(
      {WebFeature::kFetch, WebFeature::kWindowFind});
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.features = features_0;
  page_load_features_1.features = features_1;
  HistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordCSSProperties) {
  // CSSPropertyID::kFont (5), CSSPropertyID::kZoom (19)
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.css_properties = {CSSSampleId::kFont,
                                         CSSSampleId::kZoom};
  page_load_features_1.css_properties = {CSSSampleId::kZoom};
  CssHistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordAnimatedCSSProperties) {
  page_load_metrics::mojom::PageLoadFeatures page_load_features_0;
  page_load_metrics::mojom::PageLoadFeatures page_load_features_1;
  page_load_features_0.css_properties = {CSSSampleId::kFont,
                                         CSSSampleId::kZoom};
  page_load_features_1.css_properties = {CSSSampleId::kZoom};
  AnimatedCssHistogramBasicTest(page_load_features_0, page_load_features_1);
}

TEST_F(UseCounterPageLoadMetricsObserverTest, RecordCSSPropertiesInRange) {
  page_load_metrics::mojom::PageLoadFeatures page_load_features;
  page_load_features.css_properties = {CSSSampleId::kColor,
                                       CSSSampleId::kMaxValue};
  CssHistogramBasicTest(page_load_features);
}
