// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/uma_file_and_data_page_load_metrics_observer.h"

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "content/public/browser/web_contents.h"

namespace {
const char kDefaultDataUrl[] = "data:text/html,Hello world";
const char kDefaultFileUrl[] = "file:///file.txt";
}  // namespace

class UmaFileAndDataPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness {
 private:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        base::WrapUnique(new UmaFileAndDataPageLoadMetricsObserver()));
  }
};

TEST_F(UmaFileAndDataPageLoadMetricsObserverTest,
       RelevantFirstContentfulPaintMetricsAreRecordedForDataScheme) {
  base::TimeDelta first_contentful_paint = base::Milliseconds(30);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.response_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = first_contentful_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(15);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10;

  NavigateAndCommit(GURL(kDefaultDataUrl));
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.DataScheme", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.DataScheme",
      first_contentful_paint.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background."
      "HttpsOrDataOrFileScheme",
      first_contentful_paint.InMilliseconds(), 0);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultDataUrl));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.DataScheme", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.DataScheme",
      timing.paint_timing->largest_contentful_paint->largest_text_paint
          ->InMilliseconds(),
      1);
}

TEST_F(UmaFileAndDataPageLoadMetricsObserverTest,
       RelevantBackgroundMetricsAreRecordedForDataScheme) {
  base::TimeDelta first_contentful_paint = base::Milliseconds(30);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.response_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = first_contentful_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(15);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10;

  // Send the page to background.
  web_contents()->WasHidden();

  NavigateAndCommit(GURL(kDefaultDataUrl));
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background."
      "HttpsOrDataOrFileScheme",
      first_contentful_paint.InMilliseconds(), 1);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultDataUrl));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
      "HttpsOrDataOrFileScheme",
      1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
      "HttpsOrDataOrFileScheme",
      timing.paint_timing->largest_contentful_paint->largest_text_paint
          ->InMilliseconds(),
      1);
}

TEST_F(UmaFileAndDataPageLoadMetricsObserverTest,
       RelevantFirstContentfulPaintMetricsAreRecordedForFileScheme) {
  base::TimeDelta first_contentful_paint = base::Milliseconds(30);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.response_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = first_contentful_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(15);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10;

  NavigateAndCommit(GURL(kDefaultFileUrl));
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.FileScheme", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.FileScheme",
      first_contentful_paint.InMilliseconds(), 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background."
      "HttpsOrDataOrFileScheme",
      first_contentful_paint.InMilliseconds(), 0);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultFileUrl));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.FileScheme", 1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.FileScheme",
      timing.paint_timing->largest_contentful_paint->largest_text_paint
          ->InMilliseconds(),
      1);
}

TEST_F(UmaFileAndDataPageLoadMetricsObserverTest,
       RelevantBackgroundMetricsAreRecordedForFileScheme) {
  base::TimeDelta first_contentful_paint = base::Milliseconds(30);

  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);

  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.parse_timing->parse_start = base::Milliseconds(1);
  timing.response_start = base::Milliseconds(1);
  timing.paint_timing->first_paint = first_contentful_paint;
  timing.paint_timing->first_contentful_paint = first_contentful_paint;
  timing.paint_timing->largest_contentful_paint->largest_text_paint =
      base::Milliseconds(15);
  timing.paint_timing->largest_contentful_paint->largest_text_paint_size = 10;

  // Send the page to background.
  web_contents()->WasHidden();

  NavigateAndCommit(GURL(kDefaultFileUrl));
  tester()->SimulateTimingUpdate(timing);

  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background."
      "HttpsOrDataOrFileScheme",
      first_contentful_paint.InMilliseconds(), 1);

  // Navigate again to force histogram recording.
  NavigateAndCommit(GURL(kDefaultFileUrl));

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
      "HttpsOrDataOrFileScheme",
      1);
  tester()->histogram_tester().ExpectBucketCount(
      "PageLoad.PaintTiming.NavigationToLargestContentfulPaint2.Background."
      "HttpsOrDataOrFileScheme",
      timing.paint_timing->largest_contentful_paint->largest_text_paint
          ->InMilliseconds(),
      1);
}
