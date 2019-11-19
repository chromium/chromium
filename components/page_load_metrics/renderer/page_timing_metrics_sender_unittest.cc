// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/renderer/page_timing_metrics_sender.h"

#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/renderer/fake_page_timing_sender.h"
#include "testing/gtest/include/gtest/gtest.h"

using CSSSampleId = blink::mojom::CSSSampleId;

namespace page_load_metrics {

// Thin wrapper around PageTimingMetricsSender that provides access to the
// MockOneShotTimer instance.
class TestPageTimingMetricsSender : public PageTimingMetricsSender {
 public:
  explicit TestPageTimingMetricsSender(
      std::unique_ptr<PageTimingSender> page_timing_sender,
      mojom::PageLoadTimingPtr initial_timing)
      : PageTimingMetricsSender(std::move(page_timing_sender),
                                std::make_unique<base::MockOneShotTimer>(),
                                std::move(initial_timing),
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
            mojom::PageLoadTiming::New())) {}

 protected:
  FakePageTimingSender::PageTimingValidator validator_;
  std::unique_ptr<TestPageTimingMetricsSender> metrics_sender_;
};

TEST_F(PageTimingMetricsSenderTest, Basic) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  timing.document_timing->first_layout = first_layout;

  metrics_sender_->SendSoon(timing.Clone());

  // Firing the timer should trigger sending of an SendTiming call.
  validator_.ExpectPageLoadTiming(timing);
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());

  // At this point, we should have triggered the send of the SendTiming call.
  validator_.VerifyExpectedTimings();

  // Attempt to send the same timing instance again. The send should be
  // suppressed, since the timing instance hasn't changed since the last send.
  metrics_sender_->SendSoon(timing.Clone());
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, CoalesceMultipleTimings) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(4);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  timing.document_timing->first_layout = first_layout;

  metrics_sender_->SendSoon(timing.Clone());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());

  // Send an updated PageLoadTiming before the timer has fired. When the timer
  // fires, the updated PageLoadTiming should be sent.
  timing.document_timing->load_event_start = load_event;
  metrics_sender_->SendSoon(timing.Clone());

  // Firing the timer should trigger sending of the SendTiming call with
  // the most recently provided PageLoadTiming instance.
  validator_.ExpectPageLoadTiming(timing);
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, MultipleTimings) {
  base::Time nav_start = base::Time::FromDoubleT(10);
  base::TimeDelta first_layout = base::TimeDelta::FromMillisecondsD(2);
  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(4);

  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = nav_start;
  timing.document_timing->first_layout = first_layout;

  metrics_sender_->SendSoon(timing.Clone());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());
  validator_.ExpectPageLoadTiming(timing);
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
  validator_.VerifyExpectedTimings();

  // Send an updated PageLoadTiming after the timer for the first send request
  // has fired, and verify that a second timing is sent.
  timing.document_timing->load_event_start = load_event;
  metrics_sender_->SendSoon(timing.Clone());
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());
  validator_.ExpectPageLoadTiming(timing);
  metrics_sender_->mock_timer()->Fire();
  EXPECT_FALSE(metrics_sender_->mock_timer()->IsRunning());
}

TEST_F(PageTimingMetricsSenderTest, SendTimingOnSendLatest) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromDoubleT(10);
  timing.document_timing->first_layout = base::TimeDelta::FromMilliseconds(10);

  // This test wants to verify behavior in the PageTimingMetricsSender
  // destructor. The EXPECT_CALL will be satisfied when the |metrics_sender_|
  // is destroyed below.
  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  ASSERT_TRUE(metrics_sender_->mock_timer()->IsRunning());

  metrics_sender_->SendLatest();
}

TEST_F(PageTimingMetricsSenderTest, SendSingleFeature) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  blink::mojom::WebFeature feature = blink::mojom::WebFeature::kFetch;

  metrics_sender_->SendSoon(timing.Clone());
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
  blink::mojom::WebFeature feature_0 = blink::mojom::WebFeature::kFetch;
  blink::mojom::WebFeature feature_1 =
      blink::mojom::WebFeature::kFetchBodyStream;

  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  // Observe the first feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_0);
  validator_.UpdateExpectPageLoadFeatures(feature_0);
  // Observe the second feature, update expected features sent across IPC.
  metrics_sender_->DidObserveNewFeatureUsage(feature_1);
  validator_.UpdateExpectPageLoadFeatures(feature_1);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedFeatures();
}

