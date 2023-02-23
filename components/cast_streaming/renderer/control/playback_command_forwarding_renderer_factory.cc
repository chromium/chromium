// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/renderer/control/playback_command_forwarding_renderer_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "components/cast_streaming/renderer/control/playback_command_forwarding_renderer.h"

namespace cast_streaming {

PlaybackCommandForwardingRendererFactory::
    PlaybackCommandForwardingRendererFactory(
        mojo::PendingReceiver<media::mojom::Renderer> pending_renderer_controls)
    : pending_renderer_controls_(std::move(pending_renderer_controls)) {
  DCHECK(pending_renderer_controls_);
}

PlaybackCommandForwardingRendererFactory::
    ~PlaybackCommandForwardingRendererFactory() = default;

void PlaybackCommandForwardingRendererFactory::SetWrappedRendererFactory(
    media::RendererFactory* wrapped_factory) {
  DCHECK(!has_create_been_called_);
  real_renderer_factory_ = wrapped_factory;
}

std::unique_ptr<media::Renderer>
PlaybackCommandForwardingRendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::AudioRendererSink* audio_renderer_sink,
    media::VideoRendererSink* video_renderer_sink,
    media::RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK(pending_renderer_controls_);
  DCHECK(real_renderer_factory_);
  has_create_been_called_ = true;
  return std::make_unique<PlaybackCommandForwardingRenderer>(
      real_renderer_factory_->CreateRenderer(
          media_task_runner, worker_task_runner, audio_renderer_sink,
          video_renderer_sink, request_overlay_info_cb, target_color_space),
      media_task_runner, std::move(pending_renderer_controls_));
}

}  // namespace cast_streaming
