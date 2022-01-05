// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/streaming_runtime_application.h"

#include "base/strings/stringprintf.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/public/cast_streaming_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

namespace chromecast {
namespace {

const char kCastTransportBindingName[] = "cast.__platform__.cast_transport";
const char kMediaCapabilitiesBindingName[] = "cast.__platform__.canDisplayType";

const char kStreamingPageUrlTemplate[] =
    "data:text/html;charset=UTF-8, <video style='position:absolute; "
    "top:50%%; left:50%%; transform:translate(-50%%,-50%%); "
    "max-width:100%%; max-height:100%%; min-width: 100%%; min-height: 100%%' "
    "src='%s'></video>";

}  // namespace

StreamingRuntimeApplication::StreamingRuntimeApplication(
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    cast_streaming::NetworkContextGetter network_context_getter)
    : RuntimeApplicationBase(mojom::RendererType::MOJO_RENDERER,
                             web_service,
                             std::move(task_runner)),
      network_context_getter_(std::move(network_context_getter)) {}

StreamingRuntimeApplication::~StreamingRuntimeApplication() {
  StopApplication();
}

void StreamingRuntimeApplication::HandleMessage(
    const cast::web::Message& message,
    cast::web::MessagePortStatus* response) {
  message_port_service_->HandleMessage(message, response);
}

void StreamingRuntimeApplication::OnStreamingSessionStarted() {
  LOG(INFO) << "Streaming session started for " << *this << "!";
  has_started_streaming_ = true;
  SetApplicationStarted();

  if (renderer_connection_) {
    StartRenderer();
  }
}

void StreamingRuntimeApplication::OnError() {
  LOG(WARNING) << "Streaming session for " << *this << " has hit an error!";
  StopApplication();
}

void StreamingRuntimeApplication::StartAvSettingsQuery(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port) {
  // Connect the port to allow for sending messages. Querying will be done by
  // the associated |receiver_session_client_|.
  message_port_service_->ConnectToPort(kMediaCapabilitiesBindingName,
                                       std::move(message_port));
}

void StreamingRuntimeApplication::InitializeApplication(
    CoreApplicationServiceGrpc* grpc_stub,
    CastWebContents* cast_web_contents) {
  DCHECK(app_url().is_empty());

  message_port_service_ =
      std::make_unique<MessagePortService>(grpc_cq_, grpc_stub);

  // Bind Cast Transport.
  std::unique_ptr<cast_api_bindings::MessagePort> server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&client_port, &server_port);
  message_port_service_->ConnectToPort(kCastTransportBindingName,
                                       std::move(client_port));

  // Allow for capturing of the renderer controls mojo pipe.
  Observe(cast_web_contents);

  // Initialize the streaming receiver.
  receiver_session_client_ = std::make_unique<StreamingReceiverSessionClient>(
      task_runner(), network_context_getter_, std::move(server_port), this,
      /* supports_audio= */ app_config().app_id() !=
          openscreen::cast::GetIosAppStreamingAudioVideoAppId(),
      /* supports_video= */ true);
  receiver_session_client_->LaunchStreamingReceiverAsync(cast_web_contents);

  std::string streaming_url =
      cast_streaming::GetCastStreamingMediaSourceUrl().spec();
  set_app_url(GURL(
      base::StringPrintf(kStreamingPageUrlTemplate, streaming_url.c_str())));
}

void StreamingRuntimeApplication::StopApplication() {
  if (!receiver_session_client_) {
    DLOG(INFO) << "Streaming session never started prior to " << *this
               << " stop.";
  }

  receiver_session_client_.reset();
  RuntimeApplicationBase::StopApplication();
  message_port_service_.reset();
}

void StreamingRuntimeApplication::MainFrameReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  DLOG(INFO)
      << "Capturing CastStreamingRendererController remote pipe for URL: "
      << navigation_handle->GetURL() << " in " << *this;

  renderer_connection_.reset();
  navigation_handle->GetRenderFrameHost()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&renderer_connection_);
  DCHECK(renderer_connection_);

  if (has_started_streaming_) {
    StartRenderer();
  }
}

void StreamingRuntimeApplication::StartRenderer() {
  DCHECK(has_started_streaming_);
  DCHECK(renderer_connection_);
  DCHECK(!renderer_controls_.is_bound());

  renderer_connection_->SetPlaybackController(
      renderer_controls_.BindNewPipeAndPassReceiver());
  renderer_controls_->StartPlayingFrom(base::Seconds(0));
  renderer_controls_->SetPlaybackRate(1.0);

  LOG(INFO) << "Starting CastStreamingRenderer playback for " << *this;
}

}  // namespace chromecast
