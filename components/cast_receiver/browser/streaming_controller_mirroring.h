// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_MIRRORING_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_MIRRORING_H_

#include <memory>

#include "components/cast_receiver/browser/streaming_controller_base.h"
#include "components/cast_streaming/common/public/mojom/demuxer_connector.mojom.h"
#include "components/cast_streaming/common/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cast_api_bindings {
class MessagePort;
}  // namespace cast_api_bindings

namespace cast_streaming {
class ReceiverSession;
}  // namespace cast_streaming

namespace content {
class WebContents;
}  // namespace content

namespace cast_receiver {

// This class provides an implementation of StreamingControllerBase using the
// mirroring functionality provided in the cast_streaming component (but not its
// remoting functionality).
class StreamingControllerMirroring : public StreamingControllerBase {
 public:
  StreamingControllerMirroring(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port,
      content::WebContents* web_contents);
  ~StreamingControllerMirroring() override;

 private:
  // StreamingControllerBase overrides:
  void StartPlayback(
      cast_streaming::ReceiverSession* receiver_session,
      mojo::AssociatedRemote<cast_streaming::mojom::DemuxerConnector>
          demuxer_connector,
      mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
          renderer_connection) override;
  void ProcessConfig(cast_streaming::ReceiverConfig& config) override;

  mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
      renderer_connection_;
  mojo::Remote<media::mojom::Renderer> renderer_controls_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_STREAMING_CONTROLLER_MIRRORING_H_
