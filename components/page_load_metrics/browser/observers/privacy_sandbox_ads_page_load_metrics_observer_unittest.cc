// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/privacy_sandbox_ads_page_load_metrics_observer.h"

#include <memory>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

using WebFeature = blink::mojom::WebFeature;

constexpr char kHistogramPrivacySandboxAdsFirstInputDelay4Prefix[] =
    "PageLoad.Clients.PrivacySandboxAds.InteractiveTiming.FirstInputDelay4.";

constexpr char
    kHistogramPrivacySandboxAdsNavigationToFirstContentfulPaintPrefix[] =
        "PageLoad.Clients.PrivacySandboxAds.PaintTiming."
        "NavigationToFirstContentfulPaint.";

constexpr char
    kHistogramPrivacySandboxAdsNavigationToLargestContentfulPaint2Prefix[] =
        "PageLoad.Clients.PrivacySandboxAds.PaintTiming."
        "NavigationToLargestContentfulPaint2.";

constexpr char kHistogramPrivacySandboxAdsMaxCumulativeShiftScorePrefix[] =
    "PageLoad.Clients.PrivacySandboxAds.LayoutInstability."
    "MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms2.";

constexpr char kTestUrl[] = "https://a.test/test";

struct TestCase {
  const char* name;
  base::flat_set<WebFeature> web_features;
};

}  // namespace

