// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/streaming_runtime_application.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "chromecast/media/base/video_plane_controller.h"
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
    std::string cast_session_id,
    cast::common::ApplicationConfig app_config,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    cast_streaming::NetworkContextGetter network_context_getter,
    media::VideoPlaneController* video_plane_controller)
    : RuntimeApplicationBase(std::move(cast_session_id),
                             std::move(app_config),
                             mojom::RendererType::MOJO_RENDERER,
                             web_service,
                             std::move(task_runner)),
      video_plane_controller_(video_plane_controller),
      network_context_getter_(std::move(network_context_getter)) {
  DCHECK(video_plane_controller_);
}

StreamingRuntimeApplication::~StreamingRuntimeApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication();
}

const GURL& StreamingRuntimeApplication::GetApplicationUrl() const {
  static const GURL kStreamingUrl(base::StringPrintf(
      kStreamingPageUrlTemplate,
      cast_streaming::GetCastStreamingMediaSourceUrl().spec().c_str()));
  return kStreamingUrl;
}

cast::utils::GrpcStatusOr<cast::web::MessagePortStatus>
StreamingRuntimeApplication::HandlePortMessage(cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return message_port_service_->HandleMessage(std::move(message));
}

void StreamingRuntimeApplication::OnStreamingSessionStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetApplicationState(
      cast::v2::ApplicationStatusRequest::STARTED,
      base::BindPostTask(
          task_runner(),
          base::BindOnce(
              &StreamingRuntimeApplication::OnApplicationStateChanged,
              weak_factory_.GetWeakPtr())));
}

void StreamingRuntimeApplication::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "Streaming session for " << *this << " has hit an error!";
  StopApplication();
}

void StreamingRuntimeApplication::StartAvSettingsQuery(
    std::unique_ptr<cast_api_bindings::MessagePort> message_port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Connect the port to allow for sending messages. Querying will be done by
  // the associated |receiver_session_client_|.
  message_port_service_->ConnectToPort(kMediaCapabilitiesBindingName,
                                       std::move(message_port));
}

void StreamingRuntimeApplication::OnResolutionChanged(
    const gfx::Rect& size,
    const ::media::VideoTransformation& transformation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  video_plane_controller_->SetGeometryFromMediaType(size, transformation);
}

void StreamingRuntimeApplication::InitializeApplication(
    base::OnceClosure app_initialized_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  message_port_service_ =
      std::make_unique<MessagePortService>(core_message_port_app_stub());

  // Bind Cast Transport.
  std::unique_ptr<cast_api_bindings::MessagePort> server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&client_port, &server_port);
  message_port_service_->ConnectToPort(kCastTransportBindingName,
                                       std::move(client_port));

  // Initialize the streaming receiver.
  receiver_session_client_ = std::make_unique<StreamingReceiverSessionClient>(
      task_runner(), network_context_getter_, std::move(server_port),
      GetCastWebContents(), this,
      /* supports_audio= */ GetAppConfig().app_id() !=
          openscreen::cast::GetIosAppStreamingAudioVideoAppId(),
      /* supports_video= */ true);
  receiver_session_client_->LaunchStreamingReceiverAsync();

  // Application is initialized now.
  std::move(app_initialized_callback).Run();
}

void StreamingRuntimeApplication::StopApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!receiver_session_client_) {
    DLOG(INFO) << "Streaming session never started prior to " << *this
               << " stop.";
  }

  receiver_session_client_.reset();
  RuntimeApplicationBase::StopApplication();
  message_port_service_.reset();
}

bool StreamingRuntimeApplication::IsStreamingApplication() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void StreamingRuntimeApplication::OnApplicationStateChanged(
    grpc::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set application state to started: " << *this
               << ", status=" << cast::utils::GrpcStatusToString(status);
    StopApplication();
    return;
  }

  LOG(INFO) << "Cast streaming application started: " << *this;
}

}  // namespace chromecast
