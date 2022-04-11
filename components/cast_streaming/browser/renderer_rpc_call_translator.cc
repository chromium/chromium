// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/renderer_rpc_call_translator.h"

#include "components/cast_streaming/public/remoting_message_factories.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming::remoting {

RendererRpcCallTranslator::RendererRpcCallTranslator(
    mojo::Remote<media::mojom::Renderer> remote_renderer)
    : renderer_client_receiver_(this),
      renderer_remote_(std::move(remote_renderer)),
      weak_factory_(this) {}

RendererRpcCallTranslator::~RendererRpcCallTranslator() = default;

void RendererRpcCallTranslator::SetMessageProcessor(
    RpcMessageProcessor processor) {
  message_processor_ = std::move(processor);
}

void RendererRpcCallTranslator::OnRpcInitialize() {
  renderer_remote_->Initialize(
      renderer_client_receiver_.BindNewEndpointAndPassRemote(),
      /* streams */ {}, /* media_url_params */ nullptr,
      base::BindOnce(&RendererRpcCallTranslator::OnInitializeCompleted,
                     weak_factory_.GetWeakPtr(), message_processor_));
}

void RendererRpcCallTranslator::OnRpcFlush(uint32_t audio_count,
                                           uint32_t video_count) {
  renderer_remote_->Flush(
      base::BindOnce(&RendererRpcCallTranslator::OnFlushCompleted,
                     weak_factory_.GetWeakPtr(), message_processor_));
}

void RendererRpcCallTranslator::OnRpcStartPlayingFrom(base::TimeDelta time) {
  renderer_remote_->StartPlayingFrom(time);
}

void RendererRpcCallTranslator::OnRpcSetPlaybackRate(double playback_rate) {
  renderer_remote_->SetPlaybackRate(playback_rate);
}

void RendererRpcCallTranslator::OnRpcSetVolume(double volume) {
  renderer_remote_->SetVolume(volume);
}

void RendererRpcCallTranslator::OnTimeUpdate(base::TimeDelta media_time,
                                             base::TimeDelta max_time,
                                             base::TimeTicks capture_time) {
  message_processor_.Run(CreateMessageForMediaTimeUpdate(media_time));
}

void RendererRpcCallTranslator::OnBufferingStateChange(
    media::BufferingState state,
    media::BufferingStateChangeReason reason) {
  message_processor_.Run(CreateMessageForBufferingStateChange(state));
}

void RendererRpcCallTranslator::OnError(const media::PipelineStatus& status) {
  message_processor_.Run(CreateMessageForError());
}

void RendererRpcCallTranslator::OnEnded() {
  message_processor_.Run(CreateMessageForMediaEnded());
}

void RendererRpcCallTranslator::OnAudioConfigChange(
    const media::AudioDecoderConfig& config) {
  message_processor_.Run(CreateMessageForAudioConfigChange(config));
}

void RendererRpcCallTranslator::OnVideoConfigChange(
    const media::VideoDecoderConfig& config) {
  message_processor_.Run(CreateMessageForVideoConfigChange(config));
}

void RendererRpcCallTranslator::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  message_processor_.Run(CreateMessageForVideoNaturalSizeChange(size));
}

void RendererRpcCallTranslator::OnVideoOpacityChange(bool opaque) {
  message_processor_.Run(CreateMessageForVideoOpacityChange(opaque));
}

void RendererRpcCallTranslator::OnStatisticsUpdate(
    const media::PipelineStatistics& stats) {
  message_processor_.Run(CreateMessageForStatisticsUpdate(stats));
}

void RendererRpcCallTranslator::OnWaiting(media::WaitingReason reason) {}

void RendererRpcCallTranslator::OnInitializeCompleted(
    RpcMessageProcessor processor,
    bool success) {
  message_processor_.Run(CreateMessageForInitializationComplete(success));
}

void RendererRpcCallTranslator::OnFlushCompleted(
    RpcMessageProcessor processor) {
  message_processor_.Run(CreateMessageForFlushComplete());
}

}  // namespace cast_streaming::remoting
