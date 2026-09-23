// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_forward_observer.h"

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "url/gurl.h"

namespace page_load_metrics {

namespace {

const char kTestUrl[] = "https://a.test/";

struct PageLoadMetricsObserverEvents final {
  bool was_started = false;
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
    return STOP_OBSERVING;
  }

  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override {
    events_->event_count++;
    return CONTINUE_OBSERVING;
  }

  // Event records should be owned outside this class as this instance will be
  // automatically destructed on STOP_OBSERVING, and so on.
  raw_ptr<PageLoadMetricsObserverEvents> events_;
};

class PageLoadMetricsForwardObserverTest
    : public PageLoadMetricsObserverContentTestHarness {
 public:
  PageLoadMetricsForwardObserverTest() = default;

 protected:
  const PageLoadMetricsObserverEvents& GetEvents() const { return events_; }

 private:
  void RegisterObservers(PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestPageLoadMetricsObserver>(&events_));
  }

  PageLoadMetricsObserverEvents events_;
};

TEST_F(PageLoadMetricsForwardObserverTest, Basic) {
  // Navigate in.
  NavigateAndCommit(GURL(kTestUrl));

  // Check observer behaviors.
  EXPECT_TRUE(GetEvents().was_started);
  EXPECT_EQ(1u, GetEvents().event_count);
}

}  // namespace

}  // namespace page_load_metrics
