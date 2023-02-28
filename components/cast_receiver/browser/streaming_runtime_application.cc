// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_runtime_application.h"

#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/application_client.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_streaming/common/public/app_ids.h"
#include "components/cast_streaming/common/public/cast_streaming_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace cast_receiver {
namespace {

constexpr char kCastTransportBindingName[] = "cast.__platform__.cast_transport";

constexpr char kStreamingPageUrlTemplate[] =
    "data:text/html;charset=UTF-8, <video style='position:absolute; "
    "top:50%%; left:50%%; transform:translate(-50%%,-50%%); "
    "max-width:100%%; max-height:100%%; min-width: 100%%; min-height: 100%%' "
    "src='%s'></video>";

}  // namespace

StreamingRuntimeApplication::StreamingRuntimeApplication(
    std::string cast_session_id,
    ApplicationConfig app_config,
    ApplicationClient& application_client)
    : RuntimeApplicationBase(std::move(cast_session_id),
                             std::move(app_config),
                             application_client) {}

StreamingRuntimeApplication::~StreamingRuntimeApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(EmbedderApplication::ApplicationStopReason::kUserRequest,
                  net::OK);
}

void StreamingRuntimeApplication::OnStreamingSessionStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPageNavigationComplete();
}

void StreamingRuntimeApplication::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(WARNING) << "Streaming session for " << *this << " has hit an error!";
  StopApplication(EmbedderApplication::ApplicationStopReason::kRuntimeError,
                  net::ERR_FAILED);
}

void StreamingRuntimeApplication::OnResolutionChanged(
    const gfx::Rect& size,
    const media::VideoTransformation& transformation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  application_client().OnStreamingResolutionChanged(size, transformation);
}

void StreamingRuntimeApplication::Launch(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(embedder_application().GetWebContents());
  SetContentPermissions(*embedder_application().GetWebContents());

  // Bind Cast Transport.
  auto* message_port_service = embedder_application().GetMessagePortService();
  DCHECK(message_port_service);
  std::unique_ptr<cast_api_bindings::MessagePort> server_port;
  std::unique_ptr<cast_api_bindings::MessagePort> client_port;
  cast_api_bindings::CreatePlatformMessagePortPair(&client_port, &server_port);
  message_port_service->ConnectToPortAsync(kCastTransportBindingName,
                                           std::move(client_port));

  // Initialize the streaming receiver.
  receiver_session_client_ = std::make_unique<StreamingReceiverSessionClient>(
      task_runner(), application_client().network_context_getter(),
      std::move(server_port), embedder_application().GetWebContents(), this,
      embedder_application().GetStreamingConfigManager(),
      /* supports_audio= */ GetAppId() !=
          cast_streaming::GetIosAppStreamingAudioVideoAppId(),
      /* supports_video= */ true);
  receiver_session_client_->LaunchStreamingReceiverAsync();

  // Application is initialized now - we can load the URL.
  NavigateToPage(GURL(base::StringPrintf(
      kStreamingPageUrlTemplate,
      cast_streaming::GetCastStreamingMediaSourceUrl().spec().c_str())));

  // Signal that application is launching.
  std::move(callback).Run(OkStatus());
}

void StreamingRuntimeApplication::StopApplication(
    EmbedderApplication::ApplicationStopReason stop_reason,
    net::Error net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!receiver_session_client_) {
    DLOG(WARNING) << "Streaming session never started prior to " << *this
                  << " stop.";
  }

  receiver_session_client_.reset();
  RuntimeApplicationBase::StopApplication(stop_reason, net_error_code);
}

bool StreamingRuntimeApplication::IsStreamingApplication() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

}  // namespace cast_receiver
