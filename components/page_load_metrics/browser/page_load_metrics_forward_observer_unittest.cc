// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_forward_observer.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

const char kTestUrl[] = "https://a.test/";

struct PageLoadMetricsObserverEvents final {
  bool was_started = false;
  bool was_fenced_frames_started = false;
  size_t event_count = 0;
};

class TestPageLoadMetricsObserver final : public PageLoadMetricsObserver {
 public:
  explicit TestPageLoadMetricsObserver(
      raw_ptr<PageLoadMetricsObserverEvents> events)
      : events_(events) {}
  TestPageLoadMetricsObserver(const TestPageLoadMetricsObserver&) = delete;
  TestPageLoadMetricsObserver& operator=(const TestPageLoadMetricsObserver&) =
      delete;

 private:
  // PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override {
    static const char kObserverName[] = "TestObserver";
    return kObserverName;
  }

  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    events_->was_started = true;
    return CONTINUE_OBSERVING;
  }

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    return STOP_OBSERVING;
  }

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    is_in_fenced_frames_ = true;
    events_->was_fenced_frames_started = true;
    return FORWARD_OBSERVING;
  }

  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override {
    // TestPageLoadMetricsObserver will be instantiated for the Primary page and
    // a FencedFrames page. As instance for a FencedFrames page will be
    // destructed after `OnFencedFramesStart` and `ShouldObserverMimeType` will
    // not be invoked for the instance. Instead, PageLoadMetricsForwardObserver
    // routes the event to the Primary page's observer.
    EXPECT_FALSE(is_in_fenced_frames_);
    if (!is_in_fenced_frames_)
      events_->event_count++;
    return CONTINUE_OBSERVING;
  }

  bool is_in_fenced_frames_ = false;

  // Event records should be owned outside this class as this instance will be
  // automatically destructed on STOP_OBSERVING, and so on.
  raw_ptr<PageLoadMetricsObserverEvents> events_;
};

class PageLoadMetricsForwardObserverTest
    : public PageLoadMetricsObserverContentTestHarness {
 public:
  PageLoadMetricsForwardObserverTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {blink::features::kFencedFrames,
             {{"implementation_type", "mparch"}}},
        },
        {});
  }

 protected:
  const PageLoadMetricsObserverEvents& GetEvents() const { return events_; }

 private:
  void RegisterObservers(PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestPageLoadMetricsObserver>(&events_));
  }

  PageLoadMetricsObserverEvents events_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PageLoadMetricsForwardObserverTest, Basic) {
  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Add a fenced frame.
  content::RenderFrameHost* fenced_frame_root =
      content::RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame())
          ->AppendFencedFrame();
  {
    auto simulator = content::NavigationSimulator::CreateRendererInitiated(
        GURL(kTestUrl), fenced_frame_root);
    ASSERT_NE(nullptr, simulator);
    simulator->Commit();
  }

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_TRUE(GetEvents().was_fenced_frames_started);
  // The event will be invoked twice in the primary page observer, once is for
  // its own, the other is forwarded one from the FencedFrames' page.
  EXPECT_EQ(2u, GetEvents().event_count);

  // Check metrics.
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kPrimaryPage, 1);
  tester()->histogram_tester().ExpectBucketCount(
      internal::kPageLoadTrackerPageType,
      internal::PageLoadTrackerPageType::kFencedFramesPage, 1);
}

}  // namespace

}  // namespace page_load_metrics
