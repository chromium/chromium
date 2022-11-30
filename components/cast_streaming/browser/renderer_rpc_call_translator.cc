// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/renderer_rpc_call_translator.h"

#include "components/cast_streaming/public/remoting_message_factories.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming::remoting {

RendererRpcCallTranslator::RendererRpcCallTranslator(
    RpcMessageProcessor processor,
    media::mojom::Renderer* renderer)
    : message_processor_(std::move(processor)),
      renderer_client_receiver_(this),
      renderer_(std::move(renderer)),
      weak_factory_(this) {}

RendererRpcCallTranslator::~RendererRpcCallTranslator() = default;

void RendererRpcCallTranslator::OnRpcInitialize() {
  if (!has_been_initialized_) {
    has_been_initialized_ = true;
    renderer_->Initialize(
        renderer_client_receiver_.BindNewEndpointAndPassRemote(),
        /* streams */ {}, /* media_url_params */ nullptr,
        base::BindOnce(&RendererRpcCallTranslator::OnInitializeCompleted,
                       weak_factory_.GetWeakPtr(), handle_));
  } else {
    OnInitializeCompleted(handle_, true);
  }
}

void RendererRpcCallTranslator::OnRpcFlush(uint32_t audio_count,
                                           uint32_t video_count) {
  renderer_->Flush(base::BindOnce(&RendererRpcCallTranslator::OnFlushCompleted,
                                  weak_factory_.GetWeakPtr(), handle_));
}

void RendererRpcCallTranslator::OnRpcStartPlayingFrom(base::TimeDelta time) {
  renderer_->StartPlayingFrom(time);
}

void RendererRpcCallTranslator::OnRpcSetPlaybackRate(double playback_rate) {
  renderer_->SetPlaybackRate(playback_rate);
}

void RendererRpcCallTranslator::OnRpcSetVolume(double volume) {
  renderer_->SetVolume(volume);
}

void RendererRpcCallTranslator::OnTimeUpdate(base::TimeDelta media_time,
                                             base::TimeDelta max_time,
                                             base::TimeTicks capture_time) {
  message_processor_.Run(handle_, CreateMessageForMediaTimeUpdate(media_time));
}

void RendererRpcCallTranslator::OnBufferingStateChange(
    media::BufferingState state,
    media::BufferingStateChangeReason reason) {
  message_processor_.Run(handle_, CreateMessageForBufferingStateChange(state));
}

void RendererRpcCallTranslator::OnError(const media::PipelineStatus& status) {
  message_processor_.Run(handle_, CreateMessageForError());
}

void RendererRpcCallTranslator::OnEnded() {
  message_processor_.Run(handle_, CreateMessageForMediaEnded());
}

void RendererRpcCallTranslator::OnAudioConfigChange(
    const media::AudioDecoderConfig& config) {
  message_processor_.Run(handle_, CreateMessageForAudioConfigChange(config));
}

void RendererRpcCallTranslator::OnVideoConfigChange(
    const media::VideoDecoderConfig& config) {
  message_processor_.Run(handle_, CreateMessageForVideoConfigChange(config));
}

void RendererRpcCallTranslator::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  message_processor_.Run(handle_, CreateMessageForVideoNaturalSizeChange(size));
}

void RendererRpcCallTranslator::OnVideoOpacityChange(bool opaque) {
  message_processor_.Run(handle_, CreateMessageForVideoOpacityChange(opaque));
}

void RendererRpcCallTranslator::OnStatisticsUpdate(
    const media::PipelineStatistics& stats) {
  message_processor_.Run(handle_, CreateMessageForStatisticsUpdate(stats));
}

void RendererRpcCallTranslator::OnWaiting(media::WaitingReason reason) {}

void RendererRpcCallTranslator::OnInitializeCompleted(
    openscreen::cast::RpcMessenger::Handle handle_at_time_of_sending,
    bool success) {
  message_processor_.Run(handle_at_time_of_sending,
                         CreateMessageForInitializationComplete(success));
}

void RendererRpcCallTranslator::OnFlushCompleted(
    openscreen::cast::RpcMessenger::Handle handle_at_time_of_sending) {
  message_processor_.Run(handle_at_time_of_sending,
                         CreateMessageForFlushComplete());
}

}  // namespace cast_streaming::remoting
