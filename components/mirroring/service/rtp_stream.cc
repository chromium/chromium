// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/rtp_stream.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/sender/audio_sender.h"
#include "media/cast/sender/video_sender.h"

using media::cast::FrameSenderConfig;
using media::cast::RtpPayloadType;

namespace mirroring {

namespace {

// The maximum time since the last video frame was received from the video
// source, before requesting refresh frames.
constexpr base::TimeDelta kRefreshInterval =
    base::TimeDelta::FromMilliseconds(250);

// The maximum number of refresh video frames to request/receive.  After this
// limit (60 * 250ms = 15 seconds), refresh frame requests will stop being made.
constexpr int kMaxConsecutiveRefreshFrames = 60;

}  // namespace

VideoRtpStream::VideoRtpStream(
    std::unique_ptr<media::cast::VideoSender> video_sender,
    base::WeakPtr<RtpStreamClient> client)
    : video_sender_(std::move(video_sender)),
      client_(client),
      consecutive_refresh_count_(0),
      expecting_a_refresh_frame_(false) {
  DCHECK(video_sender_);
  DCHECK(client);

  refresh_timer_.Start(FROM_HERE, kRefreshInterval,
                       base::BindRepeating(&VideoRtpStream::OnRefreshTimerFired,
                                           this->AsWeakPtr()));
}

VideoRtpStream::~VideoRtpStream() {}

void VideoRtpStream::InsertVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(client_);
  if (!video_frame->metadata()->reference_time.has_value()) {
    client_->OnError("Missing REFERENCE_TIME.");
    return;
  }

  base::TimeTicks reference_time = *video_frame->metadata()->reference_time;
  DCHECK(!reference_time.is_null());
  if (expecting_a_refresh_frame_) {
    // There is uncertainty as to whether the video frame was in response to a
    // refresh request.  However, if it was not, more video frames will soon
    // follow, and before the refresh timer can fire again.  Thus, the
    // behavior resulting from this logic will be correct.
    expecting_a_refresh_frame_ = false;
  } else {
    consecutive_refresh_count_ = 0;
    // The following re-starts the timer, scheduling it to fire at
    // kRefreshInterval from now.
    refresh_timer_.Reset();
  }

  if (!(video_frame->format() == media::PIXEL_FORMAT_I420 ||
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

void VideoRtpStream::OnRefreshTimerFired() {
  ++consecutive_refresh_count_;
  if (consecutive_refresh_count_ >= kMaxConsecutiveRefreshFrames)
    refresh_timer_.Stop();  // Stop timer until receiving a non-refresh frame.

  DVLOG(1) << "CastVideoSink is requesting another refresh frame "
              "(consecutive count="
           << consecutive_refresh_count_ << ").";
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

AudioRtpStream::~AudioRtpStream() {}

void AudioRtpStream::InsertAudio(std::unique_ptr<media::AudioBus> audio_bus,
                                 const base::TimeTicks& capture_time) {
  audio_sender_->InsertAudio(std::move(audio_bus), capture_time);
}

void AudioRtpStream::SetTargetPlayoutDelay(base::TimeDelta playout_delay) {
  audio_sender_->SetTargetPlayoutDelay(playout_delay);
}

}  // namespace mirroring
