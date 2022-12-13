// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"

#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/renderer/fake_page_timing_sender.h"
#include "testing/gtest/include/gtest/gtest.h"
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
                                std::make_unique<PageResourceDataUse>()) {}

  base::MockOneShotTimer* mock_timer() const {
    return static_cast<base::MockOneShotTimer*>(timer());
  }
};

class PageTimingMetricsSenderTest : public testing::Test {
 public:
  PageTimingMetricsSenderTest()
      : metrics_sender_(new TestPageTimingMetricsSender(
            std::make_unique<FakePageTimingSender>(&validator_),
            mojom::PageLoadTiming::New(),
            PageTimingMetadataRecorder::MonotonicTiming())) {}

 protected:
  FakePageTimingSender::PageTimingValidator validator_;
  std::unique_ptr<TestPageTimingMetricsSender> metrics_sender_;
};

TEST_F(PageTimingMetricsSenderTest, Basic) {
  base::Time nav_start = base::Time::FromDoubleT(10);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());

  // Firing the timer should trigger sending of an SendTiming call.
  validator_.ExpectPageLoadTiming(timing);
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
  base::Time nav_start = base::Time::FromDoubleT(10);
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
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, MultipleTimings) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta load_event = base::Milliseconds(4);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());
  validator_.ExpectPageLoadTiming(timing);
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
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, SendTimingOnSendLatest) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(10);

  // This test wants to verify behavior in the PageTimingMetricsSender
  // destructor. The EXPECT_CALL will be satisfied when the |metrics_sender_|
  // is destroyed below.
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());

  metrics_sender_->SendLatest();
}

TEST_F(PageTimingMetricsSenderTest, SendInputEvents) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  base::TimeDelta input_delay_1 = base::Milliseconds(40);
  base::TimeDelta input_delay_2 = base::Milliseconds(60);

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);

  metrics_sender_->DidObserveInputDelay(input_delay_1);
  validator_.UpdateExpectedInputTiming(input_delay_1);

  metrics_sender_->DidObserveInputDelay(input_delay_2);
  validator_.UpdateExpectedInputTiming(input_delay_2);

  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedInputTiming();
}

TEST_F(PageTimingMetricsSenderTest, SendSubresourceLoadMetrics) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);

  metrics_sender_->DidObserveSubresourceLoad(5, 2);

  mojom::SubresourceLoadMetricsPtr expected =
      mojom::SubresourceLoadMetrics::New();
  expected->number_of_subresources_loaded = 5;
  expected->number_of_subresource_loads_handled_by_service_worker = 2;
  validator_.UpdateExpectedSubresourceLoadMetrics(*expected);
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

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
  // Observe the first feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_0);
  validator_.UpdateExpectPageLoadFeatures(feature_0);
  // Observe the second feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_1);
  validator_.UpdateExpectPageLoadFeatures(feature_1);
  // Observe the third feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_2);
  validator_.UpdateExpectPageLoadFeatures(feature_2);
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

  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  validator_.ExpectPageLoadTiming(timing);
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
  // Observe duplicated feature usage, without updating expected features sent
  // across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_0);
  metrics_sender_->DidObserveNewFeatureUsage(feature_1);
  // Observe an additional feature usage, update expected features sent across
  // IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_2);
  validator_.UpdateExpectPageLoadFeatures(feature_2);
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

  metrics_sender_->OnMainFrameViewportRectangleChanged(gfx::Rect(2, 2, 1, 1));
  validator_.UpdateExpectedMainFrameViewportRect(gfx::Rect(2, 2, 1, 1));

  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedMainFrameViewportRect();
}

TEST_F(PageTimingMetricsSenderTest, FirstContentfulPaintForcesSend) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.paint_timing->first_contentful_paint = base::Seconds(1);
  validator_.ExpectPageLoadTiming(timing);

  // Updating when |timing| has FCP will cause the metrics to be sent urgently.
  metrics_sender_->Update(timing.Clone(),
                          PageTimingMetadataRecorder::MonotonicTiming());
  EXPECT_EQ(metrics_sender_->mock_timer()->GetCurrentDelay(),
            base::Milliseconds(0));
  metrics_sender_->mock_timer()->Fire();
}

}  // namespace page_load_metrics
