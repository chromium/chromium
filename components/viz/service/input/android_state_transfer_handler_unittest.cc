// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/android_state_transfer_handler.h"

#include <android/input.h>

#include <utility>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "components/viz/service/input/viz_touch_state_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

namespace viz {

using ::testing::_;

namespace {

constexpr int kAndroidActionDown = AMOTION_EVENT_ACTION_DOWN;
constexpr int kAndroidActionMove = AMOTION_EVENT_ACTION_MOVE;
constexpr int kAndroidActionUp = AMOTION_EVENT_ACTION_UP;
constexpr FrameSinkId kRootCompositorFrameSinkId = FrameSinkId(1, 1);
constexpr FrameSinkId kRootWidgetFrameSinkId = FrameSinkId(2, 3);

MATCHER_P(EqPixToDip, pix_to_dip, "Matches pix_to_dip value of MotionEvent") {
  return pix_to_dip == (arg.GetX(0) / arg.GetXPix(0));
}

MATCHER_P(EqEventTime, event_time, "Matches event time of MotionEvent") {
  if (arg.GetEventTime() == event_time) {
    return true;
  }

  *result_listener << "GetEventTime: " << arg.GetEventTime()
                   << ", event_time: " << event_time;
  return false;
}

MATCHER_P2(EqXYInPixels, x, y, "Matches x,y values of MotionEvent") {
  if (x == arg.GetXPix(0) && y == arg.GetYPix(0)) {
    return true;
  } else {
    *result_listener << "GetXPix: " << arg.GetXPix(0)
                     << ", GetYPix: " << arg.GetYPix(0);
    return false;
  }
}

base::android::ScopedInputEvent GetInputEvent(jlong down_time_ms,
                                              jlong event_time_ms,
                                              int action,
                                              float x,
                                              float y) {
  // Java_MotionEvent_obtain expects timestamps(down time, event time) obtained
  // from |SystemClock#uptimeMillis()|.
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_motion_event =
      JNI_MotionEvent::Java_MotionEvent_obtain(env, down_time_ms, event_time_ms,
                                               action, x, y,
                                               /*metaState=*/0);
  const AInputEvent* native_event = nullptr;
  if (__builtin_available(android 31, *)) {
    native_event = AMotionEvent_fromJava(env, java_motion_event.obj());
  }
  CHECK(native_event);

  return base::android::ScopedInputEvent(native_event);
}

struct TestInputStream {
  base::TimeTicks down_time_ms;
  std::vector<base::android::ScopedInputEvent> events;

  size_t size() const { return events.size(); }
};

TestInputStream GenerateEventsForSequence(int num_moves,
                                          bool include_touch_up) {
  static base::TimeTicks event_time =
      base::TimeTicks::Now() - base::Milliseconds(100);
  static float x = 100;
  static float y = 200;

  TestInputStream event_stream;

  event_time += base::Milliseconds(8);
  x += 5;
  y += 5;

  jlong down_time = event_time.ToUptimeMillis();
  event_stream.down_time_ms = base::TimeTicks::FromUptimeMillis(down_time);
  event_stream.events.push_back(GetInputEvent(
      down_time, event_time.ToUptimeMillis(), kAndroidActionDown, x, y));

  for (int i = 1; i <= num_moves; i++) {
    event_time += base::Milliseconds(8);
    x += 5;
    y += 5;
    event_stream.events.push_back(GetInputEvent(
        down_time, event_time.ToUptimeMillis(), kAndroidActionMove, x, y));
  }
  if (include_touch_up) {
    event_time += base::Milliseconds(8);
    event_stream.events.push_back(GetInputEvent(
        down_time, event_time.ToUptimeMillis(), kAndroidActionUp, x, y));
  }
  return event_stream;
}

}  // namespace

class MockRenderInputRouterSupportAndroid
    : public RenderInputRouterSupportAndroidInterface {
 public:
  virtual ~MockRenderInputRouterSupportAndroid() = default;

  MOCK_METHOD((bool),
              OnTouchEvent,
              (const ui::MotionEventAndroid&, bool),
              (override));

  base::WeakPtr<RenderInputRouterSupportAndroidInterface> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void Destroy() { weak_factory_.InvalidateWeakPtrs(); }

 private:
  base::WeakPtrFactory<RenderInputRouterSupportAndroidInterface> weak_factory_{
      this};
};

class MockAndroidStateTransferHandlerClient
    : public AndroidStateTransferHandlerClient {
 public:
  MOCK_METHOD((bool), TransferInputBackToBrowser, (), (override));
};

class MockVizTouchStateHandler : public VizTouchStateHandler {
 public:
  MOCK_METHOD(void, UpdateLastTransferredBackDownTimeMs, (int64_t), (override));
};

class AndroidStateTransferHandlerTest : public testing::Test {
 public:
  AndroidStateTransferHandlerTest()
      : handler_(mock_handler_client_, &mock_viz_touch_state_handler_) {}
  void SetUp() override {
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_V) {
      GTEST_SKIP()
          << "AndroidStateTransferHandlerTest is used only when InputOnViz "
             "is enabled i.e. on Android V+";
    }
  }

