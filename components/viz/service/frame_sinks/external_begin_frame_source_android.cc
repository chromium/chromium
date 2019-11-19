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
  // TODO(ericrk): This logic is ported from window_android.cc. Once OOP-D
  // conversion is complete, we can delete the logic there.

  // Warning: It is generally unsafe to manufacture TimeTicks values. The
  // following assumption is being made, AND COULD EASILY BREAK AT ANY TIME:
  // Upstream, Java code is providing "System.nanos() / 1000," and this is the
  // same timestamp that would be provided by the CLOCK_MONOTONIC POSIX clock.
  DCHECK_EQ(base::TimeTicks::GetClock(),
            base::TimeTicks::Clock::LINUX_CLOCK_MONOTONIC);
  base::TimeTicks frame_time =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(time_micros);
  base::TimeDelta vsync_period(
      base::TimeDelta::FromMicroseconds(period_micros));

  uint64_t sequence_number = next_sequence_number_;
  // We expect |sequence_number| to be the number for the frame at
  // |expected_frame_time|. We adjust this sequence number according to the
  // actual frame time in case it is later than expected.
  if (next_expected_frame_time_ != base::TimeTicks()) {
    // Add |error_margin| to round |frame_time| up to the next tick if it is
    // close to the end of an interval. This happens when a timebase is a bit
    // off because of an imperfect presentation timestamp that may be a bit
    // later than the beginning of the next interval.
    constexpr double kErrorMarginIntervalPct = 0.05;
    base::TimeDelta error_margin = vsync_period * kErrorMarginIntervalPct;
    int ticks_since_estimated_frame_time =
        (frame_time + error_margin - next_expected_frame_time_) / vsync_period;
    sequence_number += std::max(0, ticks_since_estimated_frame_time);
  }

  // Calculate the next frame deadline:
  base::TimeTicks deadline = frame_time + vsync_period;
  auto begin_frame_args = BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, source_id(), sequence_number, frame_time, deadline,
      vsync_period, BeginFrameArgs::NORMAL);
  next_sequence_number_ = sequence_number + 1;
  next_expected_frame_time_ = deadline;

  OnBeginFrame(begin_frame_args);
}

void ExternalBeginFrameSourceAndroid::UpdateRefreshRate(float refresh_rate) {
  Java_ExternalBeginFrameSourceAndroid_updateRefreshRate(
      base::android::AttachCurrentThread(), j_object_, refresh_rate);
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
