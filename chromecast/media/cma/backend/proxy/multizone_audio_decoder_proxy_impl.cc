// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/multizone_audio_decoder_proxy_impl.h"

#include "base/notreached.h"
#include "chromecast/media/api/monotonic_clock.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/media_pipeline_device_params.h"

namespace chromecast {
namespace media {

MultizoneAudioDecoderProxyImpl::MultizoneAudioDecoderProxyImpl(
    const MediaPipelineDeviceParams& params,
    CmaBackend::AudioDecoder* downstream_decoder)
    : MultizoneAudioDecoderProxy(downstream_decoder),
      cast_session_id_(params.session_id),
      decoder_mode_(CmaProxyHandler::AudioDecoderOperationMode::kMultiroomOnly),
      proxy_handler_(CmaProxyHandler::Create(params.task_runner, this)),
      clock_(MonotonicClock::Create()),
      buffer_id_manager_(this) {
  DCHECK(proxy_handler_);
  DCHECK(clock_);
}

MultizoneAudioDecoderProxyImpl::MultizoneAudioDecoderProxyImpl(
    const MediaPipelineDeviceParams& params,
    std::unique_ptr<AudioDecoderPipelineNode> downstream_decoder)
    : MultizoneAudioDecoderProxyImpl(params, downstream_decoder.get()) {
  SetOwnedDecoder(std::move(downstream_decoder));
}

MultizoneAudioDecoderProxyImpl::~MultizoneAudioDecoderProxyImpl() = default;

void MultizoneAudioDecoderProxyImpl::Initialize() {
  CheckCalledOnCorrectThread();
  proxy_handler_->Initialize(cast_session_id_, decoder_mode_);
}

void MultizoneAudioDecoderProxyImpl::Start(int64_t start_pts) {
  CheckCalledOnCorrectThread();
  proxy_handler_->Start(start_pts, CreateTargetBufferInfo());
}

void MultizoneAudioDecoderProxyImpl::Stop() {
  CheckCalledOnCorrectThread();
  proxy_handler_->Stop();
}

void MultizoneAudioDecoderProxyImpl::Pause() {
  CheckCalledOnCorrectThread();
  proxy_handler_->Pause();
}

void MultizoneAudioDecoderProxyImpl::Resume() {
  CheckCalledOnCorrectThread();
  proxy_handler_->Resume(CreateTargetBufferInfo());
}

void MultizoneAudioDecoderProxyImpl::SetPlaybackRate(float rate) {
  CheckCalledOnCorrectThread();
  proxy_handler_->SetPlaybackRate(rate);
}

void MultizoneAudioDecoderProxyImpl::LogicalPause() {
  CheckCalledOnCorrectThread();
  // There is intentionally no proxy implementation of this method.
}

void MultizoneAudioDecoderProxyImpl::LogicalResume() {
  CheckCalledOnCorrectThread();
  // There is intentionally no proxy implementation of this method.
}

int64_t MultizoneAudioDecoderProxyImpl::GetCurrentPts() const {
  CheckCalledOnCorrectThread();

  // This will be implemented as part of audio-audio sync.
  NOTIMPLEMENTED();
  return pts_offset_;
}

CmaProxyHandler::TargetBufferInfo
MultizoneAudioDecoderProxyImpl::CreateTargetBufferInfo() {
  return {buffer_id_manager_.GetCurrentlyProcessingBuffer(), clock_->Now()};
}

MultizoneAudioDecoderProxy::BufferStatus
MultizoneAudioDecoderProxyImpl::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  if (!proxy_handler_->PushBuffer(
          buffer, buffer_id_manager_.AssignBufferId(buffer->data_size()))) {
    return BufferStatus::kBufferFailed;
  }

  return MultizoneAudioDecoderProxy::PushBuffer(std::move(buffer));
}

bool MultizoneAudioDecoderProxyImpl::SetConfig(const AudioConfig& config) {
  return proxy_handler_->SetConfig(config) &&
         MultizoneAudioDecoderProxy::SetConfig(config);
}

void MultizoneAudioDecoderProxyImpl::GetStatistics(Statistics* statistics) {
  DCHECK(statistics);
  CheckCalledOnCorrectThread();
  statistics->decoded_bytes = bytes_decoded_;
}

void MultizoneAudioDecoderProxyImpl::OnError() {
  CheckCalledOnCorrectThread();
  NOTREACHED();
}

void MultizoneAudioDecoderProxyImpl::OnPipelineStateChange(
    CmaProxyHandler::PipelineState state) {
  CheckCalledOnCorrectThread();
}

void MultizoneAudioDecoderProxyImpl::OnBytesDecoded(
    int64_t decoded_byte_count) {
  CheckCalledOnCorrectThread();
  bytes_decoded_ = decoded_byte_count;
}

}  // namespace media
}  // namespace chromecast