 protected:
  MockRenderInputRouterSupportAndroid mock_rir_support_;
  MockAndroidStateTransferHandlerClient mock_handler_client_;
  MockVizTouchStateHandler mock_viz_touch_state_handler_;
  AndroidStateTransferHandler handler_;
};

// The order of events received:
// Down1 -> Move1 -> StateTransfer(for Down1)
TEST_F(AndroidStateTransferHandlerTest, EventsProcessedOnStateTransfer) {
  TestInputStream event_stream = GenerateEventsForSequence(
      /*num_moves*/ 1,
      /*include_touch_up*/ false);

  for (auto& event : event_stream.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 2u);

  auto state = input::mojom::TouchTransferState::New();
  state->down_time_ms = event_stream.down_time_ms;
  state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;

  // Both the events should be dequeued and processed.
  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(2);
  handler_.StateOnTouchTransfer(std::move(state),
                                mock_rir_support_.GetWeakPtr());
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

// The order of events received:
// Down1 -> Move1 -> StateTransfer(for Down1)
// The rir_support for widget referred in state is already destroyed.
TEST_F(AndroidStateTransferHandlerTest, RIRSupportNullOnStateTransfer) {
  TestInputStream event_stream = GenerateEventsForSequence(
      /*num_moves*/ 1,
      /*include_touch_up*/ false);

  for (auto& event : event_stream.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 2u);

  auto state = input::mojom::TouchTransferState::New();
  state->down_time_ms = event_stream.down_time_ms;
  state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(0);
  handler_.StateOnTouchTransfer(std::move(state),
                                /* rir_support= */ nullptr);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

// The order of events received:
// StateTransfer(for Down1) -> Down1 -> Move1 -> OnDestroyedCompositorFrameSink
// -> Move3.
// Move3 ends up getting dropped.
TEST_F(AndroidStateTransferHandlerTest,
       InputReceivingCompositorFrameSinkDestroyedMidSequence) {
  TestInputStream event_stream = GenerateEventsForSequence(
      /*num_moves*/ 2,
      /*include_touch_up*/ false);

  auto state = input::mojom::TouchTransferState::New();
  state->down_time_ms = event_stream.down_time_ms;
  state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;

  handler_.StateOnTouchTransfer(std::move(state),
                                mock_rir_support_.GetWeakPtr());
  handler_.OnMotionEvent(std::move(event_stream.events[0]),
                         kRootCompositorFrameSinkId);
  handler_.OnMotionEvent(std::move(event_stream.events[1]),
                         kRootCompositorFrameSinkId);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(0);
  mock_rir_support_.Destroy();

  //  The events are dropped after the render input router support is
  //  destroyed.
  handler_.OnMotionEvent(std::move(event_stream.events[2]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

// The order of events received:
// StateTransfer(for Down1) -> Down1 -> Move1
// Down1 and Move1 can be immediately processed.
TEST_F(AndroidStateTransferHandlerTest, StateReceivedBeforeTouchDownOnViz) {
  TestInputStream event_stream = GenerateEventsForSequence(
      /*num_moves*/ 2,
      /*include_touch_up*/ false);

  auto state = input::mojom::TouchTransferState::New();
  state->down_time_ms = event_stream.down_time_ms;
  state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
  handler_.StateOnTouchTransfer(std::move(state),
                                mock_rir_support_.GetWeakPtr());

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(1);
  handler_.OnMotionEvent(std::move(event_stream.events[0]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(1);
  handler_.OnMotionEvent(std::move(event_stream.events[1]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

// The order of events received:
// StateTransfer(for Down1) -> Down1 -> Move1 -> StateTransfer(for Down2) -> Up1
// -> Down2 Down1 and move1 can be immediately processed.
TEST_F(AndroidStateTransferHandlerTest, NewStateReceivedMidSequence) {
  TestInputStream event_stream_1 =
      GenerateEventsForSequence(/*num_moves*/ 1,
                                /*include_touch_up*/ true);

  TestInputStream event_stream_2 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ false);

  // State is received before receiving any touch events directly on Viz
  auto state1 = input::mojom::TouchTransferState::New();
  state1->down_time_ms = event_stream_1.down_time_ms;

  handler_.StateOnTouchTransfer(std::move(state1),
                                mock_rir_support_.GetWeakPtr());

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(3);
  // Down1 is received.
  handler_.OnMotionEvent(std::move(event_stream_1.events[0]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);

  // Move1 is received.
  handler_.OnMotionEvent(std::move(event_stream_1.events[1]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);

  // State for second sequence is received.
  MockRenderInputRouterSupportAndroid mock_rir_support2;
  auto state2 = input::mojom::TouchTransferState::New();
  state2->down_time_ms = event_stream_2.down_time_ms;
  handler_.StateOnTouchTransfer(std::move(state2),
                                mock_rir_support2.GetWeakPtr());

  // Up1 is received.
  handler_.OnMotionEvent(std::move(event_stream_1.events[2]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);

  // Down1 is received from second sequence, on different
  // RenderInputRouterSupport.
  EXPECT_CALL(mock_rir_support2, OnTouchEvent(_, _)).Times(1);
  handler_.OnMotionEvent(std::move(event_stream_2.events[0]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

// The order of events received:
// Down1 -> Move1 -> Up1 -> Down2 -> StateTransfer(for Down1)
// Down1 and move1 can be immediately processed.
TEST_F(AndroidStateTransferHandlerTest,
       FullSequenceReceivedBeforeStateTransfer) {
  TestInputStream event_stream_1 =
      GenerateEventsForSequence(/*num_moves*/ 1,
                                /*include_touch_up*/ true);

  TestInputStream event_stream_2 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ false);

  for (auto& event : event_stream_1.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 3u);

  for (auto& event : event_stream_2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 4u);

  // State is received before receiving any touch events directly on Viz
  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(3);
  auto state1 = input::mojom::TouchTransferState::New();
  state1->down_time_ms = event_stream_1.down_time_ms;
  handler_.StateOnTouchTransfer(std::move(state1),
                                mock_rir_support_.GetWeakPtr());
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 1u);
}

// The order of events received:
// StateTransfer(Down2) -> Down1 -> Up1 -> Down2
TEST_F(AndroidStateTransferHandlerTest,
       InputEventsAreNotQueuedForDroppedStateTransfer) {
  TestInputStream event_stream_1 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ true);

  TestInputStream event_stream_2 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ false);

  auto state2 = input::mojom::TouchTransferState::New();
  state2->down_time_ms = event_stream_2.down_time_ms;
  handler_.StateOnTouchTransfer(std::move(state2),
                                mock_rir_support_.GetWeakPtr());

  for (auto& event : event_stream_1.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(1);
  for (auto& event : event_stream_2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

// The order of events received:
// Down1 -> Up1 -> Down2 -> Up2 -> Down3 -> StateTransfer(Down3)
TEST_F(AndroidStateTransferHandlerTest,
       QueuedInputEventsDroppedUponLaterStateTransfer) {
  TestInputStream event_stream_1 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ true);

  TestInputStream event_stream_2 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ true);

  TestInputStream event_stream_3 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ false);

  for (auto& event : event_stream_1.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  for (auto& event : event_stream_2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  for (auto& event : event_stream_3.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(),
            event_stream_1.events.size() + event_stream_2.events.size() +
                event_stream_3.events.size());

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(1);
  auto state3 = input::mojom::TouchTransferState::New();
  state3->down_time_ms = event_stream_3.down_time_ms;
  handler_.StateOnTouchTransfer(std::move(state3),
                                mock_rir_support_.GetWeakPtr());
}

// The order of events received:
// StateTransfer(Down1) -> Down1 -> Move1 -> OnDestroyedCompositorFrameSink ->
// Up1 -> Down2 -> StateTransfer(Down2)
TEST_F(AndroidStateTransferHandlerTest, RirNullOnLastInputInSequence) {
  TestInputStream event_stream_1 =
      GenerateEventsForSequence(/*num_moves*/ 1,
                                /*include_touch_up*/ true);

  TestInputStream event_stream_2 =
      GenerateEventsForSequence(/*num_moves*/ 0,
                                /*include_touch_up*/ false);

  auto state1 = input::mojom::TouchTransferState::New();
  state1->down_time_ms = event_stream_1.down_time_ms;
  handler_.StateOnTouchTransfer(std::move(state1),
                                mock_rir_support_.GetWeakPtr());

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(2);
  handler_.OnMotionEvent(std::move(event_stream_1.events[0]),
                         kRootCompositorFrameSinkId);
  handler_.OnMotionEvent(std::move(event_stream_1.events[1]),
                         kRootCompositorFrameSinkId);

  mock_rir_support_.Destroy();

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(0);
  handler_.OnMotionEvent(std::move(event_stream_1.events[2]),
                         kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);

  for (auto& event : event_stream_2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 1u);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(1);
  auto state2 = input::mojom::TouchTransferState::New();
  state2->down_time_ms = event_stream_2.down_time_ms;
  handler_.StateOnTouchTransfer(std::move(state2),
                                mock_rir_support_.GetWeakPtr());
}

TEST_F(AndroidStateTransferHandlerTest, UsesDeviceScaleFactorFromState) {
  TestInputStream event_stream =
      GenerateEventsForSequence(/*num_moves*/ 1,
                                /*include_touch_up*/ true);

  float dip_scale = 2.5f;

  auto state = input::mojom::TouchTransferState::New();
  state->down_time_ms = event_stream.down_time_ms;
  state->dip_scale = dip_scale;
  handler_.StateOnTouchTransfer(std::move(state),
                                mock_rir_support_.GetWeakPtr());

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(EqPixToDip(1.f / dip_scale), _))
      .Times(event_stream.events.size());
  for (auto& event : event_stream.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
}

//  _______________________
// | (sys_ui_offset)       |
// |-----------------------|
// | (web_contents_offset) |
// |-----------------------|
// |                       |
// |                       |
// |                       |
// |                       |
// |                       |
// |                       |
// |-----------------------|
//
TEST_F(AndroidStateTransferHandlerTest,
       UsesWebContentsOffsetForMotionEventCreation) {
  int sys_ui_offset = -9;
  const int web_contents_offset = -8;
  const int raw_y = 109;
  const int delta_x = 0;
  const int meta_state = 0;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_motion_event =
      JNI_MotionEvent::Java_MotionEvent_obtain(env, 0, 0, kAndroidActionDown, 0,
                                               raw_y, meta_state);
  JNI_MotionEvent::Java_MotionEvent_offsetLocation(env, java_motion_event,
                                                   delta_x, sys_ui_offset);

  const AInputEvent* native_event = nullptr;
  if (__builtin_available(android 31, *)) {
    native_event = AMotionEvent_fromJava(env, java_motion_event.obj());
  }
  CHECK(native_event);

  auto state = input::mojom::TouchTransferState::New();
  state->dip_scale = 1.f;
  state->web_contents_y_offset_pix = web_contents_offset;

  handler_.StateOnTouchTransfer(std::move(state),
                                mock_rir_support_.GetWeakPtr());

  int expected_y = raw_y + sys_ui_offset + web_contents_offset;
  EXPECT_CALL(mock_rir_support_, OnTouchEvent(EqXYInPixels(0, expected_y), _));
  handler_.OnMotionEvent(base::android::ScopedInputEvent(native_event),
                         kRootCompositorFrameSinkId);

  // Offset by an arbitrary value which is larger than absolute value of
  // `web_contents_offset`.
  sys_ui_offset -= 10;
  base::android::ScopedJavaLocalRef<jobject>
      motion_event_with_diff_sys_ui_offset =
          JNI_MotionEvent::Java_MotionEvent_obtain(
              env, 0, 0, kAndroidActionMove, 0, raw_y, meta_state);
  JNI_MotionEvent::Java_MotionEvent_offsetLocation(
      env, motion_event_with_diff_sys_ui_offset, delta_x, sys_ui_offset);

  if (__builtin_available(android 31, *)) {
    native_event =
        AMotionEvent_fromJava(env, motion_event_with_diff_sys_ui_offset.obj());
  }
  CHECK(native_event);

  expected_y = raw_y + sys_ui_offset + web_contents_offset;
  EXPECT_CALL(mock_rir_support_, OnTouchEvent(EqXYInPixels(0, expected_y), _));
  handler_.OnMotionEvent(base::android::ScopedInputEvent(native_event),
                         kRootCompositorFrameSinkId);
}

// Sequence1: |-----|
// Sequence2:        |-----|
// If the touch end of Sequence1 didn't arrive on Browser in time, Browser would
// assume it is a pointer down and transfer the Sequence 2 to Viz, while in some
// cases it would have wanted to handle it.
TEST_F(AndroidStateTransferHandlerTest,
       ReturnBackSequenceThatBrowserWouldHaveHandled) {
  TestInputStream event_stream_1 = GenerateEventsForSequence(
      /*num_moves*/ 1,
      /*include_touch_up*/ true);
  auto state1 = input::mojom::TouchTransferState::New();
  state1->down_time_ms = event_stream_1.down_time_ms;
  state1->root_widget_frame_sink_id = kRootWidgetFrameSinkId;

  TestInputStream event_stream_2 = GenerateEventsForSequence(
      /*num_moves*/ 2,
      /*include_touch_up*/ true);
  auto state2 = input::mojom::TouchTransferState::New();
  state2->down_time_ms = event_stream_2.down_time_ms;
  state2->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
  state2->browser_would_have_handled = true;

  handler_.StateOnTouchTransfer(std::move(state1),
                                mock_rir_support_.GetWeakPtr());
  handler_.StateOnTouchTransfer(std::move(state2),
                                mock_rir_support_.GetWeakPtr());
  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(3);
  for (auto& event : event_stream_1.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }

  EXPECT_CALL(mock_handler_client_, TransferInputBackToBrowser()).Times(1);
  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(0);
  for (auto& event : event_stream_2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
}

TEST_F(AndroidStateTransferHandlerTest, OlderStatesAreDropped) {
  TestInputStream event_stream_1 = GenerateEventsForSequence(
      /*num_moves*/ 2,
      /*include_touch_up*/ true);

  TestInputStream event_stream_2 = GenerateEventsForSequence(
      /*num_moves*/ 2,
      /*include_touch_up*/ false);

  auto state1 = input::mojom::TouchTransferState::New();
  state1->down_time_ms = event_stream_1.down_time_ms;
  state1->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
  auto state1_pointer_down = input::mojom::TouchTransferState::New();
  state1_pointer_down->down_time_ms =
      event_stream_1.down_time_ms + base::Milliseconds(10);
  state1_pointer_down->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
  auto state2 = input::mojom::TouchTransferState::New();
  state2->down_time_ms = event_stream_2.down_time_ms;
  state2->root_widget_frame_sink_id = kRootWidgetFrameSinkId;

  handler_.StateOnTouchTransfer(std::move(state1),
                                mock_rir_support_.GetWeakPtr());
  handler_.StateOnTouchTransfer(std::move(state1_pointer_down),
                                mock_rir_support_.GetWeakPtr());
  handler_.StateOnTouchTransfer(std::move(state2),
                                mock_rir_support_.GetWeakPtr());

  EXPECT_EQ(handler_.GetPendingTransferredStatesSizeForTesting(), 3u);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _))
      .Times(event_stream_1.size() + event_stream_2.size());

  for (auto& event : event_stream_1.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  for (auto& event : event_stream_2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetPendingTransferredStatesSizeForTesting(), 0u);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

TEST_F(AndroidStateTransferHandlerTest, DownEventUsesDownTimeAsEventTime) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(100);
  const jlong down_time_ms = event_time.ToUptimeMillis();
  const base::TimeTicks down_time =
      base::TimeTicks::FromUptimeMillis(down_time_ms);

  // Create an ACTION_DOWN event with different event time and down time.
  event_time += base::Milliseconds(8);
  base::android::ScopedInputEvent down_event =
      GetInputEvent(down_time_ms, event_time.ToUptimeMillis(),
                    kAndroidActionDown, /*x=*/100, /*y*/ 100);

  {
    auto state = input::mojom::TouchTransferState::New();
    state->down_time_ms = base::TimeTicks::FromUptimeMillis(down_time_ms);
    state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
    handler_.StateOnTouchTransfer(std::move(state),
                                  mock_rir_support_.GetWeakPtr());
  }

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(EqEventTime(down_time), _))
      .Times(1);
  handler_.OnMotionEvent(std::move(down_event), kRootCompositorFrameSinkId);
}

// Touch events seen by Browser: TouchDown1, TouchUp1, TouchDown2.
// In such a scenario it's possible that Browser requests for transfer of
// sequence with TouchDown1 but by the time system server handled transfer
// request a new sequence with TouchDown2 was transferred, and Browser wouldn't
// have enough information to tell which sequence was transferred.
// In such scenarios Browser sends a state for each touch down it sees until
// cancel. Make sure Seqeunce2 is not dropped in such scenarios due to
// mismatching state.
TEST_F(AndroidStateTransferHandlerTest,
       SystemTransfersFollowupSequenceIsNotDropped) {
  base::TimeTicks event_time = base::TimeTicks::Now() - base::Milliseconds(100);
  jlong down_time_ms = event_time.ToUptimeMillis();
  base::TimeTicks down_time = base::TimeTicks::FromUptimeMillis(down_time_ms);

  {
    auto state = input::mojom::TouchTransferState::New();
    state->down_time_ms = down_time;
    state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
    handler_.StateOnTouchTransfer(std::move(state),
                                  mock_rir_support_.GetWeakPtr());
  }
  EXPECT_EQ(handler_.GetPendingTransferredStatesSizeForTesting(), 1u);

  // Create an ACTION_DOWN event later than the last state sent to system.
  down_time += base::Milliseconds(8);
  event_time = down_time + base::Milliseconds(8);
  base::android::ScopedInputEvent down_event =
      GetInputEvent(down_time.ToUptimeMillis(), event_time.ToUptimeMillis(),
                    kAndroidActionDown, /*x=*/100, /*y*/ 100);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(0);
  handler_.OnMotionEvent(std::move(down_event), kRootCompositorFrameSinkId);
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 1u);
  // States with down time older than current event are dropped.
  EXPECT_EQ(handler_.GetPendingTransferredStatesSizeForTesting(), 0u);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(1);
  {
    auto state = input::mojom::TouchTransferState::New();
    state->down_time_ms = down_time;
    state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
    handler_.StateOnTouchTransfer(std::move(state),
                                  mock_rir_support_.GetWeakPtr());
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
  EXPECT_EQ(handler_.GetPendingTransferredStatesSizeForTesting(), 0u);
}

TEST_F(AndroidStateTransferHandlerTest,
       CallsUpdateLastTransferredBackDownTimeMs) {
  TestInputStream event_stream = GenerateEventsForSequence(
      /*num_moves*/ 1,
      /*include_touch_up*/ true);
  auto state = input::mojom::TouchTransferState::New();
  state->down_time_ms = event_stream.down_time_ms;
  state->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
  state->browser_would_have_handled = true;

  handler_.StateOnTouchTransfer(std::move(state),
                                mock_rir_support_.GetWeakPtr());

  // Expect UpdateLastTransferredBackDownTimeMs to be called when
  // browser_would_have_handled is true.
  EXPECT_CALL(mock_viz_touch_state_handler_,
              UpdateLastTransferredBackDownTimeMs(
                  event_stream.down_time_ms.ToUptimeMillis()))
      .Times(1);
  EXPECT_CALL(mock_handler_client_, TransferInputBackToBrowser()).Times(1);

  for (auto& event : event_stream.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }

  // Now test the case where browser_would_have_handled is false.
  testing::Mock::VerifyAndClearExpectations(&mock_viz_touch_state_handler_);
  TestInputStream event_stream2 = GenerateEventsForSequence(
      /*num_moves*/ 1,
      /*include_touch_up*/ true);
  auto state2 = input::mojom::TouchTransferState::New();
  state2->down_time_ms = event_stream2.down_time_ms;
  state2->root_widget_frame_sink_id = kRootWidgetFrameSinkId;
  state2->browser_would_have_handled = false;

  handler_.StateOnTouchTransfer(std::move(state2),
                                mock_rir_support_.GetWeakPtr());

  // Expect UpdateLastTransferredBackDownTimeMs to be called with 0.
  EXPECT_CALL(mock_viz_touch_state_handler_,
              UpdateLastTransferredBackDownTimeMs(0))
      .Times(1);
  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(3);

  for (auto& event : event_stream2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
}

TEST_F(AndroidStateTransferHandlerTest, FirstSequenceTransferredBackToBrowser) {
  TestInputStream event_stream_1 = GenerateEventsForSequence(
      /*num_moves*/ 1,
      /*include_touch_up*/ true);

  TestInputStream event_stream_2 = GenerateEventsForSequence(
      /*num_moves*/ 1,
      /*include_touch_up*/ false);

  auto state1 = input::mojom::TouchTransferState::New();
  state1->down_time_ms = event_stream_1.down_time_ms;
  state1->browser_would_have_handled = true;

  auto state2 = input::mojom::TouchTransferState::New();
  state2->down_time_ms = event_stream_2.down_time_ms;
  state2->root_widget_frame_sink_id = kRootWidgetFrameSinkId;

  for (auto& event : event_stream_1.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  for (auto& event : event_stream_2.events) {
    handler_.OnMotionEvent(std::move(event), kRootCompositorFrameSinkId);
  }
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 5u);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(0);
  handler_.StateOnTouchTransfer(std::move(state1),
                                mock_rir_support_.GetWeakPtr());
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 2u);

  EXPECT_CALL(mock_rir_support_, OnTouchEvent(_, _)).Times(2);
  handler_.StateOnTouchTransfer(std::move(state2),
                                mock_rir_support_.GetWeakPtr());
  EXPECT_EQ(handler_.GetEventsBufferSizeForTesting(), 0u);
}

}  // namespace viz
