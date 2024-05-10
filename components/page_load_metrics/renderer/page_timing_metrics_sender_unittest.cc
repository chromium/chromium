// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"

#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/renderer/fake_page_timing_sender.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/responsiveness_metrics/user_interaction_latency.h"
#include "third_party/blink/public/common/subresource_load_metrics.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-shared.h"

using CSSSampleId = blink::mojom::CSSSampleId;

namespace page_load_metrics {

// Thin wrapper around PageTimingMetricsSender that provides access to the
// MockOneShotTimer instance.
class TestPageTimingMetricsSender : public PageTimingMetricsSender {
 public:
  explicit TestPageTimingMetricsSender(
      std::unique_ptr<PageTimingSender> page_timing_sender,
      mojom::PageLoadTimingPtr initial_timing,
      const PageTimingMetadataRecorder::MonotonicTiming& monotonic_timing)
      : PageTimingMetricsSender(std::move(page_timing_sender),
                                std::make_unique<base::MockOneShotTimer>(),
                                std::move(initial_timing),
                                monotonic_timing,
                                /* initial_request=*/nullptr,
                                /*is_main_frame=*/true) {}

  base::MockOneShotTimer* mock_timer() const {
    return static_cast<base::MockOneShotTimer*>(timer());
  }
};

class PageTimingMetricsSenderTest : public testing::Test {
 public:
  PageTimingMetricsSenderTest()
      : metrics_sender_(new TestPageTimingMetricsSender(
            std::make_unique<FakePageTimingSender>(&validator_),
            CreatePageLoadTiming(),
            PageTimingMetadataRecorder::MonotonicTiming())) {}

  mojom::SoftNavigationMetrics CreateEmptySoftNavigationMetrics() {
    return mojom::SoftNavigationMetrics(blink::kSoftNavigationCountDefaultValue,
                                        base::Milliseconds(0), std::string(),
                                        CreateLargestContentfulPaintTiming());
  }

 protected:
  FakePageTimingSender::PageTimingValidator validator_;
  std::unique_ptr<TestPageTimingMetricsSender> metrics_sender_;
};

TEST_F(PageTimingMetricsSenderTest, Basic) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());

  // Firing the timer should trigger sending of an SendTiming call.
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());

  // At this point, we should have triggered the send of the SendTiming call.
  validator_.VerifyExpectedTimings();

  // Attempt to send the same timing instance again. The send should be
  // suppressed, since the timing instance hasn't changed since the last send.
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, CoalesceMultipleTimings) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);
  base::TimeDelta load_event = base::Milliseconds(4);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());

  // Send an updated PageLoadTiming before the timer has fired. When the timer
  // fires, the updated PageLoadTiming should be sent.
  timing.document_timing->load_event_start = load_event;
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());

  // Firing the timer should trigger sending of the SendTiming call with
  // the most recently provided PageLoadTiming instance.
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, MultipleTimings) {
  base::Time nav_start = base::Time::FromSecondsSinceUnixEpoch(10);
  base::TimeDelta load_event = base::Milliseconds(4);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
  validator_.VerifyExpectedTimings();

  // Send an updated PageLoadTiming after the timer for the first send request
  // has fired, and verify that a second timing is sent.
  timing.document_timing->load_event_start = load_event;
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, SendTimingOnSendLatest) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(10);

  // This test wants to verify behavior in the PageTimingMetricsSender
  // destructor. The EXPECT_CALL will be satisfied when the |metrics_sender_|
  // is destroyed below.
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());

  metrics_sender_->SendLatest();
}

TEST_F(PageTimingMetricsSenderTest, SendSubresourceLoadMetrics) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  blink::SubresourceLoadMetrics metrics{
      .number_of_subresources_loaded = 5,
      .number_of_subresource_loads_handled_by_service_worker = 2,
      .service_worker_subresource_load_metrics =
          blink::ServiceWorkerSubresourceLoadMetrics{
              .mock_handled = true,
              .mock_fallback = true,
          },
  };
  metrics_sender_->DidObserveSubresourceLoad(metrics);
  validator_.UpdateExpectedSubresourceLoadMetrics(metrics);
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedSubresourceLoadMetrics();
}

TEST_F(PageTimingMetricsSenderTest, SendSingleFeature) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  blink::UseCounterFeature feature = {
      blink::mojom::UseCounterFeatureType::kWebFeature, 0};

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  // Observe a single feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature);
  validator_.UpdateExpectPageLoadFeatures(feature);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedFeatures();
}

TEST_F(PageTimingMetricsSenderTest, SendMultipleFeatures) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  blink::UseCounterFeature feature_0 = {
      blink::mojom::UseCounterFeatureType::kWebFeature, 0};
  blink::UseCounterFeature feature_1 = {
      blink::mojom::UseCounterFeatureType::kCssProperty, 1};
  blink::UseCounterFeature feature_2 = {
      blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2};
  blink::UseCounterFeature feature_3 = {
      blink::mojom::UseCounterFeatureType::kWebDXFeature, 3};

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  // Observe the first feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_0);
  validator_.UpdateExpectPageLoadFeatures(feature_0);
  // Observe the second feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_1);
  validator_.UpdateExpectPageLoadFeatures(feature_1);
  // Observe the third feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_2);
  validator_.UpdateExpectPageLoadFeatures(feature_2);
  // Observe the fourth feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_3);
  validator_.UpdateExpectPageLoadFeatures(feature_3);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedFeatures();
}

