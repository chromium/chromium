// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/receiver/audio_output_service_receiver.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/media/api/cma_backend_factory.h"
#include "chromecast/media/audio/audio_output_service/constants.h"
#include "chromecast/media/audio/audio_output_service/output_socket.h"
#include "chromecast/media/audio/audio_output_service/receiver/cma_backend_shim.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

constexpr base::TimeDelta kInactivityTimeout = base::Seconds(5);

enum MessageTypes : int {
  kUpdateMediaTime = 1,
  kBackendInitialized = 2,
};

}  // namespace

class AudioOutputServiceReceiver::Stream
    : public audio_output_service::OutputSocket::Delegate,
      public audio_output_service::CmaBackendShim::Delegate {
 public:
  Stream(AudioOutputServiceReceiver* receiver,
         std::unique_ptr<OutputSocket> socket)
      : receiver_(receiver), socket_(std::move(socket)) {
    DCHECK(receiver_);
    DCHECK(socket_);

    socket_->SetDelegate(this);

    inactivity_timer_.Start(FROM_HERE, kInactivityTimeout, this,
                            &Stream::OnInactivityTimeout);
  }
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;
  ~Stream() override = default;

  // OutputSocket::Delegate implementation:
  bool HandleMetadata(const Generic& message) override {
    last_receive_time_ = base::TimeTicks::Now();
    inactivity_timer_.Reset();

    if (message.has_heartbeat()) {
      return true;
    }

    if (message.has_backend_params()) {
      if (cma_audio_) {
        LOG(INFO) << "Received stream metadata after initialization, there must"
                  << " be an update to the audio config.";
        cma_audio_->UpdateAudioConfig(message.backend_params());
        return true;
      }

      pushed_eos_ = false;
      cma_audio_.reset(new audio_output_service::CmaBackendShim(
          weak_factory_.GetWeakPtr(),
          base::SequencedTaskRunner::GetCurrentDefault(),
          receiver_->media_task_runner(), message.backend_params(),
          receiver_->cma_backend_factory()));
    }

    if (message.has_set_start_timestamp()) {
      if (!cma_audio_) {
        LOG(INFO) << "Can't start before stream is set up.";
        ResetConnection();
        return false;
      }
      cma_audio_->StartPlayingFrom(message.set_start_timestamp().start_pts());
    }

    if (message.has_stop_playback()) {
      if (!cma_audio_) {
        LOG(INFO) << "Can't stop before stream is set up.";
        ResetConnection();
        return false;
      }
      cma_audio_->Stop();
    }

    if (message.has_set_stream_volume()) {
      if (!cma_audio_) {
        LOG(INFO) << "Can't set volume before stream is set up.";
        ResetConnection();
        return false;
      }
      cma_audio_->SetVolumeMultiplier(message.set_stream_volume().volume());
    }

    if (message.has_set_playback_rate()) {
      if (!cma_audio_) {
        LOG(INFO) << "Can't set playback rate before stream is set up.";
        ResetConnection();
        return false;
      }
      cma_audio_->SetPlaybackRate(message.set_playback_rate().playback_rate());
    }

    if (message.has_eos_played_out()) {
      // Explicit EOS.
      return HandleAudioData(nullptr, 0, INT64_MIN);
    }

    return true;
  }

  bool HandleAudioData(char* data, size_t size, int64_t timestamp) override {
    last_receive_time_ = base::TimeTicks::Now();
    inactivity_timer_.Reset();

    if (!cma_audio_) {
      LOG(INFO) << "Received audio before stream metadata; ignoring";
      ResetConnection();
      return false;
    }

    if (size == 0) {
      pushed_eos_ = true;
    }
    cma_audio_->AddData(data, size, timestamp);
    stopped_receiving_ = true;
    return false;  // Don't receive any more messages until the buffer is
                   // pushed.
  }

  void OnConnectionError() override {
    LOG(INFO) << "Connection lost for " << this;
    receiver_->RemoveStream(this);
  }

 private:
  void OnInactivityTimeout() {
    LOG(INFO) << "Timed out " << this
              << " due to inactivity; now = " << base::TimeTicks::Now()
              << ", last send = " << last_send_time_
              << ", last receive = " << last_receive_time_;
    receiver_->RemoveStream(this);
  }

  // CmaBackendShim::Delegate implementation:
  void OnBackendInitialized(bool success) override {
    audio_output_service::Generic generic;
    generic.mutable_backend_initialization_status()->set_status(
        success ? audio_output_service::BackendInitializationStatus::SUCCESS
                : audio_output_service::BackendInitializationStatus::ERROR);
    socket_->SendProto(kBackendInitialized, generic);
    last_send_time_ = base::TimeTicks::Now();
  }

  void OnBufferPushed() override {
    if (!stopped_receiving_) {
      return;
    }
    stopped_receiving_ = false;
    socket_->ReceiveMoreMessages();
  }

  void UpdateMediaTimeAndRenderingDelay(
      int64_t media_timestamp_microseconds,
      int64_t reference_timestamp_microseconds,
      int64_t delay_microseconds,
      int64_t delay_timestamp_microseconds) override {
    audio_output_service::CurrentMediaTimestamp message;
    message.set_media_timestamp_microseconds(media_timestamp_microseconds);
    message.set_reference_timestamp_microseconds(
        reference_timestamp_microseconds);
    message.set_delay_microseconds(delay_microseconds);
    message.set_delay_timestamp_microseconds(delay_timestamp_microseconds);
    audio_output_service::Generic generic;
    *(generic.mutable_current_media_timestamp()) = message;
    socket_->SendProto(kUpdateMediaTime, generic);
    last_send_time_ = base::TimeTicks::Now();
  }

  void OnAudioPlaybackError() override {
    LOG(INFO) << "Audio playback error for " << this;
    receiver_->RemoveStream(this);
  }

  void ResetConnection() {
    // Reset will cause the deletion of |this|, so it is better to post a task.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&Stream::OnAudioPlaybackError,
                                  weak_factory_.GetWeakPtr()));
  }

  AudioOutputServiceReceiver* const receiver_;
  const std::unique_ptr<OutputSocket> socket_;

  base::OneShotTimer inactivity_timer_;

  std::unique_ptr<audio_output_service::CmaBackendShim,
                  audio_output_service::CmaBackendShim::Deleter>
      cma_audio_;
  bool pushed_eos_ = false;
  bool stopped_receiving_ = false;

  base::TimeTicks last_send_time_;
  base::TimeTicks last_receive_time_;

  base::WeakPtrFactory<Stream> weak_factory_{this};
};

AudioOutputServiceReceiver::AudioOutputServiceReceiver(
    CmaBackendFactory* cma_backend_factory,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
    : Receiver(
          audio_output_service::kDefaultAudioOutputServiceUnixDomainSocketPath,
          audio_output_service::kDefaultAudioOutputServiceTcpPort),
      cma_backend_factory_(cma_backend_factory),
      media_task_runner_(std::move(media_task_runner)) {
  DCHECK(cma_backend_factory_);
  DCHECK(media_task_runner_);
}

AudioOutputServiceReceiver::~AudioOutputServiceReceiver() = default;

void AudioOutputServiceReceiver::CreateOutputStream(
    std::unique_ptr<OutputSocket> socket,
    const Generic& message) {
  auto stream = std::make_unique<Stream>(this, std::move(socket));
  Stream* ptr = stream.get();
  streams_[ptr] = std::move(stream);
  ptr->HandleMetadata(message);
}

void AudioOutputServiceReceiver::RemoveStream(Stream* stream) {
  streams_.erase(stream);
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
