// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/touchpad_pinch_event_queue.h"

#include <string>

#include "base/functional/bind.h"
#include "components/input/event_with_latency_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/latency/latency_info.h"

namespace input {

class MockTouchpadPinchEventQueueClient {
 public:
  MockTouchpadPinchEventQueueClient() = default;
  ~MockTouchpadPinchEventQueueClient() = default;

  // TouchpadPinchEventQueueClient
  MOCK_METHOD1(SendMouseWheelEventForPinchImmediately,
               void(const MouseWheelEventWithLatencyInfo& event));
  MOCK_METHOD3(OnGestureEventForPinchAck,
               void(const GestureEventWithLatencyInfo& event,
                    blink::mojom::InputEventResultSource ack_source,
                    blink::mojom::InputEventResultState ack_result));
};

class TouchpadPinchEventQueueTest : public testing::TestWithParam<bool>,
                                    public TouchpadPinchEventQueueClient {
 protected:
  TouchpadPinchEventQueueTest() {
    queue_ = std::make_unique<TouchpadPinchEventQueue>(this);
  }
  ~TouchpadPinchEventQueueTest() = default;

  void QueueEvent(const blink::WebGestureEvent& event) {
    queue_->QueueEvent(GestureEventWithLatencyInfo(event));
  }

  void QueuePinchBegin() {
    blink::WebGestureEvent event(
        blink::WebInputEvent::Type::kGesturePinchBegin,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.SetNeedsWheelEvent(true);
    QueueEvent(event);
  }

  void QueuePinchEnd() {
    blink::WebGestureEvent event(
        blink::WebInputEvent::Type::kGesturePinchEnd,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.SetNeedsWheelEvent(true);
    QueueEvent(event);
  }

  void QueuePinchUpdate(float scale, bool zoom_disabled) {
    blink::WebGestureEvent event(
        blink::WebInputEvent::Type::kGesturePinchUpdate,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.SetNeedsWheelEvent(true);
    event.data.pinch_update.zoom_disabled = zoom_disabled;
    event.data.pinch_update.scale = scale;
    QueueEvent(event);
  }

  void QueueDoubleTap() {
    blink::WebGestureEvent event(
        blink::WebInputEvent::Type::kGestureDoubleTap,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests(),
        blink::WebGestureDevice::kTouchpad);
    event.SetPositionInWidget(gfx::PointF(1, 1));
    event.SetPositionInScreen(gfx::PointF(1, 1));
    event.data.tap.tap_count = 1;
    event.SetNeedsWheelEvent(true);
    QueueEvent(event);
  }

  using HandleEventCallback =
      base::OnceCallback<void(blink::mojom::InputEventResultSource ack_source,
                              blink::mojom::InputEventResultState ack_result)>;

  void SendWheelEventAck(blink::mojom::InputEventResultSource ack_source,
                         blink::mojom::InputEventResultState ack_result) {
    std::move(callbacks_.front()).Run(ack_source, ack_result);
    callbacks_.pop_front();
  }

  void SendMouseWheelEventForPinchImmediately(
      const MouseWheelEventWithLatencyInfo& event,
      MouseWheelEventHandledCallback callback) override {
    mock_client_.SendMouseWheelEventForPinchImmediately(event);
    callbacks_.emplace_back(base::BindOnce(
        [](MouseWheelEventHandledCallback callback,
           const MouseWheelEventWithLatencyInfo& event,
           blink::mojom::InputEventResultSource ack_source,
           blink::mojom::InputEventResultState ack_result) {
          std::move(callback).Run(event, ack_source, ack_result);
        },
        std::move(callback), event));
  }

  void OnGestureEventForPinchAck(
      const GestureEventWithLatencyInfo& event,
      blink::mojom::InputEventResultSource ack_source,
      blink::mojom::InputEventResultState ack_result) override {
    mock_client_.OnGestureEventForPinchAck(event, ack_source, ack_result);
  }

  testing::StrictMock<MockTouchpadPinchEventQueueClient> mock_client_;
  std::unique_ptr<TouchpadPinchEventQueue> queue_;
  base::circular_deque<HandleEventCallback> callbacks_;
};

MATCHER_P(EventHasType,
          type,
          std::string(negation ? "does not have" : "has") + " type " +
              ::testing::PrintToString(type)) {
  return arg.event.GetType() == type;
}

MATCHER_P(EventHasPhase,
          phase,
          std::string(negation ? "does not have" : "has") + " phase " +
              ::testing::PrintToString(phase)) {
  return arg.event.phase == phase;
}

MATCHER_P(EventHasScale,
          expected_scale,
          std::string(negation ? "does not have" : "has") + " scale " +
              ::testing::PrintToString(expected_scale)) {
  const float actual_scale = exp(arg.event.delta_y / 100.0f);
  return ::testing::Matches(::testing::FloatEq(expected_scale))(actual_scale);
}

MATCHER(EventHasCtrlModifier,
        std::string(negation ? "does not have" : "has") + " control modifier") {
  return (arg.event.GetModifiers() & blink::WebInputEvent::kControlKey) != 0;
}

MATCHER(EventIsBlocking,
        std::string(negation ? "is not" : "is") + " blocking") {
  return arg.event.dispatch_type ==
         blink::WebInputEvent::DispatchType::kBlocking;
}

// Ensure that when the queue receives a touchpad pinch sequence, it sends a
// synthetic mouse wheel event and acks the pinch events back to the client.
TEST_F(TouchpadPinchEventQueueTest, Basic) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

// Ensure the queue sends the wheel events with phase information.
TEST_F(TouchpadPinchEventQueueTest, MouseWheelPhase) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseBegan)));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseChanged)));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  testing::_, testing::_));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseEnded)));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

