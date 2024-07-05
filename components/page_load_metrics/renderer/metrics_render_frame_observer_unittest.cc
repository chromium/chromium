// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/metrics_render_frame_observer.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/weak_mock_timer.h"
#include "components/page_load_metrics/renderer/fake_page_timing_sender.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_load_metrics {

// Implementation of the MetricsRenderFrameObserver class we're testing, with
// the GetTiming() method stubbed out to make the rest of the class more
// testable.
class TestMetricsRenderFrameObserver : public MetricsRenderFrameObserver,
                                       public test::WeakMockTimerProvider {
 public:
  TestMetricsRenderFrameObserver() : MetricsRenderFrameObserver(nullptr) {}

  std::unique_ptr<base::OneShotTimer> CreateTimer() override {
    auto timer = std::make_unique<test::WeakMockTimer>();
    SetMockTimer(timer->AsWeakPtr());
    return std::move(timer);
  }

  std::unique_ptr<PageTimingSender> CreatePageTimingSender(
      bool limited_sending_mode) override {
    return base::WrapUnique<PageTimingSender>(
        new FakePageTimingSender(&validator_));
  }

  void ExpectPageLoadTiming(const mojom::PageLoadTiming& timing) {
    SetFakePageLoadTiming(timing);
    validator_.ExpectPageLoadTiming(timing);
  }

  void ExpectCpuTiming(const base::TimeDelta& timing) {
    validator_.ExpectCpuTiming(timing);
  }

  void SetFakePageLoadTiming(const mojom::PageLoadTiming& timing) {
    EXPECT_EQ(nullptr, fake_timing_.get());
    fake_timing_ = timing.Clone();
  }

  void ExpectMainFrameIntersectionRect(
      const gfx::Rect& main_frame_intersection_rect) {
    validator_.UpdateExpectedMainFrameIntersectionRect(
        main_frame_intersection_rect);
  }

  Timing GetTiming() const override {
    EXPECT_NE(nullptr, fake_timing_.get());
    return Timing(std::move(fake_timing_),
                  PageTimingMetadataRecorder::MonotonicTiming());
  }

  mojom::CustomUserTimingMarkPtr GetCustomUserTimingMark() const override {
    return mojom::CustomUserTimingMark::New("fake_user_timing_mark",
                                            base::Milliseconds(100));
  }

  void ExpectSoftNavigationMetrics(
      const mojom::SoftNavigationMetrics& soft_navigation_metrics) {
    fake_soft_navigation_metrics_ = soft_navigation_metrics.Clone();
    validator_.ExpectSoftNavigationMetrics(soft_navigation_metrics);
  }

  void ExpectSoftNavigationMetrics() {
    validator_.ExpectSoftNavigationMetrics(
        *fake_soft_navigation_metrics_->Clone());
  }

  void VerifyExpectedSoftNavigationMetrics() const {
    validator_.VerifyExpectedSoftNavigationMetrics();
  }

  mojom::SoftNavigationMetricsPtr GetSoftNavigationMetrics() const override {
    return fake_soft_navigation_metrics_->Clone();
  }

  void VerifyExpectedTimings() const {
    EXPECT_EQ(nullptr, fake_timing_.get());
    validator_.VerifyExpectedTimings();
  }

  bool HasNoRenderFrame() const override { return false; }

  bool IsMainFrame() const override { return true; }

 private:
  FakePageTimingSender::PageTimingValidator validator_;
  mutable mojom::PageLoadTimingPtr fake_timing_;
  mojom::SoftNavigationMetricsPtr fake_soft_navigation_metrics_ =
      mojom::SoftNavigationMetrics::New(blink::kSoftNavigationCountDefaultValue,
                                        base::Milliseconds(0),
                                        std::string(),
                                        CreateLargestContentfulPaintTiming());
};

typedef testing::Test MetricsRenderFrameObserverTest;

TEST_F(MetricsRenderFrameObserverTest, NoMetrics) {
  TestMetricsRenderFrameObserver observer;
  observer.DidChangePerformanceTiming();
  ASSERT_EQ(nullptr, observer.GetMockTimer());
}

TEST_F(MetricsRenderFrameObserverTest, SingleMetric) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);

  TestMetricsRenderFrameObserver observer;

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics();

  observer.DidStartNavigation(GURL(), std::nullopt);
  observer.ReadyToCommitNavigation(nullptr);
  observer.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer.GetMockTimer()->Fire();

  timing.parse_timing->parse_start = base::Milliseconds(10);
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics();

  observer.DidChangePerformanceTiming();
  observer.GetMockTimer()->Fire();
}

TEST_F(MetricsRenderFrameObserverTest,
       MainFrameIntersectionUpdateBeforeMetricsSenderCreated) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);

  TestMetricsRenderFrameObserver observer;
  observer.OnMainFrameIntersectionChanged(gfx::Rect(1, 2, 3, 4));

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics();
  observer.DidStartNavigation(GURL(), std::nullopt);
  observer.ReadyToCommitNavigation(nullptr);
  observer.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);

  observer.ExpectMainFrameIntersectionRect(gfx::Rect(1, 2, 3, 4));

  observer.GetMockTimer()->Fire();
}

