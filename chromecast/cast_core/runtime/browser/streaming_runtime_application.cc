// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/streaming_runtime_application.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/public/application_client.h"
#include "components/cast_streaming/browser/public/receiver_session.h"
#include "components/cast_streaming/public/cast_streaming_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/openscreen/src/cast/common/public/cast_streaming_app_ids.h"

namespace chromecast {
namespace {

constexpr char kCastTransportBindingName[] = "cast.__platform__.cast_transport";
constexpr char kMediaCapabilitiesBindingName[] =
    "cast.__platform__.canDisplayType";

constexpr char kStreamingPageUrlTemplate[] =
    "data:text/html;charset=UTF-8, <video style='position:absolute; "
    "top:50%%; left:50%%; transform:translate(-50%%,-50%%); "
    "max-width:100%%; max-height:100%%; min-width: 100%%; min-height: 100%%' "
    "src='%s'></video>";

}  // namespace

StreamingRuntimeApplication::StreamingRuntimeApplication(
    std::string cast_session_id,
    cast::common::ApplicationConfig app_config,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    cast_receiver::ApplicationClient& application_client,
    RuntimeApplicationPlatform::Factory runtime_application_factory)
    : RuntimeApplicationBase(std::move(cast_session_id),
                             std::move(app_config),
                             mojom::RendererType::MOJO_RENDERER,
                             web_service,
                             std::move(task_runner),
                             std::move(runtime_application_factory)),
      application_client_(application_client) {}

StreamingRuntimeApplication::~StreamingRuntimeApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(cast::common::StopReason::USER_REQUEST, net::OK);
}

bool StreamingRuntimeApplication::OnMessagePortMessage(
    cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!message_port_service_) {
    return false;
  }
  return message_port_service_->HandleMessage(std::move(message));
}

void StreamingRuntimeApplication::OnStreamingSessionStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPageLoaded();
}

void StreamingRuntimeApplication::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "Streaming session for " << *this << " has hit an error!";
  StopApplication(cast::common::StopReason::RUNTIME_ERROR, net::ERR_FAILED);
}

void StreamingRuntimeApplication::StartAvSettingsQuery(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connect the port to allow for sending messages. Querying will be done by
  // the associated |receiver_session_client_|.
  message_port_service_->ConnectToPortAsync(kMediaCapabilitiesBindingName,
                                            std::move(message_port));
}

void StreamingRuntimeApplication::OnResolutionChanged(
    const gfx::Rect& size,
    const ::media::VideoTransformation& transformation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  application_client_->OnStreamingResolutionChanged(size, transformation);
}

void StreamingRuntimeApplication::OnApplicationLaunched() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  message_port_service_ = application_platform().CreateMessagePortService();

  // Bind Cast Transport.
  std::unique_ptr<cast_api_bindings::MessagePort> server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&client_port, &server_port);
  message_port_service_->ConnectToPortAsync(kCastTransportBindingName,
                                            std::move(client_port));

  // Initialize the streaming receiver.
  receiver_session_client_ = std::make_unique<StreamingReceiverSessionClient>(
      task_runner(), application_client_->GetNetworkContextGetter(),
      std::move(server_port), cast_web_contents()->web_contents(), this,
      /* supports_audio= */ config().app_id() !=
          openscreen::cast::GetIosAppStreamingAudioVideoAppId(),
      /* supports_video= */ true);
  receiver_session_client_->LaunchStreamingReceiverAsync();

  // Application is initialized now - we can load the URL.
  LoadPage(GURL(base::StringPrintf(
      kStreamingPageUrlTemplate,
      cast_streaming::GetCastStreamingMediaSourceUrl().spec().c_str())));
}

void StreamingRuntimeApplication::StopApplication(
    cast::common::StopReason::Type stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!receiver_session_client_) {
    DLOG(INFO) << "Streaming session never started prior to " << *this
               << " stop.";
  }

  receiver_session_client_.reset();
  RuntimeApplicationBase::StopApplication(stop_reason, net_error_code);
  message_port_service_.reset();
}

bool StreamingRuntimeApplication::IsStreamingApplication() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

}  // namespace chromecast
