// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/viz_touch_state_handler.h"

#include <android/input.h>

#include <memory>
#include <vector>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/input/viz_touch_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/android/motion_event_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/events/motionevent_jni_headers/MotionEvent_jni.h"

namespace viz {
namespace {

constexpr int kAndroidActionDown = AMOTION_EVENT_ACTION_DOWN;
constexpr int kAndroidActionMove = AMOTION_EVENT_ACTION_MOVE;
constexpr int kAndroidActionUp = AMOTION_EVENT_ACTION_UP;
constexpr int kAndroidActionCancel = AMOTION_EVENT_ACTION_CANCEL;

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

class VizTouchStateHandlerTest : public testing::Test {
 public:
  VizTouchStateHandlerTest() = default;
  ~VizTouchStateHandlerTest() override = default;

  void SetUp() override {
    if (base::android::android_info::sdk_int() <
        base::android::android_info::SDK_VERSION_V) {
      GTEST_SKIP() << "VizTouchStateHandlerTest is used only when InputOnViz "
                      "is enabled i.e. on Android V+";
    }
  }

 protected:
  const VizTouchState* GetVizTouchState() {
    base::ReadOnlySharedMemoryRegion region =
        handler_.DuplicateVizTouchStateRegion();
    if (!region.IsValid()) {
      return nullptr;
    }
    mapping_ = region.Map();
    if (!mapping_.IsValid()) {
      return nullptr;
    }
    return mapping_.GetMemoryAs<const VizTouchState>();
  }

  VizTouchStateHandler handler_;
  base::ReadOnlySharedMemoryMapping mapping_;
};

TEST_F(VizTouchStateHandlerTest, Initialization) {
  const VizTouchState* state = GetVizTouchState();
  ASSERT_TRUE(state);
  EXPECT_FALSE(state->is_sequence_active.load(std::memory_order_acquire));
}

TEST_F(VizTouchStateHandlerTest, VizTouchStateWritesCorrectly) {
  const VizTouchState* state = GetVizTouchState();
  ASSERT_TRUE(state);

  // Initially, no touch sequence should be active.
  EXPECT_FALSE(state->is_sequence_active.load(std::memory_order_acquire));

  // Simulate a full down -> move -> up sequence.
  TestInputStream stream1 = GenerateEventsForSequence(1, true);
  handler_.OnMotionEvent(stream1.events[0]);  // DOWN
  EXPECT_TRUE(state->is_sequence_active.load(std::memory_order_acquire));
  handler_.OnMotionEvent(stream1.events[1]);  // MOVE
  EXPECT_TRUE(state->is_sequence_active.load(std::memory_order_acquire));
  handler_.OnMotionEvent(stream1.events[2]);  // UP
  EXPECT_FALSE(state->is_sequence_active.load(std::memory_order_acquire));

  // Simulate a down -> cancel sequence.
  TestInputStream stream2 = GenerateEventsForSequence(0, false);
  handler_.OnMotionEvent(stream2.events[0]);  // DOWN
  EXPECT_TRUE(state->is_sequence_active.load(std::memory_order_acquire));
  // Manually create a CANCEL event.
  base::android::ScopedInputEvent cancel_event = GetInputEvent(
      stream2.down_time_ms.ToUptimeMillis(),
      (stream2.down_time_ms + base::Milliseconds(8)).ToUptimeMillis(),
      kAndroidActionCancel, 10, 10);
  handler_.OnMotionEvent(cancel_event);
  EXPECT_FALSE(state->is_sequence_active.load(std::memory_order_acquire));
}

TEST_F(VizTouchStateHandlerTest, UpdateLastTransferredBackDownTimeMs) {
  const VizTouchState* state = GetVizTouchState();
  ASSERT_TRUE(state);

  // Initially, the last transferred back down time should be 0.
  EXPECT_EQ(
      state->last_transferred_back_down_time_ms.load(std::memory_order_acquire),
      0);

  // Update the last transferred back down time.
  int64_t down_time_ms = base::TimeTicks::Now().ToUptimeMillis();
  handler_.UpdateLastTransferredBackDownTimeMs(down_time_ms);
  EXPECT_EQ(
      state->last_transferred_back_down_time_ms.load(std::memory_order_acquire),
      down_time_ms);

  // Update it again with a different value.
  down_time_ms += 100;
  handler_.UpdateLastTransferredBackDownTimeMs(down_time_ms);
  EXPECT_EQ(
      state->last_transferred_back_down_time_ms.load(std::memory_order_acquire),
      down_time_ms);

  // Reset the last transferred back down time to 0.
  handler_.UpdateLastTransferredBackDownTimeMs(0);
  EXPECT_EQ(
      state->last_transferred_back_down_time_ms.load(std::memory_order_acquire),
      0);
}

}  // namespace
}  // namespace viz
