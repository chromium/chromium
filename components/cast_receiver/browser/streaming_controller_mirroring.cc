// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_controller_mirroring.h"

#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"

namespace cast_receiver {

// Callback used for RendererController::SetPlaybackController() mojo call.
void OnCastStreamingRendererAcquired() {
  // This method has been intentionally left empty.
}

StreamingControllerMirroring::StreamingControllerMirroring(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    content::WebContents* web_contents)
    : StreamingControllerBase(std::move(message_port), web_contents) {}

StreamingControllerMirroring::~StreamingControllerMirroring() = default;

void StreamingControllerMirroring::StartPlayback(
    cast_streaming::ReceiverSession* receiver_session,
    mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
        demuxer_connector,
    mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
        renderer_connection) {
  receiver_session->StartStreamingAsync(std::move(demuxer_connector));

  renderer_connection_ = std::move(renderer_connection);
  renderer_connection_->SetPlaybackController(
      renderer_controls_.BindNewPipeAndPassReceiver(),
      base::BindOnce(&OnCastStreamingRendererAcquired));
  renderer_controls_->StartPlayingFrom(base::Seconds(0));
  renderer_controls_->SetPlaybackRate(1.0);
}

void StreamingControllerMirroring::ProcessConfig(
    cast_streaming::ReceiverConfig& config) {
  // Ensure remoting is disabled for this streaming session.
  DLOG_IF(WARNING, config.remoting)
      << "Remoting configuration removed from received ReceiverConfig";
  config.remoting = std::nullopt;
}

}  // namespace cast_receiver
