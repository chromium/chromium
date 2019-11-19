// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/cast_receiver_session_delegate.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"

CastReceiverSessionDelegate::CastReceiverSessionDelegate() {}
CastReceiverSessionDelegate::~CastReceiverSessionDelegate() {}

void CastReceiverSessionDelegate::Start(
    const media::cast::FrameReceiverConfig& audio_config,
    const media::cast::FrameReceiverConfig& video_config,
    const net::IPEndPoint& local_endpoint,
    const net::IPEndPoint& remote_endpoint,
    std::unique_ptr<base::DictionaryValue> options,
    const media::VideoCaptureFormat& format,
    const ErrorCallback& error_callback) {
  format_ = format;
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  CastSessionDelegateBase::StartUDP(local_endpoint, remote_endpoint,
                                    std::move(options), error_callback);
  cast_receiver_ = media::cast::CastReceiver::Create(cast_environment_,
                                                     audio_config,
                                                     video_config,
                                                     cast_transport_.get());
  on_audio_decoded_cb_ = base::Bind(
      &CastReceiverSessionDelegate::OnDecodedAudioFrame,
      weak_factory_.GetWeakPtr());
  on_video_decoded_cb_ = base::Bind(
      &CastReceiverSessionDelegate::OnDecodedVideoFrame,
      weak_factory_.GetWeakPtr());
}

void CastReceiverSessionDelegate::ReceivePacket(
    std::unique_ptr<media::cast::Packet> packet) {
  cast_receiver_->ReceivePacket(std::move(packet));
}

void CastReceiverSessionDelegate::StartAudio(
    scoped_refptr<CastReceiverAudioValve> audio_valve) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  audio_valve_ = audio_valve;
  cast_receiver_->RequestDecodedAudioFrame(on_audio_decoded_cb_);
}

void CastReceiverSessionDelegate::OnDecodedAudioFrame(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks playout_time,
    bool is_continuous) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!audio_valve_)
    return;

  // We're on the IO thread, which doesn't allow blocking
  // operations. Since we don't know what the Capture callback
  // will do exactly, we need to jump to a different thread.
  // Let's re-use the audio decoder thread.
  cast_environment_->PostTask(
      media::cast::CastEnvironment::AUDIO, FROM_HERE,
      base::Bind(&CastReceiverAudioValve::DeliverDecodedAudio, audio_valve_,
                 base::Owned(audio_bus.release()), playout_time));
  cast_receiver_->RequestDecodedAudioFrame(on_audio_decoded_cb_);
}

void CastReceiverSessionDelegate::StartVideo(
    blink::VideoCaptureDeliverFrameCB video_callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  frame_callback_ = video_callback;
  cast_receiver_->RequestDecodedVideoFrame(on_video_decoded_cb_);
}

void  CastReceiverSessionDelegate::StopVideo() {
  frame_callback_ = blink::VideoCaptureDeliverFrameCB();
}

void CastReceiverSessionDelegate::OnDecodedVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks playout_time,
    bool is_continuous) {
  if (frame_callback_.is_null())
    return;
  frame_callback_.Run(std::move(video_frame), playout_time);
  cast_receiver_->RequestDecodedVideoFrame(on_video_decoded_cb_);
}
