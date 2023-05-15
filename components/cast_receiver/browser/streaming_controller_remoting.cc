// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_controller_remoting.h"

#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"

namespace cast_receiver {

StreamingControllerRemoting::StreamingControllerRemoting(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    content::WebContents* web_contents)
    : StreamingControllerBase(std::move(message_port), web_contents) {}

StreamingControllerRemoting::~StreamingControllerRemoting() = default;

void StreamingControllerRemoting::StartPlayback(
    cast_streaming::ReceiverSession* receiver_session,
    mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
        demuxer_connector,
    mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
        renderer_connection) {
  receiver_session->StartStreamingAsync(std::move(demuxer_connector),
                                        std::move(renderer_connection));
}

void StreamingControllerRemoting::ProcessConfig(
    cast_streaming::ReceiverConfig& config) {
  // Ensure remoting is enabled for this streaming session.
  if (!config.remoting) {
    DLOG(WARNING) << "Remoting configuration added to received ReceiverConfig";
    config.remoting.emplace();
  }
}

}  // namespace cast_receiver
