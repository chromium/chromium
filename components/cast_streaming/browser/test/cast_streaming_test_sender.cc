// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_BROWSER_TEST_CAST_STREAMING_SENDER_SESSION_TEST_H_
#define COMPONENTS_CAST_STREAMING_BROWSER_TEST_CAST_STREAMING_SENDER_SESSION_TEST_H_

#include "components/cast_streaming/browser/test/cast_streaming_test_sender.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/cast_streaming/public/config_conversions.h"
#include "third_party/openscreen/src/cast/streaming/capture_recommendations.h"

namespace cast_streaming {

namespace {

const char kSenderId[] = "testSenderId";
const char kReceiverId[] = "testReceiverId";

// Converts |data_buffer| into an Open Screen EncodedFrame. The caller must keep
// a reference to |data_buffer| while using the returned EncodedFrame since they
// point to the same data.
openscreen::cast::EncodedFrame DataBufferToEncodedFrame(
    const scoped_refptr<media::DataBuffer>& data_buffer,
    bool is_key_frame,
    openscreen::cast::FrameId frame_id,
    openscreen::cast::FrameId* last_referenced_frame_id,
    int rtp_timebase) {
  CHECK(data_buffer);
  CHECK(!data_buffer->end_of_stream());

  openscreen::cast::EncodedFrame encoded_frame;
  encoded_frame.frame_id = frame_id;
  if (is_key_frame) {
    encoded_frame.dependency =
        openscreen::cast::EncodedFrame::Dependency::KEY_FRAME;
    *last_referenced_frame_id = encoded_frame.frame_id;
  } else {
    encoded_frame.dependency =
        openscreen::cast::EncodedFrame::Dependency::DEPENDS_ON_ANOTHER;
  }
  encoded_frame.referenced_frame_id = *last_referenced_frame_id;

  std::chrono::milliseconds timestamp(
      data_buffer->timestamp().InMilliseconds());
  encoded_frame.rtp_timestamp =
      openscreen::cast::RtpTimeTicks::FromTimeSinceOrigin<
          std::chrono::milliseconds>(timestamp, rtp_timebase);
  encoded_frame.reference_time = openscreen::Clock::time_point(timestamp);

  encoded_frame.data = absl::Span<uint8_t>(data_buffer->writable_data(),
                                           data_buffer->data_size());

  return encoded_frame;
}

}  // namespace

CastStreamingTestSender::CastStreamingTestSender()
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      environment_(&openscreen::Clock::now, &task_runner_) {}

CastStreamingTestSender::~CastStreamingTestSender() = default;

bool CastStreamingTestSender::Start(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    net::IPAddress receiver_address,
    absl::optional<media::AudioDecoderConfig> audio_config,
    absl::optional<media::VideoDecoderConfig> video_config) {
  VLOG(1) << __func__;
  CHECK(!is_active_);
  CHECK(!sender_session_);
  CHECK(audio_config || video_config);

  // Instantiate the |sender_session_|.
  message_port_ = std::make_unique<CastMessagePortSenderImpl>(
      std::move(message_port),
      base::BindOnce(&CastStreamingTestSender::OnCastChannelClosed,
                     base::Unretained(this)));
  sender_session_ = std::make_unique<openscreen::cast::SenderSession>(
      openscreen::cast::SenderSession::Configuration{
          openscreen::IPAddress::kV6LoopbackAddress(), this, &environment_,
          message_port_.get(), kSenderId, kReceiverId,
          true /* use_android_rtp_hack */});

  std::vector<openscreen::cast::AudioCaptureConfig> audio_configs;
  if (audio_config) {
    audio_configs.push_back(ToAudioCaptureConfig(audio_config.value()));
  }

  std::vector<openscreen::cast::VideoCaptureConfig> video_configs;
  if (video_config) {
    video_configs.push_back(ToVideoCaptureConfig(video_config.value()));
  }

  openscreen::Error error = sender_session_->Negotiate(
      std::move(audio_configs), std::move(video_configs));

  if (error == openscreen::Error::None()) {
    return true;
  }

  LOG(ERROR) << "Failed to start sender session. " << error.ToString();
  sender_session_.reset();
  message_port_.reset();
  return false;
}

void CastStreamingTestSender::Stop() {
  VLOG(1) << __func__;

  sender_session_.reset();
  message_port_.reset();
  audio_sender_ = nullptr;
  video_sender_ = nullptr;
  audio_decoder_config_.reset();
  video_decoder_config_.reset();
  is_active_ = false;

  if (sender_stopped_closure_) {
    std::move(sender_stopped_closure_).Run();
  }
}

void CastStreamingTestSender::SendAudioBuffer(
    scoped_refptr<media::DataBuffer> audio_buffer) {
  VLOG(3) << __func__;
  CHECK(audio_sender_);

  if (audio_sender_->EnqueueFrame(DataBufferToEncodedFrame(
          audio_buffer, true /* is_key_frame */,
          audio_sender_->GetNextFrameId(), &last_audio_reference_frame_id_,
          audio_sender_->rtp_timebase())) !=
      openscreen::cast::Sender::EnqueueFrameResult::OK) {
    Stop();
  }
}

void CastStreamingTestSender::SendVideoBuffer(
    scoped_refptr<media::DataBuffer> video_buffer,
    bool is_key_frame) {
  VLOG(3) << __func__;
  CHECK(video_sender_);

  if (video_sender_->EnqueueFrame(DataBufferToEncodedFrame(
          video_buffer, is_key_frame, video_sender_->GetNextFrameId(),
          &last_video_reference_frame_id_, video_sender_->rtp_timebase())) !=
      openscreen::cast::Sender::EnqueueFrameResult::OK) {
    Stop();
  }
}

void CastStreamingTestSender::RunUntilStarted() {
  VLOG(1) << __func__;
  while (!is_active_) {
    base::RunLoop run_loop;
    CHECK(!sender_started_closure_);
    sender_started_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void CastStreamingTestSender::RunUntilStopped() {
  VLOG(1) << __func__;
  while (is_active_) {
    base::RunLoop run_loop;
    CHECK(!sender_stopped_closure_);
    sender_stopped_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void CastStreamingTestSender::OnCastChannelClosed() {
  VLOG(1) << __func__;
  Stop();
}

void CastStreamingTestSender::OnNegotiated(
    const openscreen::cast::SenderSession* session,
    openscreen::cast::SenderSession::ConfiguredSenders senders,
    openscreen::cast::capture_recommendations::Recommendations
        capture_recommendations) {
  VLOG(1) << __func__;
  CHECK_EQ(session, sender_session_.get());
  CHECK(senders.audio_sender || senders.video_sender);

  if (senders.audio_sender) {
    audio_sender_ = senders.audio_sender;
    audio_decoder_config_ = ToAudioDecoderConfig(senders.audio_config);
  }

  if (senders.video_sender) {
    video_sender_ = senders.video_sender;
    video_decoder_config_ = ToVideoDecoderConfig(senders.video_config);
  }

  is_active_ = true;
  if (sender_started_closure_) {
    std::move(sender_started_closure_).Run();
  }
}

void CastStreamingTestSender::OnError(
    const openscreen::cast::SenderSession* session,
    openscreen::Error error) {
  LOG(ERROR) << "Sender Session error: " << error.ToString();
  CHECK_EQ(session, sender_session_.get());
  Stop();
}

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_BROWSER_TEST_CAST_STREAMING_SENDER_SESSION_TEST_H_