class PrivacySandboxAdsPageLoadMetricsObserverTest
    : public PageLoadMetricsObserverContentTestHarness,
      public ::testing::WithParamInterface<TestCase> {
 protected:
  void HistogramTest(const base::flat_set<WebFeature>& web_features) {
    NavigateAndCommit(GURL(kTestUrl));

    std::vector<blink::UseCounterFeature> enabled_features;
    for (auto feature : web_features) {
      enabled_features.emplace_back(
          blink::mojom::UseCounterFeatureType::kWebFeature,
          static_cast<int>(feature));
    }
    tester()->SimulateFeaturesUpdate(enabled_features);

    PopulateTimingForHistograms();
    tester()->NavigateToUntrackedUrl();

    static const base::flat_map<PrivacySandboxAdsApi, std::vector<WebFeature>>
        kFeaturesMap = {
            {PrivacySandboxAdsApi::kAttributionReporting,
             {WebFeature::kAttributionReportingAPIAll}},
            {PrivacySandboxAdsApi::kFencedFrames,
             {WebFeature::kHTMLFencedFrameElement}},
            {PrivacySandboxAdsApi::kProtectedAudienceRunAdAuction,
             {WebFeature::kV8Navigator_RunAdAuction_Method}},
            {PrivacySandboxAdsApi::kProtectedAudienceJoinAdInterestGroup,
             {WebFeature::kV8Navigator_JoinAdInterestGroup_Method}},
            {PrivacySandboxAdsApi::kPrivateAggregation,
             {WebFeature::kPrivateAggregationApiAll}},
            {PrivacySandboxAdsApi::kSharedStorage,
             {WebFeature::kSharedStorageAPI_SharedStorage_DOMReference,
              WebFeature::kSharedStorageAPI_Run_Method,
              WebFeature::kSharedStorageAPI_SelectURL_Method}},
            {PrivacySandboxAdsApi::kTopics,
             {WebFeature::kTopicsAPI_BrowsingTopics_Method}},
        };

    for (auto api :
         base::EnumSet<PrivacySandboxAdsApi, PrivacySandboxAdsApi::kMinValue,
                       PrivacySandboxAdsApi::kMaxValue>::All()) {
      int expected_count = base::ranges::any_of(
          kFeaturesMap.at(api), [&](WebFeature web_feature) {
            return web_features.contains(web_feature);
          });

      tester()->histogram_tester().ExpectTotalCount(
          PrivacySandboxAdsPageLoadMetricsObserver::GetHistogramName(
              kHistogramPrivacySandboxAdsFirstInputDelay4Prefix, api),
          expected_count);

      tester()->histogram_tester().ExpectTotalCount(
          PrivacySandboxAdsPageLoadMetricsObserver::GetHistogramName(
              kHistogramPrivacySandboxAdsNavigationToFirstContentfulPaintPrefix,
              api),
          expected_count);

      tester()->histogram_tester().ExpectTotalCount(
          PrivacySandboxAdsPageLoadMetricsObserver::GetHistogramName(
              kHistogramPrivacySandboxAdsNavigationToLargestContentfulPaint2Prefix,
              api),
          expected_count);

      tester()->histogram_tester().ExpectTotalCount(
          PrivacySandboxAdsPageLoadMetricsObserver::GetHistogramName(
              kHistogramPrivacySandboxAdsMaxCumulativeShiftScorePrefix, api),
          expected_count);
    }
  }

 private:
  using PrivacySandboxAdsApi =
      PrivacySandboxAdsPageLoadMetricsObserver::PrivacySandboxAdsApi;

  void SetUp() override { PageLoadMetricsObserverContentTestHarness::SetUp(); }

  void RegisterObservers(PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<PrivacySandboxAdsPageLoadMetricsObserver>());
  }

  void PopulateTimingForHistograms() {
    mojom::PageLoadTiming timing;
    InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::Now();
    timing.parse_timing->parse_stop = base::Milliseconds(50);
    timing.paint_timing->first_image_paint = base::Milliseconds(80);
    timing.paint_timing->first_contentful_paint = base::Milliseconds(100);

    auto largest_contentful_paint = CreateLargestContentfulPaintTiming();
    largest_contentful_paint->largest_image_paint = base::Milliseconds(100);
    largest_contentful_paint->largest_image_paint_size = 100;
    timing.paint_timing->largest_contentful_paint =
        std::move(largest_contentful_paint);

    timing.interactive_timing->first_input_delay = base::Milliseconds(10);
    timing.interactive_timing->first_input_timestamp = base::Milliseconds(4780);

    PopulateRequiredTimingFields(&timing);
    tester()->SimulateTimingUpdate(timing);
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    PrivacySandboxAdsPageLoadMetricsObserverTest,
    ::testing::Values(
        TestCase{
            .name = "none",
            .web_features = {},
        },
        TestCase{
            .name = "all",
            .web_features =
                {WebFeature::kAttributionReportingAPIAll,
                 WebFeature::kHTMLFencedFrameElement,
                 WebFeature::kV8Navigator_RunAdAuction_Method,
                 WebFeature::kV8Navigator_JoinAdInterestGroup_Method,
                 WebFeature::kPrivateAggregationApiAll,
                 WebFeature::kSharedStorageAPI_SharedStorage_DOMReference,
                 WebFeature::kSharedStorageAPI_Run_Method,
                 WebFeature::kSharedStorageAPI_SelectURL_Method,
                 WebFeature::kTopicsAPI_BrowsingTopics_Method},
        },
        TestCase{
            .name = "run_ad_auction",
            .web_features = {WebFeature::kV8Navigator_RunAdAuction_Method},
        },
        TestCase{
            .name = "join_ad_interest_group",
            .web_features =
                {WebFeature::kV8Navigator_JoinAdInterestGroup_Method},
        },
        TestCase{
            .name = "shared_storage_dom_reference",
            .web_features =
                {WebFeature::kSharedStorageAPI_SharedStorage_DOMReference},
        },
        TestCase{
            .name = "shared_storage_run",
            .web_features = {WebFeature::kSharedStorageAPI_Run_Method},
        },
        TestCase{
            .name = "shared_storage_select_url",
            .web_features = {WebFeature::kSharedStorageAPI_SelectURL_Method},
        }),
    [](const auto& info) { return info.param.name; });  // test name generator

TEST_P(PrivacySandboxAdsPageLoadMetricsObserverTest, VerifyMetrics) {
  HistogramTest(GetParam().web_features);
}

}  // namespace page_load_metrics
