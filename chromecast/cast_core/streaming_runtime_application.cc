// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/streaming_runtime_application.h"

#include "base/strings/stringprintf.h"
#include "chromecast/cast_core/message_port_service.h"
#include "components/cast/message_port/cast_core/create_message_port_core.h"
#include "components/cast/message_port/cast_core/message_port_core.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/public/cast_streaming_url.h"
#include "content/public/browser/web_contents.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"

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
    : RuntimeApplicationBase(mojom::RendererType::REMOTING_RENDERER,
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
  SetApplicationStarted();
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

GURL StreamingRuntimeApplication::ProcessWebView(
    CoreApplicationServiceGrpc* grpc_stub,
    CastWebContents* cast_web_contents) {
  message_port_service_ = std::make_unique<MessagePortService>(
      base::BindRepeating(&cast_api_bindings::CreateMessagePortCorePair),
      grpc_cq_, grpc_stub);

  // Bind Cast Transport.
  std::unique_ptr<cast_api_bindings::MessagePort> server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> client_port;
  cast_api_bindings::CreateMessagePortCorePair(&client_port, &server_port);
  message_port_service_->ConnectToPort(kCastTransportBindingName,
                                       std::move(server_port));

  // Initialize the streaming receiver.
  receiver_session_client_ = std::make_unique<StreamingReceiverSessionClient>(
      task_runner(), network_context_getter_, std::move(client_port), this);
  receiver_session_client_->LaunchStreamingReceiverAsync(cast_web_contents);

  std::string streaming_url =
      cast_streaming::GetCastStreamingMediaSourceUrl().spec();
  return GURL(
      base::StringPrintf(kStreamingPageUrlTemplate, streaming_url.c_str()));
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

}  // namespace chromecast
