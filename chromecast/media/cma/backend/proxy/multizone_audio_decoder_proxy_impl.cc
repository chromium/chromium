// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy_impl.h"

#include "base/notreached.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

MultizoneAudioDecoderProxyImpl::MultizoneAudioDecoderProxyImpl(
    const MediaPipelineDeviceParams& params,
    CmaBackend::AudioDecoder* downstream_decoder)
    : cast_session_id_(params.session_id),
      decoder_mode_(CmaProxyHandler::AudioDecoderOperationMode::kMultiroomOnly),
      downstream_decoder_(downstream_decoder),
      proxy_handler_(CmaProxyHandler::Create(params.task_runner, this)) {
  DCHECK(downstream_decoder_);
  DCHECK(proxy_handler_);

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MultizoneAudioDecoderProxyImpl::~MultizoneAudioDecoderProxyImpl() = default;

void MultizoneAudioDecoderProxyImpl::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_handler_->Initialize(cast_session_id_, decoder_mode_);
}

void MultizoneAudioDecoderProxyImpl::Start(int64_t start_pts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_handler_->Start(start_pts);
}

void MultizoneAudioDecoderProxyImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_handler_->Stop();
}

void MultizoneAudioDecoderProxyImpl::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_handler_->Pause();
}

void MultizoneAudioDecoderProxyImpl::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_handler_->Resume();
}

void MultizoneAudioDecoderProxyImpl::SetPlaybackRate(float rate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  proxy_handler_->SetPlaybackRate(rate);
}

void MultizoneAudioDecoderProxyImpl::LogicalPause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There is intentionally no proxy implementation of this method.
}

void MultizoneAudioDecoderProxyImpl::LogicalResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // There is intentionally no proxy implementation of this method.
}

int64_t MultizoneAudioDecoderProxyImpl::GetCurrentPts() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This will be implemented as part of audio-audio sync.
  NOTIMPLEMENTED();
  return pts_offset_;
}

void MultizoneAudioDecoderProxyImpl::SetDelegate(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  downstream_decoder_->SetDelegate(delegate);
}

MultizoneAudioDecoderProxy::BufferStatus
MultizoneAudioDecoderProxyImpl::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!proxy_handler_->PushBuffer(buffer)) {
    return BufferStatus::kBufferFailed;
  }

  return downstream_decoder_->PushBuffer(std::move(buffer));
}

bool MultizoneAudioDecoderProxyImpl::SetConfig(const AudioConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return proxy_handler_->SetConfig(config) &&
         downstream_decoder_->SetConfig(config);
}

bool MultizoneAudioDecoderProxyImpl::SetVolume(float multiplier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The proxy implementation of this method is INTENTIONALLY not called here.
  return true;
}

MultizoneAudioDecoderProxyImpl::RenderingDelay
MultizoneAudioDecoderProxyImpl::GetRenderingDelay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This will be implemented as part of audio-audio sync.
  NOTIMPLEMENTED();
  return RenderingDelay{};
}

void MultizoneAudioDecoderProxyImpl::GetStatistics(Statistics* statistics) {
  DCHECK(statistics);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  statistics->decoded_bytes = bytes_decoded_;
}

bool MultizoneAudioDecoderProxyImpl::RequiresDecryption() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return downstream_decoder_->RequiresDecryption();
}

void MultizoneAudioDecoderProxyImpl::SetObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  downstream_decoder_->SetObserver(observer);
}

void MultizoneAudioDecoderProxyImpl::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

void MultizoneAudioDecoderProxyImpl::OnPipelineStateChange(
    CmaProxyHandler::PipelineState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MultizoneAudioDecoderProxyImpl::OnBytesDecoded(
    int64_t decoded_byte_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bytes_decoded_ = decoded_byte_count;
}

}  // namespace media
}  // namespace chromecast
