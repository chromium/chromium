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
// several virtual methods stubbed out to make the rest of the class more
// testable.
class TestMetricsRenderFrameObserver : public MetricsRenderFrameObserver,
                                       public test::WeakMockTimerProvider {
 public:
  explicit TestMetricsRenderFrameObserver(
      FakePageTimingSender::PageTimingValidator* validator)
      : MetricsRenderFrameObserver(nullptr), validator_(validator) {}

  std::unique_ptr<base::OneShotTimer> CreateTimer() override {
    auto timer = std::make_unique<test::WeakMockTimer>();
    SetMockTimer(timer->AsWeakPtr());
    return std::move(timer);
  }

  std::unique_ptr<PageTimingSender> CreatePageTimingSender(
      bool limited_sending_mode) override {
    return base::WrapUnique<PageTimingSender>(
        new FakePageTimingSender(validator_));
  }

  double GetNavigationStart() const override {
    EXPECT_NE(nullptr, fake_timing_.get());
    return fake_timing_->navigation_start.InSecondsFSinceUnixEpoch();
  }

  Timing GetTiming() const override {
    EXPECT_NE(nullptr, fake_timing_.get());
    return Timing(fake_timing_.Clone(),
                  PageTimingMetadataRecorder::MonotonicTiming());
  }

  mojom::CustomUserTimingMarkPtr GetCustomUserTimingMark() const override {
    return mojom::CustomUserTimingMark::New("fake_user_timing_mark",
                                            base::Milliseconds(100));
  }

  bool HasNoRenderFrame() const override { return false; }

  bool IsMainFrame() const override { return true; }

  void SetFakePageLoadTiming(const mojom::PageLoadTiming& timing) {
    fake_timing_ = timing.Clone();
  }

 private:
  mojom::PageLoadTimingPtr fake_timing_;
  raw_ptr<FakePageTimingSender::PageTimingValidator> validator_;
};

class MetricsRenderFrameObserverTest : public testing::Test {
 public:
  MetricsRenderFrameObserverTest() : observer_(&validator_) {}

 protected:
  TestMetricsRenderFrameObserver observer_;
  FakePageTimingSender::PageTimingValidator validator_;
};

TEST_F(MetricsRenderFrameObserverTest, NoMetrics) {
  observer_.DidChangePerformanceTiming();
  ASSERT_EQ(nullptr, observer_.GetMockTimer());
}

TEST_F(MetricsRenderFrameObserverTest, SingleMetric) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  auto empty_softnav = mojom::SoftNavigationMetrics::New();
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);

  observer_.DidStartNavigation(GURL(), std::nullopt);
  observer_.ReadyToCommitNavigation(nullptr);
  observer_.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer_.GetMockTimer()->Fire();

  timing.parse_timing->parse_start = base::Milliseconds(10);
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);

  observer_.DidChangePerformanceTiming();
  observer_.GetMockTimer()->Fire();
}

TEST_F(MetricsRenderFrameObserverTest,
       MainFrameIntersectionUpdateBeforeMetricsSenderCreated) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);

  observer_.OnMainFrameIntersectionChanged(gfx::Rect(1, 2, 3, 4));

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  auto empty_softnav = mojom::SoftNavigationMetrics::New();
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);
  observer_.DidStartNavigation(GURL(), std::nullopt);
  observer_.ReadyToCommitNavigation(nullptr);
  observer_.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  validator_.UpdateExpectedMainFrameIntersectionRect(gfx::Rect(1, 2, 3, 4));

  observer_.GetMockTimer()->Fire();
}

