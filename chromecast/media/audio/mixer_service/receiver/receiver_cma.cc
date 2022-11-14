// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/receiver/receiver_cma.h"

#include <limits>
#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/audio/mixer_service/receiver/cma_backend_shim.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

constexpr base::TimeDelta kInactivityTimeout = base::Seconds(5);

enum MessageTypes : int {
  kPushResult = 1,
  kEndOfStream,
};

}  // namespace

class ReceiverCma::UnusedSocket : public MixerSocket::Delegate {
 public:
  UnusedSocket(ReceiverCma* receiver, std::unique_ptr<MixerSocket> socket)
      : receiver_(receiver), socket_(std::move(socket)) {
    DCHECK(receiver_);
    DCHECK(socket_);
    socket_->SetDelegate(this);
  }

  UnusedSocket(const UnusedSocket&) = delete;
  UnusedSocket& operator=(const UnusedSocket&) = delete;

  ~UnusedSocket() override = default;

 private:
  // MixerSocket::Delegate implementation:
  void OnConnectionError() override { receiver_->RemoveUnusedSocket(this); }

  ReceiverCma* const receiver_;
  const std::unique_ptr<MixerSocket> socket_;
};

class ReceiverCma::Stream : public MixerSocket::Delegate,
                            public CmaBackendShim::Delegate {
 public:
  Stream(ReceiverCma* receiver, std::unique_ptr<MixerSocket> socket)
      : receiver_(receiver), socket_(std::move(socket)), weak_factory_(this) {
    DCHECK(receiver_);
    DCHECK(socket_);

    socket_->SetDelegate(this);

    inactivity_timer_.Start(FROM_HERE, kInactivityTimeout, this,
                            &Stream::OnInactivityTimeout);
  }

  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;

  ~Stream() override = default;

  // MixerSocket::Delegate implementation:
  bool HandleMetadata(const mixer_service::Generic& message) override {
    last_receive_time_ = base::TimeTicks::Now();
    inactivity_timer_.Reset();

    if (message.has_output_stream_params()) {
      if (cma_audio_) {
        LOG(INFO) << "Received stream metadata after stream was already set up";
        return true;
      }

      pushed_eos_ = false;
      cma_audio_.reset(new mixer_service::CmaBackendShim(
          weak_factory_.GetWeakPtr(),
          base::SequencedTaskRunner::GetCurrentDefault(),
          message.output_stream_params(), receiver_->backend_manager()));
    }

    if (message.has_set_stream_volume()) {
      if (!cma_audio_) {
        LOG(INFO) << "Can't set volume before stream is set up";
        return true;
      }
      cma_audio_->SetVolumeMultiplier(message.set_stream_volume().volume());
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
      return true;
    }

    if (size == 0) {
      pushed_eos_ = true;
    }
    cma_audio_->AddData(data, size);
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
  void OnBufferPushed(CmaBackendShim::RenderingDelay rendering_delay) override {
    if (!pushed_eos_) {
      mixer_service::BufferPushResult message;
      message.set_delay_timestamp(rendering_delay.timestamp_microseconds);
      message.set_delay(rendering_delay.delay_microseconds);
      mixer_service::Generic generic;
      *(generic.mutable_push_result()) = message;
      socket_->SendProto(kPushResult, generic);
      last_send_time_ = base::TimeTicks::Now();
    }

    socket_->ReceiveMoreMessages();
  }

  void PlayedEos() override {
    LOG(INFO) << "EOS played for " << this;
    mixer_service::EosPlayedOut message;
    mixer_service::Generic generic;
    *generic.mutable_eos_played_out() = message;
    socket_->SendProto(kEndOfStream, generic);
    last_send_time_ = base::TimeTicks::Now();

    cma_audio_.reset();
  }

  void OnAudioPlaybackError() override {
    LOG(INFO) << "Audio playback error for " << this;
    receiver_->RemoveStream(this);
  }

  ReceiverCma* const receiver_;
  const std::unique_ptr<MixerSocket> socket_;

  base::OneShotTimer inactivity_timer_;

  std::unique_ptr<mixer_service::CmaBackendShim,
                  mixer_service::CmaBackendShim::Deleter>
      cma_audio_;
  bool pushed_eos_ = false;

  base::TimeTicks last_send_time_;
  base::TimeTicks last_receive_time_;

  base::WeakPtrFactory<Stream> weak_factory_;
};

ReceiverCma::ReceiverCma(MediaPipelineBackendManager* backend_manager)
    : backend_manager_(backend_manager) {
  DCHECK(backend_manager_);
}

ReceiverCma::~ReceiverCma() = default;

void ReceiverCma::CreateOutputStream(std::unique_ptr<MixerSocket> socket,
                                     const Generic& message) {
  auto stream = std::make_unique<Stream>(this, std::move(socket));
  Stream* ptr = stream.get();
  streams_[ptr] = std::move(stream);
  ptr->HandleMetadata(message);
}

void ReceiverCma::CreateLoopbackConnection(std::unique_ptr<MixerSocket> socket,
                                           const Generic& message) {
  LOG(INFO) << "Unhandled loopback connection";
  AddUnusedSocket(std::move(socket));
}

void ReceiverCma::CreateAudioRedirection(std::unique_ptr<MixerSocket> socket,
                                         const Generic& message) {
  LOG(INFO) << "Unhandled redirection connection";
  AddUnusedSocket(std::move(socket));
}

void ReceiverCma::CreateControlConnection(std::unique_ptr<MixerSocket> socket,
                                          const Generic& message) {
  LOG(INFO) << "Unhandled control connection";
  AddUnusedSocket(std::move(socket));
}

void ReceiverCma::RemoveStream(Stream* stream) {
  streams_.erase(stream);
}

void ReceiverCma::AddUnusedSocket(std::unique_ptr<MixerSocket> socket) {
  auto unused_socket = std::make_unique<UnusedSocket>(this, std::move(socket));
  UnusedSocket* ptr = unused_socket.get();
  unused_sockets_[ptr] = std::move(unused_socket);
}

void ReceiverCma::RemoveUnusedSocket(UnusedSocket* unused_socket) {
  unused_sockets_.erase(unused_socket);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
