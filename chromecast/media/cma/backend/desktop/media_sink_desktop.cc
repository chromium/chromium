// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/desktop/media_sink_desktop.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

MediaSinkDesktop::MediaSinkDesktop(
    MediaPipelineBackend::Decoder::Delegate* delegate,
    base::TimeDelta start_pts)
    : delegate_(delegate),
      time_interpolator_(&tick_clock_),
      playback_rate_(1.0f),
      last_frame_pts_(start_pts),
      received_eos_(false) {
  DCHECK(delegate_);
  time_interpolator_.SetPlaybackRate(playback_rate_);
  time_interpolator_.SetBounds(start_pts, start_pts, tick_clock_.NowTicks());
  time_interpolator_.StartInterpolating();
}

MediaSinkDesktop::~MediaSinkDesktop() {}

void MediaSinkDesktop::SetPlaybackRate(float rate) {
  DCHECK_GE(rate, 0.0f);
  playback_rate_ = rate;
  time_interpolator_.SetPlaybackRate(playback_rate_);

  // Changing the playback rate affects the delay after which EOS callback
  // should run. Reschedule the task according to the new delay.
  if (received_eos_) {
    eos_task_.Cancel();
    ScheduleEndOfStreamTask();
  }
}

base::TimeDelta MediaSinkDesktop::GetCurrentPts() {
  return time_interpolator_.GetInterpolatedTime();
}

MediaPipelineBackend::BufferStatus MediaSinkDesktop::PushBuffer(
    CastDecoderBuffer* buffer) {
  if (buffer->end_of_stream()) {
    received_eos_ = true;
    ScheduleEndOfStreamTask();
    return MediaPipelineBackend::kBufferSuccess;
  }

  // This is wrong on several levels.
  // 1. The correct PTS should be buffer->timestamp() + buffer->duration().
  //    But CastDecoderBuffer does not expose duration unlike
  //    ::media::DecoderBuffer.
  // 2. The PTS reported by GetCurrentPts should not move backwards.
  //    It should be clamped in the range [start_pts, last_frame_pts_].
  //    But doing so makes several AudioVideoPipelineDeviceTest cases fail.
  //    Those tests are wrong should be fixed.
  // TODO(alokp): Fix these issues when the next version of CMA backend is
  // scheduled to roll out. crbug.com/678394
  auto timestamp = base::Microseconds(buffer->timestamp());
  if (timestamp != ::media::kNoTimestamp) {
    last_frame_pts_ = timestamp;
    time_interpolator_.SetUpperBound(last_frame_pts_);
  }
  return MediaPipelineBackend::kBufferSuccess;
}

void MediaSinkDesktop::ScheduleEndOfStreamTask() {
  DCHECK(received_eos_);
  DCHECK(eos_task_.IsCancelled());

  // Do not schedule if playback is paused.
  if (playback_rate_ == 0.0f)
    return;

  eos_task_.Reset(
      base::BindOnce(&MediaPipelineBackend::Decoder::Delegate::OnEndOfStream,
                     base::Unretained(delegate_)));
  base::TimeDelta delay = (last_frame_pts_ - GetCurrentPts()) / playback_rate_;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, eos_task_.callback(), delay);
}

}  // namespace media
}  // namespace chromecast