TEST_F(PageTimingMetricsSenderTest, SendDuplicatedFeatures) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  blink::UseCounterFeature feature = {
      blink::mojom::UseCounterFeatureType::kWebFeature, 0};

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  metrics_sender_->DidObserveNewFeatureUsage(feature);
  validator_.UpdateExpectPageLoadFeatures(feature);
  // Observe a duplicated feature usage, without updating expected features sent
  // across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedFeatures();
}

TEST_F(PageTimingMetricsSenderTest, SendMultipleFeaturesTwice) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  blink::UseCounterFeature feature_0 = {
      blink::mojom::UseCounterFeatureType::kWebFeature, 0};
  blink::UseCounterFeature feature_1 = {
      blink::mojom::UseCounterFeatureType::kCssProperty, 1};
  blink::UseCounterFeature feature_2 = {
      blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2};
  blink::UseCounterFeature feature_3 = {
      blink::mojom::UseCounterFeatureType::kWebDXFeature, 3};

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  // Observe the first feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_0);
  validator_.UpdateExpectPageLoadFeatures(feature_0);
  // Observe the second feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_1);
  validator_.UpdateExpectPageLoadFeatures(feature_1);
  // Observe a duplicated feature usage, without updating expected features sent
  // across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_0);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedFeatures();

  base::TimeDelta load_event = base::Milliseconds(4);
  // Send an updated PageLoadTiming after the timer for the first send request
  // has fired, and verify that a second list of features is sent.
  timing.document_timing->load_event_start = load_event;
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());
  // Observe duplicated feature usage, without updating expected features sent
  // across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_0);
  metrics_sender_->DidObserveNewFeatureUsage(feature_1);
  // Observe an additional feature usage, update expected features sent across
  // IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_2);
  validator_.UpdateExpectPageLoadFeatures(feature_2);
  metrics_sender_->DidObserveNewFeatureUsage(feature_3);
  validator_.UpdateExpectPageLoadFeatures(feature_3);
  // Fire the timer to trigger another sending of features via the second
  // SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedFeatures();
}

TEST_F(PageTimingMetricsSenderTest, SendPageRenderData) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);

  // We need to send the PageLoadTiming here even though it is not really
  // related to the PageRenderData.  This is because metrics_sender_ sends
  // its last_timing_ when the mock timer fires, causing the validator to
  // look for a matching expectation.
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  metrics_sender_->DidObserveLayoutShift(0.5, false);
  metrics_sender_->DidObserveLayoutShift(0.5, false);
  metrics_sender_->DidObserveLayoutShift(0.5, true);

  mojom::FrameRenderDataUpdate render_data(1.5, 1.0, {});
  validator_.UpdateExpectFrameRenderDataUpdate(render_data);

  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedRenderData();
}

TEST_F(PageTimingMetricsSenderTest, SendMainFrameIntersectionRect) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  metrics_sender_->OnMainFrameIntersectionChanged(gfx::Rect(0, 0, 1, 1));
  validator_.UpdateExpectedMainFrameIntersectionRect(gfx::Rect(0, 0, 1, 1));

  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedMainFrameIntersectionRect();
}

TEST_F(PageTimingMetricsSenderTest, SendMainFrameViewportRect) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  metrics_sender_->OnMainFrameViewportRectangleChanged(gfx::Rect(2, 2, 1, 1));
  validator_.UpdateExpectedMainFrameViewportRect(gfx::Rect(2, 2, 1, 1));

  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedMainFrameViewportRect();
}

TEST_F(PageTimingMetricsSenderTest, SendInteractions) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  base::TimeDelta interaction_duration_1 = base::Milliseconds(90);

  base::TimeTicks interaction_start_1 = base::TimeTicks::Now();
  base::TimeTicks interaction_end_1 =
      interaction_start_1 + interaction_duration_1;
  base::TimeDelta interaction_duration_2 = base::Milliseconds(600);
  base::TimeTicks interaction_start_2 =
      base::TimeTicks::Now() + base::Milliseconds(2000);
  base::TimeTicks interaction_end_2 =
      interaction_start_2 + interaction_duration_2;

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  // max_event_queued and max_event_commit_finish is irrelevant to this test.
  metrics_sender_->DidObserveUserInteraction(
      interaction_start_1, base::TimeTicks(), base::TimeTicks(),
      interaction_end_1, blink::UserInteractionType::kKeyboard, 0);
  validator_.UpdateExpectedInteractionTiming(
      interaction_duration_1, mojom::UserInteractionType::kKeyboard, 0,
      interaction_start_1);

  // max_event_queued and max_event_commit_finish is irrelevant to this test.
  metrics_sender_->DidObserveUserInteraction(
      interaction_start_2, base::TimeTicks(), base::TimeTicks(),
      interaction_end_2, blink::UserInteractionType::kTapOrClick, 1);
  validator_.UpdateExpectedInteractionTiming(
      interaction_duration_2, mojom::UserInteractionType::kTapOrClick, 1,
      interaction_start_2);

  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedInteractionTiming();
}

TEST_F(PageTimingMetricsSenderTest, FirstContentfulPaintForcesSend) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  validator_.ExpectPageLoadTiming(timing);
  validator_.ExpectSoftNavigationMetrics(CreateEmptySoftNavigationMetrics());

  // Updating when |timing| has FCP will cause the metrics to be sent urgently.
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  EXPECT_EQ(metrics_sender_->mock_timer()->GetCurrentDelay(),
            base::Milliseconds(0));
  metrics_sender_->mock_timer()->Fire();
}

}  // namespace page_load_metrics