// Verify that when two CpuTimings come in, they're grouped into a single
// Message with the total being the sum of the two.
TEST_F(MetricsRenderFrameObserverTest, SingleCpuMetric) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);
  mojom::PageLoadTiming timing;

  // Initialize the page and add the initial timing info to the expected.
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  auto empty_softnav = mojom::SoftNavigationMetrics::New();
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);
  observer_.DidStartNavigation(GURL(), std::nullopt);
  observer_.ReadyToCommitNavigation(nullptr);
  observer_.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);

  // Send cpu timing updates and verify the expected result.
  observer_.DidChangeCpuTiming(base::Milliseconds(110));
  observer_.DidChangeCpuTiming(base::Milliseconds(50));
  validator_.ExpectCpuTiming(base::Milliseconds(160));
  observer_.GetMockTimer()->Fire();
}

TEST_F(MetricsRenderFrameObserverTest, MultipleMetricsAndSoftNavigations) {
  //
  // Navigation start: 42 seconds after the Unix epoch.
  //
  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(42);
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  auto soft_navigation_metrics = mojom::SoftNavigationMetrics::New();
  validator_.ExpectSoftNavigationMetrics(*soft_navigation_metrics);
  validator_.ExpectSoftLargestContentfulPaint(
      *CreateLargestContentfulPaintTiming());

  observer_.DidStartNavigation(GURL(), std::nullopt);
  observer_.ReadyToCommitNavigation(nullptr);
  observer_.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer_.GetMockTimer()->Fire();

  //
  // DOM content loaded: 100 milliseconds after navigation start.
  //
  timing.document_timing->dom_content_loaded_event_start =
      base::Milliseconds(100);
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(*soft_navigation_metrics);
  validator_.ExpectSoftLargestContentfulPaint(
      *CreateLargestContentfulPaintTiming());

  observer_.DidChangePerformanceTiming();
  observer_.GetMockTimer()->Fire();
  validator_.VerifyExpectedTimings();

  //
  // LOAD: 200 milliseconds after navigation start.
  // First soft navigation: 221.1 milliseconds after navigation start.
  //
  timing.document_timing->load_event_start = base::Milliseconds(200);
  observer_.SetFakePageLoadTiming(timing);
  observer_.DidChangePerformanceTiming();
  validator_.ExpectPageLoadTiming(timing);

  soft_navigation_metrics->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigation_metrics->count = 1;
  soft_navigation_metrics->start_time = base::Milliseconds(221.1);
  validator_.ExpectSoftNavigationMetrics(*soft_navigation_metrics);
  validator_.ExpectSoftLargestContentfulPaint(
      *CreateLargestContentfulPaintTiming());

  observer_.DidObserveSoftNavigation(blink::SoftNavigationMetricsForReporting{
      .count = soft_navigation_metrics->count,
      .start_time = timing.navigation_start - base::Time::UnixEpoch() +
                    soft_navigation_metrics->start_time,
      .same_document_metrics_token =
          *soft_navigation_metrics->same_document_metrics_token,
  });

  observer_.GetMockTimer()->Fire();
  validator_.VerifyExpectedTimings();
  validator_.VerifyExpectedSoftNavigationMetrics();
  ASSERT_FALSE(observer_.GetMockTimer()->IsRunning());

  //
  // Soft LCP: 120 milliseconds after soft navigation start.
  //
  auto soft_largest_contentful_paint = CreateLargestContentfulPaintTiming();
  base::TimeDelta soft_lcp = base::Milliseconds(120);
  soft_largest_contentful_paint->largest_image_paint =
      soft_lcp + soft_navigation_metrics->start_time;
  soft_largest_contentful_paint->largest_image_paint_size = 2500;

  validator_.ExpectSoftNavigationMetrics(*soft_navigation_metrics);
  validator_.ExpectSoftLargestContentfulPaint(*soft_largest_contentful_paint);
  observer_.DidObserveSoftLargestContentfulPaint(
      blink::LargestContentfulPaintDetailsForReporting{
          .image_paint_time =
              (timing.navigation_start - base::Time::UnixEpoch() +
               soft_navigation_metrics->start_time + soft_lcp)
                  .InSecondsF(),
          .image_paint_size =
              soft_largest_contentful_paint->largest_image_paint_size});
  validator_.ExpectPageLoadTiming(timing);
  observer_.GetMockTimer()->Fire();
  validator_.VerifyExpectedTimings();
  validator_.VerifyExpectedSoftNavigationMetrics();
  ASSERT_FALSE(observer_.GetMockTimer()->IsRunning());
  //
  // Second soft navigation: 4020.71 milliseconds after navigation start
  //
  // This page load timing is the same as the previous one, but as the soft
  // navigation metric is being sent, this timing is also sent along with the
  // soft navigation metric. Therefore we should expect 1 more page load timing.
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);

  soft_navigation_metrics = mojom::SoftNavigationMetrics::New();

  soft_navigation_metrics->same_document_metrics_token =
      base::UnguessableToken::Create();
  soft_navigation_metrics->count = 2;
  soft_navigation_metrics->start_time = base::Milliseconds(4020.71);

  observer_.DidObserveSoftNavigation(blink::SoftNavigationMetricsForReporting{
      .count = soft_navigation_metrics->count,
      .start_time = timing.navigation_start - base::Time::UnixEpoch() +
                    soft_navigation_metrics->start_time,
      .same_document_metrics_token =
          *soft_navigation_metrics->same_document_metrics_token,
  });

  validator_.ExpectSoftNavigationMetrics(*soft_navigation_metrics);
  validator_.ExpectSoftLargestContentfulPaint(
      *CreateLargestContentfulPaintTiming());

  observer_.GetMockTimer()->Fire();

  validator_.VerifyExpectedSoftNavigationMetrics();
  validator_.VerifyExpectedSoftLargestContentfulPaint();

  ASSERT_FALSE(observer_.GetMockTimer()->IsRunning());
}

