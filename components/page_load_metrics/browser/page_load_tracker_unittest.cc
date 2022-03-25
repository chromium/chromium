// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_tracker.h"

#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/blink/public/common/features.h"

namespace page_load_metrics {

namespace {

const char kTestUrl[] = "https://a.test";

struct PageLoadMetricsObserverEvents final {
  bool was_started = false;
  bool was_prerender_started = false;
  bool was_fenced_frames_started = false;
  bool was_committed = false;
};

class TestPageLoadMetricsObserver final : public PageLoadMetricsObserver {
 public:
  TestPageLoadMetricsObserver(raw_ptr<PageLoadMetricsObserverEvents> events)
      : events_(events) {}

  void StopObservingOnPrerender() { stop_on_prerender_ = true; }

 private:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    events_->was_started = true;
    return CONTINUE_OBSERVING;
  }
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    events_->was_fenced_frames_started = true;
    return CONTINUE_OBSERVING;
  }

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    events_->was_prerender_started = true;
    return stop_on_prerender_ ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override {
    events_->was_committed = true;
    return CONTINUE_OBSERVING;
  }

  bool stop_on_prerender_ = false;

  // Event records should be owned outside this class as this instance will be
  // automatically destructed on STOP_OBSERVING, and so on.
  raw_ptr<PageLoadMetricsObserverEvents> events_;
};

class PageLoadTrackerTest : public PageLoadMetricsObserverContentTestHarness {
 public:
  PageLoadTrackerTest() : observer_(new TestPageLoadMetricsObserver(&events_)) {
    // Force to enable Prerender2.
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kPrerender2},
        {blink::features::kPrerender2MemoryControls});
  }

 protected:
  void SetTargetUrl(const std::string& url) { target_url_ = GURL(url); }
  const PageLoadMetricsObserverEvents& GetEvents() const { return events_; }

  void StopObservingOnPrerender() { observer_->StopObservingOnPrerender(); }

 private:
  void RegisterObservers(PageLoadTracker* tracker) override {
    if (tracker->GetUrl() != target_url_)
      return;

    tracker->AddObserver(std::unique_ptr<PageLoadMetricsObserver>(observer_));
  }

  PageLoadMetricsObserverEvents events_;
  raw_ptr<TestPageLoadMetricsObserver> observer_;

  base::test::ScopedFeatureList scoped_feature_list_;
  GURL target_url_;
};

TEST_F(PageLoadTrackerTest, PrimaryPageType) {
  // Target URL to monitor the tracker via the test observer.
  SetTargetUrl(kTestUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_FALSE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectUniqueSample(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);
}

TEST_F(PageLoadTrackerTest, PrerenderPageType) {
  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_TRUE(GetEvents().was_committed);

  // Check metrics.
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrerenderPage, 1);
}

TEST_F(PageLoadTrackerTest, StopObservingOnPrerender) {
  // Target URL to monitor the tracker via the test observer.
  const char kPrerenderingUrl[] = "https://a.test/prerender";
  SetTargetUrl(kPrerenderingUrl);
  StopObservingOnPrerender();

  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a prerender page.
  content::WebContentsTester::For(web_contents())
      ->AddPrerenderAndCommitNavigation(GURL(kPrerenderingUrl));

  // Check observer behaviors.
  EXPECT_FALSE(GetEvents().was_started);
  EXPECT_FALSE(GetEvents().was_fenced_frames_started);
  EXPECT_TRUE(GetEvents().was_prerender_started);
  EXPECT_FALSE(GetEvents().was_committed);
}

// TODO(https://crbug.com/1301880): Add tests for FencedFrames cases.

}  // namespace

}  // namespace page_load_metrics
