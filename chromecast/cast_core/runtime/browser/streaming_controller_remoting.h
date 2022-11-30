// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_CONTROLLER_REMOTING_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_CONTROLLER_REMOTING_H_

#include <memory>

#include "chromecast/cast_core/runtime/browser/streaming_controller_base.h"
#include "components/cast_streaming/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace cast_api_bindings {
class MessagePort;
}  // namespace cast_api_bindings

namespace cast_streaming {
class ReceiverSession;
}  // namespace cast_streaming

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {

// This class provides an implementation of StreamingControllerBase using the
// remoting functionality provided in the cast_streaming component.
class StreamingControllerRemoting : public StreamingControllerBase {
 public:
  StreamingControllerRemoting(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      content::WebContents* web_contents);
  ~StreamingControllerRemoting() override;

 private:
  // StreamingControllerBase overrides:
  void StartPlayback(
      cast_streaming::ReceiverSession* receiver_session,
      mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
          demuxer_connector,
      mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
          renderer_connection) override;
  void ProcessAVConstraints(
      cast_streaming::ReceiverSession::AVConstraints* constraints) override;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_CONTROLLER_REMOTING_H_