// Verify that when two CpuTimings come in, they're grouped into a single
// Message with the total being the sum of the two.
TEST_F(MetricsRenderFrameObserverTest, SingleCpuMetric) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);
  TestMetricsRenderFrameObserver observer;
  mojom::PageLoadTiming timing;

  // Initialize the page and add the initial timing info to the expected.
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics();
  observer.DidStartNavigation(GURL(), std::nullopt);
  observer.ReadyToCommitNavigation(nullptr);
  observer.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);

  // Send cpu timing updates and verify the expected result.
  observer.DidChangeCpuTiming(base::Milliseconds(110));
  observer.DidChangeCpuTiming(base::Milliseconds(50));
  observer.ExpectCpuTiming(base::Milliseconds(160));
  observer.GetMockTimer()->Fire();
}

TEST_F(MetricsRenderFrameObserverTest, MultipleMetrics) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);
  base::TimeDelta dom_event = base::Milliseconds(2);
  base::TimeDelta load_event = base::Milliseconds(2);

  TestMetricsRenderFrameObserver observer;

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  mojom::SoftNavigationMetricsPtr soft_navigation_metrics =
      mojom::SoftNavigationMetrics::New(blink::kSoftNavigationCountDefaultValue,
                                        base::Milliseconds(0), std::string(),
                                        CreateLargestContentfulPaintTiming());
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics(*soft_navigation_metrics);

  observer.DidStartNavigation(GURL(), std::nullopt);
  observer.ReadyToCommitNavigation(nullptr);
  observer.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer.GetMockTimer()->Fire();

  timing.document_timing->dom_content_loaded_event_start = dom_event;
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics(*soft_navigation_metrics);

  observer.DidChangePerformanceTiming();
  observer.GetMockTimer()->Fire();

  // At this point, we should have triggered the generation of two metrics.
  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  observer.VerifyExpectedTimings();

  timing.document_timing->load_event_start = load_event;
  observer.ExpectPageLoadTiming(timing);
  // Expect a soft navigation metrics being sent because of soft navigation
  // detection.
  soft_navigation_metrics->count = 1;

  soft_navigation_metrics->start_time =
      base::TimeDelta() + base::Milliseconds(221.1);

  soft_navigation_metrics->navigation_id =
      "94befe01-5108-43f2-9ec9-936dd0f36e02";

  observer.ExpectSoftNavigationMetrics(*soft_navigation_metrics);

  observer.DidChangePerformanceTiming();
  observer.GetMockTimer()->Fire();

  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  observer.VerifyExpectedTimings();
  observer.VerifyExpectedSoftNavigationMetrics();

  // The PageLoadTiming above includes timing information for the first layout,
  // dom content, and load metrics. However, since we've already generated
  // timing information for all of these metrics previously, we do not expect
  // this invocation to generate any additional metrics.
  observer.SetFakePageLoadTiming(timing);
  observer.DidChangePerformanceTiming();
  ASSERT_FALSE(observer.GetMockTimer()->IsRunning());

  // Expect a non-empty soft navigation metric being sent because of largest
  // contentful paint update.

  // This page load timing is the same as the previous one, but as the soft
  // navigation metric is being sent, this timing is also sent along with the
  // soft navigation metric. Therefore we should expect 1 more page load
  // timing.
  observer.ExpectPageLoadTiming(timing);

  soft_navigation_metrics->largest_contentful_paint =
      CreateLargestContentfulPaintTiming();

  soft_navigation_metrics->largest_contentful_paint->largest_image_paint_size =
      1;

  observer.ExpectSoftNavigationMetrics(*soft_navigation_metrics);

  observer.DidChangePerformanceTiming();

  observer.GetMockTimer()->Fire();

  observer.VerifyExpectedSoftNavigationMetrics();

  ASSERT_FALSE(observer.GetMockTimer()->IsRunning());
}

TEST_F(MetricsRenderFrameObserverTest, MultipleNavigations) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);
  base::TimeDelta dom_event = base::Milliseconds(2);
  base::TimeDelta load_event = base::Milliseconds(2);

  TestMetricsRenderFrameObserver observer;

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics();
  observer.DidStartNavigation(GURL(), std::nullopt);
  observer.ReadyToCommitNavigation(nullptr);
  observer.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer.GetMockTimer()->Fire();

  timing.document_timing->dom_content_loaded_event_start = dom_event;
  timing.document_timing->load_event_start = load_event;
  observer.ExpectPageLoadTiming(timing);
  observer.ExpectSoftNavigationMetrics();
  observer.DidChangePerformanceTiming();
  observer.GetMockTimer()->Fire();

  // At this point, we should have triggered the generation of two metrics.
  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  observer.VerifyExpectedTimings();

  base::Time nav_start_2 = base::Time::FromSecondsSinceUnixEpoch(100);
  base::TimeDelta dom_event_2 = base::Milliseconds(20);
  base::TimeDelta load_event_2 = base::Milliseconds(20);
  mojom::PageLoadTiming timing_2;
  page_load_metrics::InitPageLoadTimingForTest(&timing_2);
  timing_2.navigation_start = nav_start_2;

  observer.SetMockTimer(nullptr);

  observer.ExpectPageLoadTiming(timing_2);
  observer.ExpectSoftNavigationMetrics();
  observer.DidStartNavigation(GURL(), std::nullopt);
  observer.ReadyToCommitNavigation(nullptr);
  observer.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer.GetMockTimer()->Fire();

  timing_2.document_timing->dom_content_loaded_event_start = dom_event_2;
  timing_2.document_timing->load_event_start = load_event_2;
  observer.ExpectPageLoadTiming(timing_2);
  observer.ExpectSoftNavigationMetrics();

  observer.DidChangePerformanceTiming();
  observer.GetMockTimer()->Fire();
}

}  // namespace page_load_metrics
