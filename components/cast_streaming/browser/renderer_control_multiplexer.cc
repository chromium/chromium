// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_streaming/browser/renderer_control_multiplexer.h"

namespace cast_streaming {

RendererControlMultiplexer::RendererControlMultiplexer(
    mojo::Remote<media::mojom::Renderer> renderer_remote)
    : renderer_remote_(std::move(renderer_remote)) {}

RendererControlMultiplexer::~RendererControlMultiplexer() = default;

void RendererControlMultiplexer::RegisterController(
    mojo::PendingReceiver<media::mojom::Renderer> controls) {
  DCHECK(controls);
  auto bound_controls =
      std::make_unique<mojo::Receiver<media::mojom::Renderer>>(
          this, std::move(controls));
  receiver_list_.push_back(std::move(bound_controls));
}

void RendererControlMultiplexer::StartPlayingFrom(::base::TimeDelta time) {
  renderer_remote_->StartPlayingFrom(time);
}

void RendererControlMultiplexer::SetPlaybackRate(double playback_rate) {
  renderer_remote_->SetPlaybackRate(playback_rate);
}

void RendererControlMultiplexer::SetVolume(float volume) {
  renderer_remote_->SetVolume(volume);
}

void RendererControlMultiplexer::SetCdm(
    const absl::optional<::base::UnguessableToken>& cdm_id,
    SetCdmCallback callback) {
  renderer_remote_->SetCdm(cdm_id, std::move(callback));
}

void RendererControlMultiplexer::Initialize(
    mojo::PendingAssociatedRemote<media::mojom::RendererClient> client,
    absl::optional<
        std::vector<::mojo::PendingRemote<::media::mojom::DemuxerStream>>>
        streams,
    media::mojom::MediaUrlParamsPtr media_url_params,
    InitializeCallback callback) {
  renderer_remote_->Initialize(std::move(client), std::move(streams),
                               std::move(media_url_params),
                               std::move(callback));
}

void RendererControlMultiplexer::Flush(FlushCallback callback) {
  renderer_remote_->Flush(std::move(callback));
}

}  // namespace cast_streaming
