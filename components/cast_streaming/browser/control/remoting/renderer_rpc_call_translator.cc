// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/control/remoting/renderer_rpc_call_translator.h"

#include "media/cast/openscreen/remoting_message_factories.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace cast_streaming::remoting {

RendererRpcCallTranslator::RendererRpcCallTranslator(
    RpcMessageProcessor processor,
    media::mojom::Renderer* renderer,
    FlushUntilCallback flush_until_cb)
    : flush_until_cb_(std::move(flush_until_cb)),
      message_processor_(std::move(processor)),
      renderer_client_receiver_(this),
      renderer_(std::move(renderer)),
      weak_factory_(this) {
  DCHECK(flush_until_cb_);
}

RendererRpcCallTranslator::~RendererRpcCallTranslator() = default;

void RendererRpcCallTranslator::SendFallbackMessage() {
  // TODO(crbug.com/1434469): Add a RPC_ONREMOTINGERROR call to report the
  // specifics of this failure.
  message_processor_.Run(remote_handle_, media::cast::CreateMessageForError());
}

void RendererRpcCallTranslator::OnRpcInitialize() {
  if (!has_been_initialized_) {
    has_been_initialized_ = true;
    renderer_->Initialize(
        renderer_client_receiver_.BindNewEndpointAndPassRemote(),
        /* streams */ {}, /* media_url_params */ nullptr,
        base::BindOnce(&RendererRpcCallTranslator::OnInitializeCompleted,
                       weak_factory_.GetWeakPtr(), remote_handle_));
  } else {
    OnInitializeCompleted(remote_handle_, true);
  }
}

void RendererRpcCallTranslator::OnRpcFlush(uint32_t audio_count,
                                           uint32_t video_count) {
  flush_handles_.push_back(remote_handle_);
  renderer_->Flush(base::BindOnce(&RendererRpcCallTranslator::OnFlushCompleted,
                                  weak_factory_.GetWeakPtr()));
  flush_until_cb_.Run(audio_count, video_count);
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
  message_processor_.Run(
      remote_handle_, media::cast::CreateMessageForMediaTimeUpdate(media_time));
}

void RendererRpcCallTranslator::OnBufferingStateChange(
    media::BufferingState state,
    media::BufferingStateChangeReason reason) {
  message_processor_.Run(
      remote_handle_, media::cast::CreateMessageForBufferingStateChange(state));
}

void RendererRpcCallTranslator::OnError(const media::PipelineStatus& status) {
  message_processor_.Run(remote_handle_, media::cast::CreateMessageForError());
}

void RendererRpcCallTranslator::OnEnded() {
  message_processor_.Run(remote_handle_,
                         media::cast::CreateMessageForMediaEnded());
}

void RendererRpcCallTranslator::OnAudioConfigChange(
    const media::AudioDecoderConfig& config) {
  message_processor_.Run(
      remote_handle_, media::cast::CreateMessageForAudioConfigChange(config));
}

void RendererRpcCallTranslator::OnVideoConfigChange(
    const media::VideoDecoderConfig& config) {
  message_processor_.Run(
      remote_handle_, media::cast::CreateMessageForVideoConfigChange(config));
}

void RendererRpcCallTranslator::OnVideoNaturalSizeChange(
    const gfx::Size& size) {
  message_processor_.Run(
      remote_handle_,
      media::cast::CreateMessageForVideoNaturalSizeChange(size));
}

void RendererRpcCallTranslator::OnVideoOpacityChange(bool opaque) {
  message_processor_.Run(
      remote_handle_, media::cast::CreateMessageForVideoOpacityChange(opaque));
}

void RendererRpcCallTranslator::OnStatisticsUpdate(
    const media::PipelineStatistics& stats) {
  message_processor_.Run(remote_handle_,
                         media::cast::CreateMessageForStatisticsUpdate(stats));
}

void RendererRpcCallTranslator::OnWaiting(media::WaitingReason reason) {}

void RendererRpcCallTranslator::OnInitializeCompleted(
    openscreen::cast::RpcMessenger::Handle handle_at_time_of_sending,
    bool success) {
  message_processor_.Run(
      handle_at_time_of_sending,
      media::cast::CreateMessageForInitializationComplete(success));
}

void RendererRpcCallTranslator::OnFlushCompleted() {
  for (auto handle : flush_handles_) {
    message_processor_.Run(handle,
                           media::cast::CreateMessageForFlushComplete());
  }
  flush_handles_.clear();
}

}  // namespace cast_streaming::remoting