// Ensure that if the renderer consumes the synthetic wheel event, the ack of
// the GesturePinchUpdate reflects this.
TEST_F(TouchpadPinchEventQueueTest, Consumed) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kMainThread,
                  blink::mojom::InputEventResultState::kConsumed));

  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kMainThread,
                    blink::mojom::InputEventResultState::kConsumed);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

// Ensure that the queue sends wheel events for updates with |zoom_disabled| as
// well.
TEST_F(TouchpadPinchEventQueueTest, ZoomDisabled) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists));

  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, true);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

TEST_F(TouchpadPinchEventQueueTest, MultipleSequences) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored))
      .Times(2);
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_))
      .Times(4);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists))
      .Times(2);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored))
      .Times(2);

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

// Ensure we can queue additional pinch event sequences while the queue is
// waiting for a wheel event ack.
TEST_F(TouchpadPinchEventQueueTest, MultipleQueuedSequences) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_));
  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);

  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  QueuePinchEnd();

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  QueuePinchEnd();

  // No calls since we're still waiting on the ack for the first wheel event.
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_))
      .Times(2);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  // ACK for end event.
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);

  // After acking the first wheel event, the queue continues.
  testing::Mock::VerifyAndClearExpectations(&mock_client_);

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_, SendMouseWheelEventForPinchImmediately(testing::_));
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  // ACK for end event.
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

// Ensure the queue handles pinch event sequences with multiple updates.
TEST_F(TouchpadPinchEventQueueTest, MultipleUpdatesInSequence) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  // Only first wheel event is cancelable.
  // Here the second and the third wheel events are not blocking because we
  // ack the first wheel event as unconsumed.
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))))
      .Times(3);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored))
      .Times(2);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

// Ensure the queue coalesces pinch update events.
TEST_F(TouchpadPinchEventQueueTest, MultipleUpdatesCoalesced) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  // Only the first wheel event is cancelable.
  // Here the second wheel is not blocking because we ack the first wheel
  // event as unconsumed.
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))))
      .Times(2);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kCompositorThread,
                  blink::mojom::InputEventResultState::kNoConsumerExists));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  // The queue will send the first wheel event for this first update.
  QueuePinchUpdate(1.23, false);
  // Before the first wheel event is acked, queue another update.
  QueuePinchUpdate(1.23, false);
  // Queue a third update. This will be coalesced with the second update which
  // is currently in the queue.
  QueuePinchUpdate(1.23, false);
  QueuePinchEnd();

  // Ack for the wheel event corresponding to the first update.
  SendWheelEventAck(blink::mojom::InputEventResultSource::kCompositorThread,
                    blink::mojom::InputEventResultState::kNoConsumerExists);
  // Ack for the wheel event corresponding to the second and third updates.
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
  // ACK for end event.
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
  EXPECT_FALSE(queue_->has_pending());
}

// Ensure the queue handles pinch event sequences with multiple canceled
// updates.
TEST_F(TouchpadPinchEventQueueTest, MultipleCanceledUpdatesInSequence) {
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())))
      .Times(3);
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kMainThread,
                  blink::mojom::InputEventResultState::kConsumed))
      .Times(3);
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kMainThread,
                    blink::mojom::InputEventResultState::kConsumed);
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kMainThread,
                    blink::mojom::InputEventResultState::kConsumed);
  QueuePinchUpdate(1.23, false);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kMainThread,
                    blink::mojom::InputEventResultState::kConsumed);
  QueuePinchEnd();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

// Ensure that when the queue receives a touchpad double tap, it sends a
// synthetic mouse wheel event and acks the double tap back to the client.
TEST_F(TouchpadPinchEventQueueTest, DoubleTap) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), EventIsBlocking(),
                  EventHasPhase(blink::WebMouseWheelEvent::kPhaseNone),
                  EventHasScale(1.0f))));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGestureDoubleTap),
                  blink::mojom::InputEventResultSource::kMainThread,
                  blink::mojom::InputEventResultState::kConsumed));

  QueueDoubleTap();
  SendWheelEventAck(blink::mojom::InputEventResultSource::kMainThread,
                    blink::mojom::InputEventResultState::kConsumed);
}

// Ensure that ACKs are only processed when they match the event that is
// currently awaiting an ACK.
TEST_F(TouchpadPinchEventQueueTest, IgnoreNonMatchingEvents) {
  ::testing::InSequence sequence;
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchBegin),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));
  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(
                  ::testing::AllOf(EventHasCtrlModifier(), EventIsBlocking())));
  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchUpdate),
                  blink::mojom::InputEventResultSource::kMainThread,
                  blink::mojom::InputEventResultState::kConsumed));

  EXPECT_CALL(mock_client_,
              SendMouseWheelEventForPinchImmediately(::testing::AllOf(
                  EventHasCtrlModifier(), ::testing::Not(EventIsBlocking()))));

  EXPECT_CALL(mock_client_,
              OnGestureEventForPinchAck(
                  EventHasType(blink::WebInputEvent::Type::kGesturePinchEnd),
                  blink::mojom::InputEventResultSource::kBrowser,
                  blink::mojom::InputEventResultState::kIgnored));

  QueuePinchBegin();
  QueuePinchUpdate(1.23, false);
  QueuePinchEnd();

  SendWheelEventAck(blink::mojom::InputEventResultSource::kMainThread,
                    blink::mojom::InputEventResultState::kConsumed);
  SendWheelEventAck(blink::mojom::InputEventResultSource::kBrowser,
                    blink::mojom::InputEventResultState::kIgnored);
}

}  // namespace input