TEST_F(MetricsRenderFrameObserverTest, MultipleNavigations) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);
  base::TimeDelta dom_event = base::Milliseconds(2);
  base::TimeDelta load_event = base::Milliseconds(2);

  mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  mojom::SoftNavigationMetricsPtr empty_softnav =
      mojom::SoftNavigationMetrics::New();
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);
  observer_.DidStartNavigation(GURL(), std::nullopt);
  observer_.ReadyToCommitNavigation(nullptr);
  observer_.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer_.GetMockTimer()->Fire();

  timing.document_timing->dom_content_loaded_event_start = dom_event;
  timing.document_timing->load_event_start = load_event;
  observer_.SetFakePageLoadTiming(timing);
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);
  observer_.DidChangePerformanceTiming();
  observer_.GetMockTimer()->Fire();

  // At this point, we should have triggered the generation of two metrics.
  // Verify and reset the observer's expectations before moving on to the next
  // part of the test.
  validator_.VerifyExpectedTimings();

  base::Time nav_start_2 = base::Time::FromSecondsSinceUnixEpoch(100);
  base::TimeDelta dom_event_2 = base::Milliseconds(20);
  base::TimeDelta load_event_2 = base::Milliseconds(20);
  mojom::PageLoadTiming timing_2;
  page_load_metrics::InitPageLoadTimingForTest(&timing_2);
  timing_2.navigation_start = nav_start_2;

  observer_.SetMockTimer(nullptr);

  observer_.SetFakePageLoadTiming(timing_2);
  validator_.ExpectPageLoadTiming(timing_2);
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);
  observer_.DidStartNavigation(GURL(), std::nullopt);
  observer_.ReadyToCommitNavigation(nullptr);
  observer_.DidCommitProvisionalLoad(ui::PAGE_TRANSITION_LINK);
  observer_.GetMockTimer()->Fire();

  timing_2.document_timing->dom_content_loaded_event_start = dom_event_2;
  timing_2.document_timing->load_event_start = load_event_2;
  observer_.SetFakePageLoadTiming(timing_2);
  validator_.ExpectPageLoadTiming(timing_2);
  validator_.ExpectSoftNavigationMetrics(*empty_softnav);

  observer_.DidChangePerformanceTiming();
  observer_.GetMockTimer()->Fire();
}

}  // namespace page_load_metrics
