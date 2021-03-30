// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/prerender_page_load_metrics_observer.h"

#include <memory>

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"

namespace {

const char kDefaultTestUrl[] = "https://a.test";
const char kOtherOriginUrl[] = "https://b.test";
const char kFeaturesHistogramName[] = "Blink.UseCounter.Features";

class PrerenderPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    // PrerenderPageLoadMetricsObserver requires
    // UseCounterPageLoadMetricsObserver to log UseCounter to UMA.
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());

    tracker->AddObserver(std::make_unique<PrerenderPageLoadMetricsObserver>());
  }

  void SimulateFirstContentfulPaint() {
    page_load_metrics::mojom::PageLoadTiming timing;
    page_load_metrics::InitPageLoadTimingForTest(&timing);
    timing.navigation_start = base::Time::Now();
    timing.parse_timing->parse_stop = base::TimeDelta::FromMilliseconds(50);
    timing.paint_timing->first_contentful_paint =
        base::TimeDelta::FromMilliseconds(100);
    PopulateRequiredTimingFields(&timing);
    tester()->SimulateTimingUpdate(timing);
  }

  int GetPageVisits() {
    return tester()->histogram_tester().GetBucketCount(
        kFeaturesHistogramName, static_cast<base::Histogram::Sample>(
                                    blink::mojom::WebFeature::kPageVisits));
  }

  int GetLocalStorageBeforeFcpCount() {
    return tester()->histogram_tester().GetBucketCount(
        kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(
            blink::mojom::WebFeature::kLocalStorageFirstUsedBeforeFcp));
  }

  int GetLocalStorageAfterFcpCount() {
    return tester()->histogram_tester().GetBucketCount(
        kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(
            blink::mojom::WebFeature::kLocalStorageFirstUsedAfterFcp));
  }

  int GetSessionStorageBeforeFcpCount() {
    return tester()->histogram_tester().GetBucketCount(
        kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(
            blink::mojom::WebFeature::kSessionStorageFirstUsedBeforeFcp));
  }

  int GetSessionStorageAfterFcpCount() {
    return tester()->histogram_tester().GetBucketCount(
        kFeaturesHistogramName,
        static_cast<base::Histogram::Sample>(
            blink::mojom::WebFeature::kSessionStorageFirstUsedAfterFcp));
  }
};

TEST_F(PrerenderPageLoadMetricsObserverTest, NoLocalStorage) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetLocalStorageBeforeFcpCount(), 0);
  EXPECT_EQ(GetLocalStorageAfterFcpCount(), 0);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, LocalStorageBeforeFcp) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Access local storage.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kLocalStorage);

  // Reach FCP.
  SimulateFirstContentfulPaint();

  // Access local storage again.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kLocalStorage);

  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetLocalStorageBeforeFcpCount(), 1);
  // The UMA counts the first use, so AfterFcp is 0.
  EXPECT_EQ(GetLocalStorageAfterFcpCount(), 0);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, LocalStorageAfterFcp) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Reach FCP.
  SimulateFirstContentfulPaint();

  // Access local storage.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kLocalStorage);

  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetLocalStorageBeforeFcpCount(), 0);
  EXPECT_EQ(GetLocalStorageAfterFcpCount(), 1);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, ThirdPartyLocalStorage) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  tester()->SimulateStorageAccess(
      GURL(kOtherOriginUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kLocalStorage);

  // Cross-origin storage is not logged.
  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetLocalStorageBeforeFcpCount(), 0);
  EXPECT_EQ(GetLocalStorageAfterFcpCount(), 0);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, NoSessionStorage) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetSessionStorageBeforeFcpCount(), 0);
  EXPECT_EQ(GetSessionStorageAfterFcpCount(), 0);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, SessionStorageBeforeFcp) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Access session storage.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kSessionStorage);

  // Reach FCP.
  SimulateFirstContentfulPaint();

  // Access session storage again.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kSessionStorage);

  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetSessionStorageBeforeFcpCount(), 1);
  // The UMA counts the first use, so AfterFcp is 0.
  EXPECT_EQ(GetSessionStorageAfterFcpCount(), 0);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, SessionStorageAfterFcp) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Reach FCP.
  SimulateFirstContentfulPaint();

  // Access session storage.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kSessionStorage);

  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetSessionStorageBeforeFcpCount(), 0);
  EXPECT_EQ(GetSessionStorageAfterFcpCount(), 1);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, ThirdPartySessionStorage) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  tester()->SimulateStorageAccess(
      GURL(kOtherOriginUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kSessionStorage);

  // Cross-origin storage is not logged.
  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetSessionStorageBeforeFcpCount(), 0);
  EXPECT_EQ(GetSessionStorageAfterFcpCount(), 0);
}

TEST_F(PrerenderPageLoadMetricsObserverTest, MultipleStorage) {
  NavigateAndCommit(GURL(kDefaultTestUrl));

  // Access local storage.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kLocalStorage);

  // Reach FCP.
  SimulateFirstContentfulPaint();

  // Access session storage.
  tester()->SimulateStorageAccess(
      GURL(kDefaultTestUrl), GURL(kDefaultTestUrl), false,
      page_load_metrics::StorageType::kSessionStorage);

  EXPECT_EQ(GetPageVisits(), 1);
  EXPECT_EQ(GetLocalStorageBeforeFcpCount(), 1);
  EXPECT_EQ(GetLocalStorageAfterFcpCount(), 0);
  EXPECT_EQ(GetSessionStorageBeforeFcpCount(), 0);
  EXPECT_EQ(GetSessionStorageAfterFcpCount(), 1);
}

}  // namespace
