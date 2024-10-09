// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rtp_stream.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"

using media::cast::FrameSenderConfig;
using media::cast::RtpPayloadType;

namespace mirroring {

VideoRtpStream::VideoRtpStream(
    std::unique_ptr<media::cast::VideoSender> video_sender,
    base::WeakPtr<RtpStreamClient> client,
    base::TimeDelta refresh_interval)
    : video_sender_(std::move(video_sender)),
      client_(client),
      refresh_interval_(refresh_interval) {
  DCHECK(video_sender_);
  DCHECK(client);

  if (refresh_interval_.is_positive()) {
    refresh_timer_.Start(
        FROM_HERE, refresh_interval_,
        base::BindRepeating(&VideoRtpStream::OnRefreshTimerFired,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

VideoRtpStream::~VideoRtpStream() = default;

void VideoRtpStream::InsertVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(client_);
  if (!video_frame->metadata().reference_time) {
    client_->OnError("Missing REFERENCE_TIME.");
    return;
  }

  // If the refresh timer isn't running when we receive a frame, restart it.
  if (refresh_interval_.is_positive() && !refresh_timer_.IsRunning()) {
    refresh_timer_.Reset();
  }

  base::TimeTicks reference_time = *video_frame->metadata().reference_time;
  DCHECK(!reference_time.is_null());
  if (expecting_a_refresh_frame_) {
    // There is uncertainty as to whether the video frame was in response to a
    // refresh request.  However, if it was not, more video frames will soon
    // follow, and before the refresh timer can fire again.  Thus, the
    // behavior resulting from this logic will be correct.
    expecting_a_refresh_frame_ = false;
  } else if (refresh_interval_.is_positive()) {
    // The following re-starts the timer, scheduling it to fire at
    // kRefreshInterval from now.
    refresh_timer_.Reset();
  }

  if (!(video_frame->format() == media::PIXEL_FORMAT_I420 ||
        video_frame->format() == media::PIXEL_FORMAT_NV12 ||
        video_frame->format() == media::PIXEL_FORMAT_YV12 ||
        video_frame->format() == media::PIXEL_FORMAT_I420A)) {
    client_->OnError("Incompatible video frame format.");
    return;
  }

  // Used by chrome/browser/media/cast_mirroring_performance_browsertest.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "ConsumeVideoFrame",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       (reference_time - base::TimeTicks()).InMicroseconds(),
                       "time_delta", video_frame->timestamp().InMicroseconds());

  video_sender_->InsertRawVideoFrame(std::move(video_frame), reference_time);
}

void VideoRtpStream::SetTargetPlayoutDelay(base::TimeDelta playout_delay) {
  video_sender_->SetTargetPlayoutDelay(playout_delay);
}

base::TimeDelta VideoRtpStream::GetTargetPlayoutDelay() const {
  return video_sender_->GetTargetPlayoutDelay();
}

void VideoRtpStream::OnRefreshTimerFired() {
  if (expecting_a_refresh_frame_) {
    // This means we requested a refresh frame, but never received it. This may
    // happen if the capturer is in a paused state. So, we should stop the
    // timer. The timer will restart the next time Reset() is called.
    refresh_timer_.Stop();
    return;
  }
  DVLOG(1) << "VideoRtpStream is requesting another refresh frame.";
  expecting_a_refresh_frame_ = true;
  client_->RequestRefreshFrame();
}

//------------------------------------------------------------------
// AudioRtpStream
//------------------------------------------------------------------

AudioRtpStream::AudioRtpStream(
    std::unique_ptr<media::cast::AudioSender> audio_sender,
    base::WeakPtr<RtpStreamClient> client)
    : audio_sender_(std::move(audio_sender)), client_(std::move(client)) {
  DCHECK(audio_sender_);
  DCHECK(client_);
}

AudioRtpStream::~AudioRtpStream() = default;

void AudioRtpStream::InsertAudio(std::unique_ptr<media::AudioBus> audio_bus,
                                 base::TimeTicks capture_time) {
  audio_sender_->InsertAudio(std::move(audio_bus), capture_time);
}

void AudioRtpStream::SetTargetPlayoutDelay(base::TimeDelta playout_delay) {
  audio_sender_->SetTargetPlayoutDelay(playout_delay);
}

base::TimeDelta AudioRtpStream::GetTargetPlayoutDelay() const {
  return audio_sender_->GetTargetPlayoutDelay();
}

int AudioRtpStream::GetEncoderBitrate() const {
  return audio_sender_->GetEncoderBitrate();
}

}  // namespace mirroring
