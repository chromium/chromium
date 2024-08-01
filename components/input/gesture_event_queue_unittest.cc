// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/input/gesture_event_queue.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/input/touchpad_tap_suppression_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/events/blink/blink_features.h"

using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace input {

class GestureEventQueueTest : public testing::Test,
                              public GestureEventQueueClient,
                              public FlingControllerEventSenderClient,
                              public FlingControllerSchedulerClient {
 public:
  GestureEventQueueTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME,
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
        acked_gesture_event_count_(0),
        sent_gesture_event_count_(0) {}

  ~GestureEventQueueTest() override {}

  // testing::Test
  void SetUp() override {
    queue_ =
        std::make_unique<GestureEventQueue>(this, this, this, DefaultConfig());
  }

  void TearDown() override {
    // Process all pending tasks to avoid leaks.
    RunUntilIdle();
    queue_.reset();
  }

  void SetUpForTapSuppression(int max_cancel_to_down_time_ms) {
    GestureEventQueue::Config gesture_config;
    gesture_config.fling_config.touchscreen_tap_suppression_config.enabled =
        true;
    gesture_config.fling_config.touchscreen_tap_suppression_config
        .max_cancel_to_down_time =
        base::Milliseconds(max_cancel_to_down_time_ms);
    queue_ =
        std::make_unique<GestureEventQueue>(this, this, this, gesture_config);
  }

  // GestureEventQueueClient
  void SendGestureEventImmediately(
      const GestureEventWithLatencyInfo& event) override {
    ++sent_gesture_event_count_;
    if (sync_ack_result_) {
      std::unique_ptr<blink::mojom::InputEventResultState> ack_result =
          std::move(sync_ack_result_);
      SendInputEventACK(event.event.GetType(), *ack_result);
    }
  }

  void OnGestureEventAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override {
    ++acked_gesture_event_count_;
    last_acked_event_ = event.event;
    if (sync_followup_event_) {
      auto sync_followup_event = std::move(sync_followup_event_);
      SimulateGestureEvent(*sync_followup_event);
    }
  }

  // FlingControllerEventSenderClient
  void SendGeneratedWheelEvent(
      const MouseWheelEventWithLatencyInfo& wheel_event) override {}
  void SendGeneratedGestureScrollEvents(
      const GestureEventWithLatencyInfo& gesture_event) override {}
  gfx::Size GetRootWidgetViewportSize() override {
    return gfx::Size(1920, 1080);
  }

  // FlingControllerSchedulerClient
  void ScheduleFlingProgress(
      base::WeakPtr<FlingController> fling_controller) override {}
  void DidStopFlingingOnBrowser(
      base::WeakPtr<FlingController> fling_controller) override {}
  bool NeedsBeginFrameForFlingProgress() override { return false; }
  bool ShouldUseMobileFlingCurve() override {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return true;
#else
    return false;
#endif
  }
  gfx::Vector2dF GetPixelsPerInch(
      const gfx::PointF& position_in_screen) override {
    return gfx::Vector2dF(input::kDefaultPixelsPerInch,
                          input::kDefaultPixelsPerInch);
  }

 protected:
  static GestureEventQueue::Config DefaultConfig() {
    return GestureEventQueue::Config();
  }

  void SetUpForDebounce(int interval_ms) {
    queue()->set_debounce_interval_time_ms_for_testing(interval_ms);
  }

  void SimulateGestureEvent(const WebGestureEvent& gesture) {
    GestureEventWithLatencyInfo gesture_event(gesture);
    if (!queue()->PassToFlingController(gesture_event)) {
      queue()->DebounceOrForwardEvent(gesture_event);
    }
  }

  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    SimulateGestureEvent(
        blink::SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGSEGeneratedByFlingController(WebGestureDevice sourceDevice) {
    WebGestureEvent gesture_scroll_end =
        blink::SyntheticWebGestureEventBuilder::Build(
            WebInputEvent::Type::kGestureScrollEnd, sourceDevice);
    gesture_scroll_end.data.scroll_end.generated_by_fling_controller = true;
    SimulateGestureEvent(gesture_scroll_end);
  }

  void SimulateGestureScrollUpdateEvent(float dX, float dY, int modifiers) {
    SimulateGestureEvent(
        blink::SyntheticWebGestureEventBuilder::BuildScrollUpdate(
            dX, dY, modifiers, blink::WebGestureDevice::kTouchscreen));
  }

  void SimulateGesturePinchUpdateEvent(float scale,
                                       float anchorX,
                                       float anchorY,
                                       int modifiers) {
    SimulateGestureEvent(
        blink::SyntheticWebGestureEventBuilder::BuildPinchUpdate(
            scale, anchorX, anchorY, modifiers,
            blink::WebGestureDevice::kTouchscreen));
  }

  void SimulateGestureFlingStartEvent(float velocityX,
                                      float velocityY,
                                      WebGestureDevice sourceDevice) {
    SimulateGestureEvent(blink::SyntheticWebGestureEventBuilder::BuildFling(
        velocityX, velocityY, sourceDevice));
  }

  void SendInputEventACK(WebInputEvent::Type type,
                         blink::mojom::InputEventResultState ack) {
    queue()->ProcessGestureAck(
        blink::mojom::InputEventResultSource::kCompositorThread, ack, type,
        ui::LatencyInfo());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delay) {
    task_environment_.FastForwardBy(delay);
  }

  size_t GetAndResetSentGestureEventCount() {
    size_t count = sent_gesture_event_count_;
    sent_gesture_event_count_ = 0;
    return count;
  }

  size_t GetAndResetAckedGestureEventCount() {
    size_t count = acked_gesture_event_count_;
    acked_gesture_event_count_ = 0;
    return count;
  }

  const WebGestureEvent& last_acked_event() const { return last_acked_event_; }

  void set_synchronous_ack(blink::mojom::InputEventResultState ack_result) {
    sync_ack_result_ =
        std::make_unique<blink::mojom::InputEventResultState>(ack_result);
  }

  void set_sync_followup_event(WebInputEvent::Type type,
                               WebGestureDevice sourceDevice) {
    sync_followup_event_ = std::make_unique<WebGestureEvent>(
        blink::SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  unsigned GestureEventQueueSize() {
    return queue()->sent_events_awaiting_ack_.size();
  }

  WebGestureEvent GestureEventSecondFromLastQueueEvent() {
    return queue()
        ->sent_events_awaiting_ack_.at(GestureEventQueueSize() - 2)
        .event;
  }

  WebGestureEvent GestureEventLastQueueEvent() {
    return queue()->sent_events_awaiting_ack_.back().event;
  }

  unsigned GestureEventDebouncingQueueSize() {
    return queue()->debouncing_deferral_queue_.size();
  }

  WebGestureEvent GestureEventQueueEventAt(int i) {
    return queue()->sent_events_awaiting_ack_.at(i).event;
  }

  bool ScrollingInProgress() { return queue()->scrolling_in_progress_; }

  bool FlingInProgress() { return queue()->FlingInProgressForTest(); }

  GestureEventQueue* queue() const { return queue_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<GestureEventQueue> queue_;
  size_t acked_gesture_event_count_;
  size_t sent_gesture_event_count_;
  WebGestureEvent last_acked_event_;
  std::unique_ptr<blink::mojom::InputEventResultState> sync_ack_result_;
  std::unique_ptr<WebGestureEvent> sync_followup_event_;
  base::test::ScopedFeatureList feature_list_;
};

class GestureEventQueueWithCompositorEventQueueTest
    : public GestureEventQueueTest {};

// Tests a single event with an synchronous ack.
TEST_F(GestureEventQueueTest, SimpleSyncAck) {
  set_synchronous_ack(blink::mojom::InputEventResultState::kConsumed);
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Tests an event with an synchronous ack which enqueues an additional event.
TEST_F(GestureEventQueueTest, SyncAckQueuesEvent) {
  std::unique_ptr<WebGestureEvent> queued_event;
  set_synchronous_ack(blink::mojom::InputEventResultState::kConsumed);
  set_sync_followup_event(WebInputEvent::Type::kGestureShowPress,
                          blink::WebGestureDevice::kTouchscreen);
  // This event enqueues the show press event.
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());

  SendInputEventACK(WebInputEvent::Type::kGestureShowPress,
                    blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(1U, GetAndResetAckedGestureEventCount());
}

// Test that a GestureScrollEnd is deferred during the debounce interval,
// that Scrolls are not and that the deferred events are sent after that
// timer fires.
TEST_F(GestureEventQueueTest, DebounceDefersFollowingGestureEvents) {
  SetUpForDebounce(3);

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());

  FastForwardBy(base::Milliseconds(5));

  // The deferred events are correctly queued in coalescing queue.
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(4U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::Type::kGestureScrollUpdate,
                                    WebInputEvent::Type::kGestureScrollUpdate,
                                    WebInputEvent::Type::kGestureScrollEnd};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
       i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

// Tests that GSE events generated by the fling controller are forwarded to the
// renderer instead of getting pushed back to the deboucing_deferral_queue_. In
// this case the following GSB won't get deferred either.
TEST_F(GestureEventQueueTest,
       DebounceDoesNotDeferGSEsGeneratedByFlingController) {
  SetUpForDebounce(3);

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGSEGeneratedByFlingController(blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::Type::kGestureScrollUpdate,
                                    WebInputEvent::Type::kGestureScrollEnd,
                                    WebInputEvent::Type::kGestureScrollBegin};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
       i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

TEST_F(GestureEventQueueTest, DebounceDefersGSBIfPreviousGSEDeferred) {
  SetUpForDebounce(3);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(2U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());
}

TEST_F(GestureEventQueueTest, DebounceDefersGSBIfPreviousGSEDropped) {
  SetUpForDebounce(3);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::Type::kGestureScrollUpdate,
                                    WebInputEvent::Type::kGestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
       i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

// Test that non-scroll events are deferred while scrolling during the debounce
// interval and are discarded if a GestureScrollUpdate event arrives before the
// interval end.
TEST_F(GestureEventQueueTest, DebounceDropsDeferredEvents) {
  SetUpForDebounce(3);

  EXPECT_FALSE(ScrollingInProgress());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // This event should get discarded.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(1U, GestureEventQueueSize());
  EXPECT_EQ(1U, GestureEventDebouncingQueueSize());

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollUpdate,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GestureEventDebouncingQueueSize());
  EXPECT_TRUE(ScrollingInProgress());

  // Verify that the coalescing queue contains the correct events.
  WebInputEvent::Type expected[] = {WebInputEvent::Type::kGestureScrollUpdate,
                                    WebInputEvent::Type::kGestureScrollUpdate};

  for (unsigned i = 0; i < sizeof(expected) / sizeof(WebInputEvent::Type);
       i++) {
    WebGestureEvent merged_event = GestureEventQueueEventAt(i);
    EXPECT_EQ(expected[i], merged_event.GetType());
  }
}

// Test that the fling cancelling tap down event and its following tap get
// suppressed when tap suppression is enabled.
TEST_F(GestureEventQueueTest, TapGetsSuppressedAfterTapDownCancelsFling) {
  SetUpForTapSuppression(400);
  // The velocity of the event must be large enough to make sure that the fling
  // is still active when the tap down happens.
  SimulateGestureFlingStartEvent(0, -1000,
                                 blink::WebGestureDevice::kTouchscreen);
  EXPECT_TRUE(FlingInProgress());
  // The fling start event is not sent to the renderer.
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GetAndResetAckedGestureEventCount());
  RunUntilIdle();

  // Simulate a fling cancel event before sending a gesture tap down event. The
  // fling cancel event is not sent to the renderer.
  SimulateGestureEvent(WebInputEvent::Type::kGestureFlingCancel,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(0U, GestureEventQueueSize());
  RunUntilIdle();

  // Simulate a fling cancelling tap down. The tap down must get suppressed
  // since the fling cancel event is processed by the fling controller.
  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GestureEventQueueSize());

  // The tap event must get suppressed since its corresponding tap down event
  // is suppressed.
  SimulateGestureEvent(WebInputEvent::Type::kGestureTap,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueWithCompositorEventQueueTest,
       PreserveOrderWithOutOfOrderAck) {
  // Simulate a scroll sequence, events should be ACKed in original order.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  SimulateGestureScrollUpdateEvent(8, -4, 1);
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollEnd,
                       blink::WebGestureDevice::kTouchscreen);

  // All events should have been sent.
  EXPECT_EQ(3U, GetAndResetSentGestureEventCount());

  // Simulate GSB ACK.
  SendInputEventACK(WebInputEvent::Type::kGestureScrollBegin,
                    blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin,
            last_acked_event().GetType());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // Simulate GSE ACK first since it's usually dispatched non-blocking.
  SendInputEventACK(WebInputEvent::Type::kGestureScrollEnd,
                    blink::mojom::InputEventResultState::kConsumed);
  // GSE ACK will be cached in GestureEventQueue since we haven't ACKed GSU yet.
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin,
            last_acked_event().GetType());
  EXPECT_EQ(2U, GestureEventQueueSize());

  // Simulate GSU ACK.
  SendInputEventACK(WebInputEvent::Type::kGestureScrollUpdate,
                    blink::mojom::InputEventResultState::kConsumed);
  // Both ACKs should be released in order.
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            last_acked_event().GetType());
  EXPECT_EQ(0U, GestureEventQueueSize());
}

TEST_F(GestureEventQueueWithCompositorEventQueueTest,
       MultipleGesturesInFlight) {
  // Simulate a pinch sequence, events should be forwarded immediately.
  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  SimulateGestureEvent(WebInputEvent::Type::kGesturePinchBegin,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());

  SimulateGestureScrollUpdateEvent(8, -4, 1);
  EXPECT_EQ(1U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(3U, GestureEventQueueSize());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            GestureEventLastQueueEvent().GetType());

  // Simulate 2 pinch update events.
  SimulateGesturePinchUpdateEvent(1.5, 60, 60, 1);
  EXPECT_EQ(4U, GestureEventQueueSize());
  SimulateGesturePinchUpdateEvent(1.3, 60, 60, 1);
  // Events should be forwarded immediately instead of being coalesced.
  EXPECT_EQ(5U, GestureEventQueueSize());
  EXPECT_EQ(2U, GetAndResetSentGestureEventCount());
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            GestureEventLastQueueEvent().GetType());

  SendInputEventACK(WebInputEvent::Type::kGestureScrollBegin,
                    blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(4U, GestureEventQueueSize());

  SendInputEventACK(WebInputEvent::Type::kGesturePinchBegin,
                    blink::mojom::InputEventResultState::kConsumed);
  SendInputEventACK(WebInputEvent::Type::kGestureScrollUpdate,
                    blink::mojom::InputEventResultState::kConsumed);

  // Both GestureScrollUpdate and GesturePinchUpdate should have been sent.
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            last_acked_event().GetType());
  EXPECT_EQ(2U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());

  // Ack the last 2 GesturePinchUpdate events.
  SendInputEventACK(WebInputEvent::Type::kGesturePinchUpdate,
                    blink::mojom::InputEventResultState::kConsumed);
  SendInputEventACK(WebInputEvent::Type::kGesturePinchUpdate,
                    blink::mojom::InputEventResultState::kConsumed);
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            last_acked_event().GetType());
  EXPECT_EQ(0U, GestureEventQueueSize());
  EXPECT_EQ(0U, GetAndResetSentGestureEventCount());
}

}  // namespace input
