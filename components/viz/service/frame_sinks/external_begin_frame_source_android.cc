// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

#include "base/android/jni_android.h"
#include "components/viz/service/service_jni_headers/ExternalBeginFrameSourceAndroid_jni.h"

namespace viz {

ExternalBeginFrameSourceAndroid::ExternalBeginFrameSourceAndroid(
    uint32_t restart_id,
    float refresh_rate)
    : ExternalBeginFrameSource(this, restart_id),
      j_object_(Java_ExternalBeginFrameSourceAndroid_Constructor(
          base::android::AttachCurrentThread(),
          reinterpret_cast<jlong>(this),
          refresh_rate)) {}

ExternalBeginFrameSourceAndroid::~ExternalBeginFrameSourceAndroid() {
  SetEnabled(false);
}

void ExternalBeginFrameSourceAndroid::OnVSync(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jlong time_micros,
    jlong period_micros) {
  // Warning: It is generally unsafe to manufacture TimeTicks values. The
  // following assumption is being made, AND COULD EASILY BREAK AT ANY TIME:
  // Upstream, Java code is providing "System.nanos() / 1000," and this is the
  // same timestamp that would be provided by the CLOCK_MONOTONIC POSIX clock.
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  base::TimeTicks frame_time =
      base::TimeTicks() + base::Microseconds(time_micros);
  base::TimeDelta vsync_period(base::Microseconds(period_micros));
  // Calculate the next frame deadline:
  base::TimeTicks deadline = frame_time + vsync_period;

  auto begin_frame_args = begin_frame_args_generator_.GenerateBeginFrameArgs(
      source_id(), frame_time, deadline, vsync_period);
  OnBeginFrame(begin_frame_args);
}

void ExternalBeginFrameSourceAndroid::UpdateRefreshRate(float refresh_rate) {
  Java_ExternalBeginFrameSourceAndroid_updateRefreshRate(
      base::android::AttachCurrentThread(), j_object_, refresh_rate);
}

void ExternalBeginFrameSourceAndroid::SetDynamicBeginFrameDeadlineOffsetSource(
    DynamicBeginFrameDeadlineOffsetSource*
        dynamic_begin_frame_deadline_offset_source) {
  begin_frame_args_generator_.set_dynamic_begin_frame_deadline_offset_source(
      dynamic_begin_frame_deadline_offset_source);
}

void ExternalBeginFrameSourceAndroid::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  SetEnabled(needs_begin_frames);
}

void ExternalBeginFrameSourceAndroid::SetEnabled(bool enabled) {
  Java_ExternalBeginFrameSourceAndroid_setEnabled(
      base::android::AttachCurrentThread(), j_object_, enabled);
}

}  // namespace viz
