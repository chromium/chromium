// Copyright 2020 The Chromium Authors
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
    : MultizoneAudioDecoderProxy(downstream_decoder),
      cast_session_id_(params.session_id),
      decoder_mode_(CmaProxyHandler::AudioDecoderOperationMode::kMultiroomOnly),
      proxy_handler_(CmaProxyHandler::Create(params.task_runner, this, this)),
      buffer_id_manager_(this, this) {
  DCHECK(proxy_handler_);
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
  proxy_handler_->Start(
      start_pts,
      buffer_id_manager_.UpdateAndGetCurrentlyProcessingBufferInfo());
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
  proxy_handler_->Resume(
      buffer_id_manager_.UpdateAndGetCurrentlyProcessingBufferInfo());
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
  NOTREACHED();
}

CmaBackend::Decoder::BufferStatus MultizoneAudioDecoderProxyImpl::PushBuffer(
    scoped_refptr<DecoderBufferBase> buffer) {
  CheckCalledOnCorrectThread();
  DCHECK(proxy_handler_);
  DCHECK(buffer);

  if (pending_push_buffer_.get()) {
    return MultizoneAudioDecoderProxy::BufferStatus::kBufferFailed;
  }

  // First try to send the buffer over the gRPC.
  const auto proxy_result = proxy_handler_->PushBuffer(
      buffer, buffer_id_manager_.AssignBufferId(*buffer));

  // If that succeeds, send the buffer to the downstream decoder. Else, wait for
  // a callback to OnProxyPushBufferComplete() by returning kBufferPending, at
  // which point the downstream decoder will receive the PushBuffer call. This
  // acts as a flow control mechanism for the proxy decoder.
  if (proxy_result != BufferStatus::kBufferSuccess) {
    pending_push_buffer_ = std::move(buffer);
    return proxy_result;
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

void MultizoneAudioDecoderProxyImpl::OnTimestampUpdateNeeded(
    BufferIdManager::TargetBufferInfo buffer) {
  proxy_handler_->UpdateTimestamp(std::move(buffer));
}

void MultizoneAudioDecoderProxyImpl::OnAudioChannelPushBufferComplete(
    CmaBackend::BufferStatus status) {
  // Try to call PushBuffer on the downstream decoder.
  if (status != CmaBackend::BufferStatus::kBufferSuccess) {
    DCHECK_NE(status, CmaBackend::BufferStatus::kBufferPending);
    MultizoneAudioDecoderProxy::OnPushBufferComplete(status);
    return;
  }

  DCHECK(pending_push_buffer_);
  const auto downstream_decoder_result =
      MultizoneAudioDecoderProxy::PushBuffer(std::move(pending_push_buffer_));
  pending_push_buffer_.reset();

  // If it is able to immediately process the result (either as success or
  // failure), call OnPushBufferComplete() to signal that the decoder is ready
  // to accept more data. Else, wait for the downstream decoder to call it per
  // that method's contract.
  if (downstream_decoder_result != BufferStatus::kBufferPending) {
    MultizoneAudioDecoderProxy::OnPushBufferComplete(downstream_decoder_result);
  }
}

}  // namespace media
}  // namespace chromecast
