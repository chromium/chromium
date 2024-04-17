// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/test/cast_streaming_test_sender.h"

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/test/cast_message_port_sender_impl.h"
#include "media/cast/openscreen/config_conversions.h"
#include "third_party/openscreen/src/platform/base/span.h"

namespace cast_streaming {

namespace {

const char kSenderId[] = "testSenderId";
const char kReceiverId[] = "testReceiverId";

// Converts |decoder_buffer| into an Open Screen EncodedFrame.
openscreen::cast::EncodedFrame DecoderBufferToEncodedFrame(
    const media::DecoderBuffer* decoder_buffer,
    openscreen::cast::FrameId frame_id,
    openscreen::cast::FrameId* last_referenced_frame_id,
    int rtp_timebase) {
  CHECK(decoder_buffer);
  CHECK(!decoder_buffer->end_of_stream());

  openscreen::cast::EncodedFrame encoded_frame;
  encoded_frame.frame_id = frame_id;
  if (decoder_buffer->is_key_frame()) {
    encoded_frame.dependency =
        openscreen::cast::EncodedFrame::Dependency::kKeyFrame;
    *last_referenced_frame_id = encoded_frame.frame_id;
  } else {
    encoded_frame.dependency =
        openscreen::cast::EncodedFrame::Dependency::kDependent;
  }
  encoded_frame.referenced_frame_id = *last_referenced_frame_id;

  std::chrono::milliseconds timestamp(
      decoder_buffer->timestamp().InMilliseconds());
  encoded_frame.rtp_timestamp =
      openscreen::cast::RtpTimeTicks::FromTimeSinceOrigin<
          std::chrono::milliseconds>(timestamp, rtp_timebase);
  encoded_frame.reference_time = openscreen::Clock::time_point(timestamp);

  encoded_frame.data = openscreen::ByteView(decoder_buffer->writable_data(),
                                            decoder_buffer->size());

  return encoded_frame;
}

}  // namespace

class CastStreamingTestSender::SenderObserver final
    : public openscreen::cast::Sender::Observer {
 public:
  explicit SenderObserver(std::unique_ptr<openscreen::cast::Sender> sender)
      : sender_(std::move(sender)) {
    CHECK(sender_);
    sender_->SetObserver(this);
  }
  ~SenderObserver() override { sender_->SetObserver(nullptr); }

  SenderObserver(const SenderObserver&) = delete;
  SenderObserver& operator=(const SenderObserver&) = delete;

  bool EnqueueBuffer(scoped_refptr<media::DecoderBuffer> buffer) {
    VLOG(3) << __func__;
    openscreen::cast::FrameId frame_id = sender_->GetNextFrameId();
    openscreen::cast::Sender::EnqueueFrameResult result =
        sender_->EnqueueFrame(DecoderBufferToEncodedFrame(
            buffer.get(), frame_id, &last_reference_frame_id_,
            sender_->rtp_timebase()));

    if (result != openscreen::cast::Sender::EnqueueFrameResult::OK) {
      LOG(ERROR) << "Failed to enqueue buffer " << result;
      return false;
    }

    buffer_map_.emplace(frame_id, std::move(buffer));
    return true;
  }

 private:
  // openscreen::cast::Sender::Observer implementation.
  void OnFrameCanceled(openscreen::cast::FrameId frame_id) final {
    VLOG(3) << __func__ << ". frame_id: " << frame_id;
    buffer_map_.erase(frame_id);
  }
  void OnPictureLost() final {}

  openscreen::cast::FrameId last_reference_frame_id_;
  std::unique_ptr<openscreen::cast::Sender> sender_;
  base::flat_map<openscreen::cast::FrameId, scoped_refptr<media::DecoderBuffer>>
      buffer_map_;
};

CastStreamingTestSender::CastStreamingTestSender()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      environment_(&openscreen::Clock::now,
                   task_runner_,
                   openscreen::IPEndpoint::kAnyV4()) {}

CastStreamingTestSender::~CastStreamingTestSender() = default;

void CastStreamingTestSender::Start(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    net::IPAddress receiver_address,
    std::optional<media::AudioDecoderConfig> audio_config,
    std::optional<media::VideoDecoderConfig> video_config) {
  VLOG(1) << __func__;
  CHECK(!has_startup_completed_);
  CHECK(!sender_session_);
  CHECK(audio_config || video_config);

  // Instantiate the |sender_session_|.
  message_port_ = std::make_unique<CastMessagePortSenderImpl>(
      std::move(message_port),
      base::BindOnce(&CastStreamingTestSender::OnCastChannelClosed,
                     base::Unretained(this)),
      base::BindOnce(&CastStreamingTestSender::OnSystemSenderMessageReceived,
                     base::Unretained(this)));
  sender_session_ = std::make_unique<openscreen::cast::SenderSession>(
      openscreen::cast::SenderSession::Configuration{
          openscreen::IPAddress::kV4LoopbackAddress(), *this, &environment_,
          message_port_.get(), kSenderId, kReceiverId,
          true /* use_android_rtp_hack */});

  if (audio_config) {
    audio_configs_.push_back(
        media::cast::ToAudioCaptureConfig(audio_config.value()));
  }

  if (video_config) {
    video_configs_.push_back(
        media::cast::ToVideoCaptureConfig(video_config.value()));
  }
}

void CastStreamingTestSender::Stop() {
  VLOG(1) << __func__;

  // Senders must be deconstructed before the session that hosts them.
  audio_sender_observer_.reset();
  video_sender_observer_.reset();

  // Disconnect the message port before destructing its client.
  if (message_port_) {
    // TODO(crbug.com/42050578): CastMessagePortSender should be RAII and clean
    // itself during the destruction instead of relying the client to call its
    // ResetClient function.
    message_port_->ResetClient();
  }
  sender_session_.reset();
  message_port_.reset();

  audio_decoder_config_.reset();
  video_decoder_config_.reset();
  has_startup_completed_ = false;

  if (sender_stopped_closure_) {
    std::move(sender_stopped_closure_).Run();
  }
}

void CastStreamingTestSender::SendAudioBuffer(
    scoped_refptr<media::DecoderBuffer> audio_buffer) {
  VLOG(3) << __func__;
  CHECK(audio_sender_observer_);

  if (!audio_sender_observer_->EnqueueBuffer(audio_buffer)) {
    // The error has already been logged in EnqueueBuffer().
    Stop();
  }
}

void CastStreamingTestSender::SendVideoBuffer(
    scoped_refptr<media::DecoderBuffer> video_buffer) {
  VLOG(3) << __func__;
  CHECK(video_sender_observer_);

  if (!video_sender_observer_->EnqueueBuffer(video_buffer)) {
    // The error has already been logged in EnqueueBuffer().
    Stop();
  }
}

bool CastStreamingTestSender::RunUntilActive() {
  VLOG(1) << __func__;
  while (!has_startup_completed_) {
    base::RunLoop run_loop;
    CHECK(!sender_started_closure_);
    sender_started_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  return !!sender_session_;
}

void CastStreamingTestSender::RunUntilStopped() {
  VLOG(1) << __func__;
  while (has_startup_completed_) {
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

void CastStreamingTestSender::OnSystemSenderMessageReceived() {
  openscreen::Error error = sender_session_->Negotiate(
      std::move(audio_configs_), std::move(video_configs_));

  if (error != openscreen::Error::None()) {
    LOG(ERROR) << "Failed to start sender session. " << error.ToString();
    sender_session_.reset();
    message_port_.reset();

    has_startup_completed_ = true;
    if (sender_started_closure_) {
      std::move(sender_started_closure_).Run();
    }
  }
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
    audio_sender_observer_ =
        std::make_unique<SenderObserver>(std::move(senders.audio_sender));
    audio_decoder_config_ =
        media::cast::ToAudioDecoderConfig(senders.audio_config);
  }

  if (senders.video_sender) {
    video_sender_observer_ =
        std::make_unique<SenderObserver>(std::move(senders.video_sender));
    video_decoder_config_ =
        media::cast::ToVideoDecoderConfig(senders.video_config);
  }

  has_startup_completed_ = true;
  if (sender_started_closure_) {
    std::move(sender_started_closure_).Run();
  }
}

void CastStreamingTestSender::OnError(
    const openscreen::cast::SenderSession* session,
    const openscreen::Error& error) {
  LOG(ERROR) << "Sender Session error: " << error.ToString();
  CHECK_EQ(session, sender_session_.get());
  Stop();
}

}  // namespace cast_streaming
