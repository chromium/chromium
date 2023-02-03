// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/cache_transparency_page_load_metrics_observer.h"

#include <memory>

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

using UserInteractionLatencies =
    page_load_metrics::mojom::UserInteractionLatencies;
using UserInteractionLatency = page_load_metrics::mojom::UserInteractionLatency;
using UserInteractionType = page_load_metrics::mojom::UserInteractionType;

namespace {

constexpr char kExampleUrl[] = "http://www.example.com/";

}  // namespace

class CacheTransparencyPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<CacheTransparencyPageLoadMetricsObserver>());
  }

  void InitializeTestPageLoadTiming(
      page_load_metrics::mojom::PageLoadTiming* timing) {
    page_load_metrics::InitPageLoadTimingForTest(timing);
    timing->navigation_start = base::Time::FromDoubleT(1);
    timing->parse_timing->parse_start = base::Milliseconds(1);
    timing->paint_timing->first_contentful_paint = base::Milliseconds(300);
    timing->paint_timing->largest_contentful_paint->largest_text_paint =
        base::Milliseconds(500);
    timing->paint_timing->largest_contentful_paint->largest_text_paint_size =
        100u;
    PopulateRequiredTimingFields(timing);
  }
};

TEST_F(CacheTransparencyPageLoadMetricsObserverTest,
       PageLoadMetricsEmittedWhenFeaturesEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::string pervasive_payloads_params =
      "1,http://127.0.0.1:4353/pervasive.js,"
      "2478392C652868C0AAF0316A28284610DBDACF02D66A00B39F3BA75D887F4829";
  feature_list.InitWithFeaturesAndParameters(
      {{network::features::kPervasivePayloadsList,
        {{"pervasive-payloads", pervasive_payloads_params}}},
       {network::features::kCacheTransparency, {}}},
      {/* disabled_features */});

  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // UMAs are emitted if Cache Transparency features are enabled.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyFirstContentfulPaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyLargestContentfulPaint, 1);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyCumulativeLayoutShift, 1);
}

TEST_F(CacheTransparencyPageLoadMetricsObserverTest,
       PageLoadMetricsEmittedWhenFeaturesNotEnabled) {
  page_load_metrics::mojom::PageLoadTiming timing;
  InitializeTestPageLoadTiming(&timing);

  page_load_metrics::mojom::InputTiming input_timing;
  input_timing.num_interactions = 3;
  input_timing.max_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& max_event_durations =
      input_timing.max_event_durations->get_user_interaction_latencies();
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(50), UserInteractionType::kKeyboard));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(100), UserInteractionType::kTapOrClick));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(150), UserInteractionType::kDrag));

  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateInputTimingUpdate(input_timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  // UMAs are not emitted if Cache Transparency features are not enabled.
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyFirstContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyLargestContentfulPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyInteractionToNextPaint, 0);
  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyCumulativeLayoutShift, 0);
}

TEST_F(CacheTransparencyPageLoadMetricsObserverTest,
       NormalizedResponsivenessMetrics) {
  base::test::ScopedFeatureList feature_list;
  std::string pervasive_payloads_params =
      "1,http://127.0.0.1:4353/pervasive.js,"
      "2478392C652868C0AAF0316A28284610DBDACF02D66A00B39F3BA75D887F4829";
  feature_list.InitWithFeaturesAndParameters(
      {{network::features::kPervasivePayloadsList,
        {{"pervasive-payloads", pervasive_payloads_params}}},
       {network::features::kCacheTransparency, {}}},
      {/* disabled_features */});

  page_load_metrics::mojom::InputTiming input_timing;
  input_timing.num_interactions = 3;
  input_timing.max_event_durations =
      UserInteractionLatencies::NewUserInteractionLatencies({});
  auto& max_event_durations =
      input_timing.max_event_durations->get_user_interaction_latencies();
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(50), UserInteractionType::kKeyboard));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(100), UserInteractionType::kTapOrClick));
  max_event_durations.emplace_back(UserInteractionLatency::New(
      base::Milliseconds(150), UserInteractionType::kDrag));

  NavigateAndCommit(GURL(kExampleUrl));
  tester()->SimulateInputTimingUpdate(input_timing);

  // Navigate again to force logging.
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      internal::kHistogramCacheTransparencyInteractionToNextPaint, 1);
}