TEST_F(PageTimingMetricsSenderTest, SendDuplicatedFeatures) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);
  blink::mojom::WebFeature feature = blink::mojom::WebFeature::kFetch;

  metrics_sender_->SendSoon(timing.Clone());
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
  blink::mojom::WebFeature feature_0 = blink::mojom::WebFeature::kFetch;
  blink::mojom::WebFeature feature_1 =
      blink::mojom::WebFeature::kFetchBodyStream;
  blink::mojom::WebFeature feature_2 = blink::mojom::WebFeature::kWindowFind;

  metrics_sender_->SendSoon(timing.Clone());
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

  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(4);
  // Send an updated PageLoadTiming after the timer for the first send request
  // has fired, and verify that a second list of features is sent.
  timing.document_timing->load_event_start = load_event;
  metrics_sender_->SendSoon(timing.Clone());
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

TEST_F(PageTimingMetricsSenderTest, SendSingleCssProperty) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);

  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  // Observe a single CSS property, update expected CSS properties sent across
  // IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kDirection,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kDirection);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedCssProperties();
}

TEST_F(PageTimingMetricsSenderTest, SendCssPropertiesInRange) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);

  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  // Observe the smallest CSS property ID.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kColor,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kColor);
  // Observe the largest CSS property ID.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kMaxValue,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kMaxValue);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedCssProperties();
}

TEST_F(PageTimingMetricsSenderTest, SendMultipleCssProperties) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);

  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  // Observe the first CSS property, update expected CSS properties sent across
  // IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kDirection,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kDirection);
  // Observe the second CSS property, update expected CSS properties sent across
  // IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kBorderLeftWidth,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kBorderLeftWidth);
  // Fire the timer to trigger sending of CSS properties via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedCssProperties();
}

TEST_F(PageTimingMetricsSenderTest, SendDuplicatedCssProperties) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);

  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kDirection,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kDirection);
  // Observe a duplicated CSS property usage, without updating expected CSS
  // properties sent across IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kDirection,
                                                 false /*is_animated*/);
  // Fire the timer to trigger sending of CSS properties via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedCssProperties();
}

TEST_F(PageTimingMetricsSenderTest, SendMultipleCssPropertiesTwice) {
  mojom::PageLoadTiming timing;
  InitPageLoadTimingForTest(&timing);

  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  // Observe the first CSS property, update expected CSS properties sent across
  // IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kColor,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kColor);
  // Observe the second CSS property, update expected CSS properties sent across
  // IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kFont,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kFont);
  // Observe a duplicated usage, without updating expected CSS properties sent
  // across IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kColor,
                                                 false /*is_animated*/);
  // Fire the timer to trigger sending of features via an SendTiming call.
  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedFeatures();

  base::TimeDelta load_event = base::TimeDelta::FromMillisecondsD(4);
  // Send an updated PageLoadTiming after the timer for the first send request
  // has fired, and verify that a second list of CSS properties is sent.
  timing.document_timing->load_event_start = load_event;
  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);
  // Observe duplicated usage, without updating expected features sent across
  // IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kDirection,
                                                 false /*is_animated*/);
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kColor,
                                                 false /*is_animated*/);
  // Observe an additional usage, update expected features sent across IPC.
  metrics_sender_->DidObserveNewCssPropertyUsage(CSSSampleId::kDirection,
                                                 false /*is_animated*/);
  validator_.UpdateExpectPageLoadCssProperties(CSSSampleId::kDirection);
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
  metrics_sender_->SendSoon(timing.Clone());
  validator_.ExpectPageLoadTiming(timing);

  metrics_sender_->DidObserveLayoutShift(0.5, false);
  metrics_sender_->DidObserveLayoutShift(0.5, false);
  metrics_sender_->DidObserveLayoutShift(0.5, true);

  mojom::FrameRenderDataUpdate render_data(1.5, 1.0);
  validator_.UpdateExpectFrameRenderDataUpdate(render_data);

  metrics_sender_->mock_timer()->Fire();
  validator_.VerifyExpectedRenderData();
}

}  // namespace page_load_metrics
