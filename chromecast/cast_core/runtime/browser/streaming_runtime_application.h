// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_RUNTIME_APPLICATION_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_RUNTIME_APPLICATION_H_

#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"
#include "chromecast/cast_core/runtime/browser/streaming_receiver_session_client.h"
#include "components/cast_streaming/browser/public/network_context_getter.h"
#include "components/cast_streaming/public/mojom/renderer_controller.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromecast {

class MessagePortService;

class StreamingRuntimeApplication final
    : public RuntimeApplicationBase,
      public StreamingReceiverSessionClient::Handler,
      public CastWebContents::Observer {
 public:
  // |web_service| is expected to exist for the lifetime of this instance.
  StreamingRuntimeApplication(
      CastWebService* web_service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      cast_streaming::NetworkContextGetter network_context_getter);
  ~StreamingRuntimeApplication() override;

 private:
  // RuntimeApplicationBase implementation:
  void HandleMessage(const cast::web::Message& message,
                     cast::web::MessagePortStatus* response) override;
  void InitializeApplication(CoreApplicationServiceGrpc* grpc_stub,
                             CastWebContents* cast_web_contents) override;
  void StopApplication() override;

  // StreamingReceiverSessionClient::Handler implementation:
  void OnStreamingSessionStarted() override;
  void OnError() override;
  void StartAvSettingsQuery(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port) override;

  // CastWebContents::Observer overrides.
  void MainFrameReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Helper method to start playback using |renderer_connection_| and
  // |renderer_controls_|.
  void StartRenderer();

  bool has_started_streaming_ = false;

  mojo::AssociatedRemote<cast_streaming::mojom::RendererController>
      renderer_connection_;
  mojo::Remote<media::mojom::Renderer> renderer_controls_;

  // Returns the network context used by |receiver_session_client_|.
  const cast_streaming::NetworkContextGetter network_context_getter_;

  // Handles communication with cast core over gRPC.
  std::unique_ptr<MessagePortService> message_port_service_;

  // Object responsible for maintaining the lifetime of the streaming session.
  std::unique_ptr<StreamingReceiverSessionClient> receiver_session_client_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_STREAMING_RUNTIME_APPLICATION_H_
