// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_wrapper.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/public/media/cast_decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {

// This revoked AudioDecoder would behave as one with buffer filled up and
// doesn't advance |statistics| and doesn't change RenderingDelay.
class RevokedAudioDecoderWrapper : public DestructableAudioDecoder {
 public:
  RevokedAudioDecoderWrapper(RenderingDelay rendering_delay,
                             Statistics statistics,
                             bool requires_decryption)
      : rendering_delay_(rendering_delay),
        statistics_(statistics),
        requires_decryption_(requires_decryption) {}
  ~RevokedAudioDecoderWrapper() override = default;

 private:
  // DestructableAudioDecoder implementation:
  void OnInitialized() override {}
  void SetDelegate(Delegate* delegate) override {}
  BufferStatus PushBuffer(scoped_refptr<DecoderBufferBase> buffer) override {
    return MediaPipelineBackend::kBufferPending;
  }
  bool SetConfig(const AudioConfig& config) override { return true; }
  bool SetVolume(float multiplier) override { return true; }
  RenderingDelay GetRenderingDelay() override { return rendering_delay_; }
  void GetStatistics(Statistics* statistics) override {
    *statistics = statistics_;
  }
  bool RequiresDecryption() override { return requires_decryption_; }

  const RenderingDelay rendering_delay_;
  const Statistics statistics_;
  const bool requires_decryption_;

  DISALLOW_COPY_AND_ASSIGN(RevokedAudioDecoderWrapper);
};

}  // namespace

ActiveAudioDecoderWrapper::ActiveAudioDecoderWrapper(
    MediaPipelineBackend::AudioDecoder* backend_decoder,
    AudioContentType type,
    MediaPipelineBackendManager::BufferDelegate* buffer_delegate)
    : decoder_(backend_decoder),
      content_type_(type),
      buffer_delegate_(buffer_delegate),
      initialized_(false),
      delegate_active_(false),
      stream_volume_multiplier_(1.0f) {
  if (buffer_delegate_) {
    buffer_delegate_->OnStreamStarted();
  }
}

ActiveAudioDecoderWrapper::~ActiveAudioDecoderWrapper() {
  if (buffer_delegate_) {
    buffer_delegate_->OnStreamStopped();
  }
}

void ActiveAudioDecoderWrapper::OnInitialized() {
  initialized_ = true;
  if (!delegate_active_) {
    decoder_.SetVolume(stream_volume_multiplier_);
  }
}

void ActiveAudioDecoderWrapper::SetDelegate(Delegate* delegate) {
  decoder_.SetDelegate(delegate);
}

CmaBackend::BufferStatus ActiveAudioDecoderWrapper::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  if (buffer_delegate_ && buffer_delegate_->IsActive()) {
    // Mute the decoder, we are sending audio to delegate.
    if (!delegate_active_) {
      delegate_active_ = true;
      decoder_.SetVolume(0.0);
    }
    buffer_delegate_->OnPushBuffer(buffer.get());
  } else {
    // Restore original volume.
    if (delegate_active_) {
      delegate_active_ = false;
      if (!decoder_.SetVolume(stream_volume_multiplier_)) {
        LOG(ERROR) << "SetVolume failed";
      }
    }
  }

  // Retain the buffer. Backend expects pipeline to hold the buffer until
  // Decoder::Delegate::OnBufferComplete is called.
  // TODO: Release the buffer at a proper time.
  pushed_buffer_ = std::move(buffer);
  return decoder_.PushBuffer(pushed_buffer_.get());
}

bool ActiveAudioDecoderWrapper::SetConfig(const AudioConfig& config) {
  if (buffer_delegate_) {
    buffer_delegate_->OnSetConfig(config);
  }
  return decoder_.SetConfig(config);
}

bool ActiveAudioDecoderWrapper::SetVolume(float multiplier) {
  stream_volume_multiplier_ = std::max(0.0f, multiplier);
  if (buffer_delegate_) {
    buffer_delegate_->OnSetVolume(stream_volume_multiplier_);
  }

  if (delegate_active_ || !initialized_) {
    return true;
  }
  return decoder_.SetVolume(stream_volume_multiplier_);
}

ActiveAudioDecoderWrapper::RenderingDelay
ActiveAudioDecoderWrapper::GetRenderingDelay() {
  return decoder_.GetRenderingDelay();
}

void ActiveAudioDecoderWrapper::GetStatistics(Statistics* statistics) {
  decoder_.GetStatistics(statistics);
}

bool ActiveAudioDecoderWrapper::RequiresDecryption() {
  return (MediaPipelineBackend::AudioDecoder::RequiresDecryption &&
          MediaPipelineBackend::AudioDecoder::RequiresDecryption()) ||
         decoder_.IsUsingSoftwareDecoder();
}

AudioDecoderWrapper::AudioDecoderWrapper(
    MediaPipelineBackend::AudioDecoder* backend_decoder,
    AudioContentType type,
    MediaPipelineBackendManager::BufferDelegate* buffer_delegate)
    : decoder_revoked_(false) {
  audio_decoder_ = std::make_unique<ActiveAudioDecoderWrapper>(
      backend_decoder, type, buffer_delegate);
}

AudioDecoderWrapper::AudioDecoderWrapper(AudioContentType type)
    : decoder_revoked_(true) {
  audio_decoder_ = std::make_unique<RevokedAudioDecoderWrapper>(
      RenderingDelay(), Statistics(), false);
}

AudioDecoderWrapper::~AudioDecoderWrapper() = default;

void AudioDecoderWrapper::OnInitialized() {
  audio_decoder_->OnInitialized();
}

void AudioDecoderWrapper::Revoke() {
  if (!decoder_revoked_) {
    decoder_revoked_ = true;
    // Get some current values from audio_decoder_(ActiveAudioDecoderWrapper),
    // then replace the audio_decoder_ with a revoked one.
    Statistics statistics;
    audio_decoder_->GetStatistics(&statistics);
    audio_decoder_ = std::make_unique<RevokedAudioDecoderWrapper>(
        audio_decoder_->GetRenderingDelay(), statistics,
        audio_decoder_->RequiresDecryption());
  }
}

void AudioDecoderWrapper::SetDelegate(Delegate* delegate) {
  audio_decoder_->SetDelegate(delegate);
}

AudioDecoderWrapper::BufferStatus AudioDecoderWrapper::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  return audio_decoder_->PushBuffer(buffer);
}

bool AudioDecoderWrapper::SetConfig(const AudioConfig& config) {
  return audio_decoder_->SetConfig(config);
}

bool AudioDecoderWrapper::SetVolume(float multiplier) {
  return audio_decoder_->SetVolume(multiplier);
}

AudioDecoderWrapper::RenderingDelay AudioDecoderWrapper::GetRenderingDelay() {
  return audio_decoder_->GetRenderingDelay();
}

void AudioDecoderWrapper::GetStatistics(Statistics* statistics) {
  audio_decoder_->GetStatistics(statistics);
}

bool AudioDecoderWrapper::RequiresDecryption() {
  return audio_decoder_->RequiresDecryption();
}

}  // namespace media
}  // namespace chromecast
