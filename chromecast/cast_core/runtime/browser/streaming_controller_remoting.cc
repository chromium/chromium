// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/streaming_controller_remoting.h"

#include "components/cast/message_port/message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "third_party/openscreen/src/cast/streaming/receiver_session.h"

namespace chromecast {

StreamingControllerRemoting::StreamingControllerRemoting(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port,
    CastWebContents* cast_web_contents)
    : StreamingControllerBase(std::move(message_port), cast_web_contents) {}

StreamingControllerRemoting::~StreamingControllerRemoting() = default;

void StreamingControllerRemoting::StartPlayback(
    cast_streaming::ReceiverSession* receiver_session,
    mojo::AssociatedRemote<cast_streaming::mojom::CastStreamingReceiver>
        cast_streaming_receiver,
    mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
        renderer_connection) {
  receiver_session->StartStreamingAsync(std::move(cast_streaming_receiver),
                                        std::move(renderer_connection));

  auto* renderer_controller = receiver_session->GetRendererControls();
  DCHECK(renderer_controller);
  DCHECK(renderer_controller->IsValid());
  renderer_controller->StartPlayingFrom(base::Seconds(0));
  renderer_controller->SetPlaybackRate(1.0);
}

void StreamingControllerRemoting::ProcessAVConstraints(
    cast_streaming::ReceiverSession::AVConstraints* constraints) {
  DCHECK(constraints);

  // Ensure remoting is enabled for this streaming session.
  if (!constraints->remoting) {
    DLOG(INFO) << "Remoting configuration added to received AVConstraints";
    constraints->remoting = std::make_unique<
        openscreen::cast::ReceiverSession::RemotingPreferences>();
  }
}

}  // namespace chromecast
