// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/audio_decoder_software_wrapper.h"

#include <ostream>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/base/decoder_config_logging.h"

namespace chromecast {
namespace media {

namespace {
const int kMonoChannelCount = 1;
const int kStereoChannelCount = 2;
const int k5_1ChannelCount = 6;

bool IsChannelLayoutSupported(AudioConfig config) {
  if (config.channel_number == kMonoChannelCount ||
      config.channel_number == kStereoChannelCount)
    return true;

  // Only supports 5.1 for Opus.
  if (config.channel_number == k5_1ChannelCount &&
      config.codec == AudioCodec::kCodecOpus)
    return true;

  return false;
}

// Codecs that cannot be decoded on the device and must be passed through.
constexpr media::AudioCodec kPassthroughCodecs[] = {
    kCodecEAC3, kCodecAC3, kCodecDTS, kCodecMpegHAudio,
};

}  // namespace

AudioDecoderSoftwareWrapper::AudioDecoderSoftwareWrapper(
    MediaPipelineBackend::AudioDecoder* backend_decoder)
    : backend_decoder_(backend_decoder),
      delegate_(nullptr),
      decoder_error_(false) {
  DCHECK(backend_decoder_);
  backend_decoder_->SetDelegate(this);
}

AudioDecoderSoftwareWrapper::~AudioDecoderSoftwareWrapper() {}

void AudioDecoderSoftwareWrapper::SetDelegate(DecoderDelegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
  if (decoder_error_) {
    delegate_->OnDecoderError();
  }
}

MediaPipelineBackend::BufferStatus AudioDecoderSoftwareWrapper::PushBuffer(
    CastDecoderBuffer* buffer) {
  DCHECK(buffer);
  if (!software_decoder_)
    return backend_decoder_->PushBuffer(buffer);

  DecoderBufferBase* buffer_base = static_cast<DecoderBufferBase*>(buffer);
  software_decoder_->Decode(
      base::WrapRefCounted(buffer_base),
      base::BindOnce(&AudioDecoderSoftwareWrapper::OnDecodedBuffer,
                     base::Unretained(this)));
  return MediaPipelineBackend::kBufferPending;
}

void AudioDecoderSoftwareWrapper::GetStatistics(Statistics* statistics) {
  DCHECK(statistics);
  return backend_decoder_->GetStatistics(statistics);
}

bool AudioDecoderSoftwareWrapper::SetConfig(const AudioConfig& config) {
  DCHECK(IsValidConfig(config));

  if (backend_decoder_->SetConfig(config)) {
    LOG(INFO) << "Using backend decoder for " << config.codec;
    software_decoder_.reset();
    output_config_ = config;
    return true;
  }

  if (base::Contains(kPassthroughCodecs, config.codec)) {
    LOG(INFO) << "Cannot use software decoder for " << config.codec;
    return false;
  }

  if (!CreateSoftwareDecoder(config)) {
    LOG(INFO) << "Failed to create software decoder for " << config.codec;
    return false;
  }

  LOG(INFO) << "Using software decoder for " << config.codec;

  output_config_.codec = media::kCodecPCM;
  output_config_.sample_format = media::kSampleFormatS16;
  // The underlying software decoder will always convert mono to stereo,
  // so set output stereo in the case of mono input.
  if (config.channel_number == kMonoChannelCount) {
    output_config_.channel_number = kStereoChannelCount;
  } else {
    output_config_.channel_number = config.channel_number;
  }
  output_config_.bytes_per_channel = 2;
  output_config_.samples_per_second = config.samples_per_second;
  output_config_.encryption_scheme = EncryptionScheme::kUnencrypted;
  output_config_.channel_layout = config.channel_layout;
  return backend_decoder_->SetConfig(output_config_);
}

bool AudioDecoderSoftwareWrapper::SetVolume(float multiplier) {
  return backend_decoder_->SetVolume(multiplier);
}

AudioDecoderSoftwareWrapper::RenderingDelay
AudioDecoderSoftwareWrapper::GetRenderingDelay() {
  return backend_decoder_->GetRenderingDelay();
}

bool AudioDecoderSoftwareWrapper::IsUsingSoftwareDecoder() {
  return software_decoder_.get() != nullptr;
}

bool AudioDecoderSoftwareWrapper::CreateSoftwareDecoder(
    const AudioConfig& config) {
  if (!IsChannelLayoutSupported(config)) {
    LOG(ERROR) << "Software audio decoding is not supported for channel: "
               << config.channel_number << " with codec: " << config.codec;
    return false;
  }
  // TODO(kmackay) Consider using planar float instead.
  software_decoder_ = media::CastAudioDecoder::Create(
      base::ThreadTaskRunnerHandle::Get(), config,
      media::CastAudioDecoder::kOutputSigned16,
      base::BindOnce(&AudioDecoderSoftwareWrapper::OnDecoderInitialized,
                     base::Unretained(this)));
  return (software_decoder_.get() != nullptr);
}

void AudioDecoderSoftwareWrapper::OnDecoderInitialized(bool success) {
  if (!success) {
    decoder_error_ = true;
    LOG(ERROR) << "Failed to initialize software decoder";
    if (delegate_) {
      delegate_->OnDecoderError();
    }
  }
}

void AudioDecoderSoftwareWrapper::OnDecodedBuffer(
    CastAudioDecoder::Status status,
    const media::AudioConfig& config,
    scoped_refptr<DecoderBufferBase> decoded) {
  DCHECK(delegate_);
  if (status != CastAudioDecoder::kDecodeOk) {
    delegate_->OnPushBufferComplete(MediaPipelineBackend::kBufferFailed);
    return;
  }

  if (config.samples_per_second != output_config_.samples_per_second) {
    // Sample rate of actual stream may differ from the rate reported from the
    // container format.
    output_config_.samples_per_second = config.samples_per_second;
    if (!backend_decoder_->SetConfig(output_config_)) {
      LOG(ERROR) << "Failed to set underlying backend to changed sample rate";
      delegate_->OnPushBufferComplete(MediaPipelineBackend::kBufferFailed);
      return;
    }
  }

  pending_pushed_buffer_ = decoded;
  MediaPipelineBackend::BufferStatus buffer_status =
      backend_decoder_->PushBuffer(pending_pushed_buffer_.get());
  if (buffer_status != MediaPipelineBackend::kBufferPending)
    delegate_->OnPushBufferComplete(buffer_status);
}

void AudioDecoderSoftwareWrapper::OnPushBufferComplete(
    MediaPipelineBackend::BufferStatus status) {
  DCHECK(delegate_);
  delegate_->OnPushBufferComplete(status);
}

void AudioDecoderSoftwareWrapper::OnEndOfStream() {
  DCHECK(delegate_);
  delegate_->OnEndOfStream();
}

void AudioDecoderSoftwareWrapper::OnDecoderError() {
  DCHECK(delegate_);
  delegate_->OnDecoderError();
}

void AudioDecoderSoftwareWrapper::OnKeyStatusChanged(const std::string& key_id,
                                                     CastKeyStatus key_status,
                                                     uint32_t system_code) {
  DCHECK(delegate_);
  delegate_->OnKeyStatusChanged(key_id, key_status, system_code);
}

void AudioDecoderSoftwareWrapper::OnVideoResolutionChanged(const Size& size) {
  NOTREACHED();
}

}  // namespace media
}  // namespace chromecast
