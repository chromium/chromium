// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/streaming_runtime_application.h"

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "components/cast/message_port/platform_message_port.h"
#include "components/cast_receiver/browser/public/embedder_application.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/cast_receiver/browser/streaming_input_observer.h"
#include "components/cast_receiver/browser/streaming_receiver_channel.h"
#include "components/cast_streaming/common/public/app_ids.h"
#include "components/cast_streaming/common/public/cast_streaming_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/geometry/rect.h"

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

void StreamingRuntimeApplication::Launch(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(embedder_application().GetWebContents());
  SetContentPermissions(*embedder_application().GetWebContents());

  // Bind Cast Transport.
  auto* message_port_service = embedder_application().GetMessagePortService();
  CHECK(message_port_service);
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

  // If extended input is supported, also start bootstrap in parallel.
  if (config().is_extended_input_supported) {
    // Get display info.
    content::WebContents* web_contents =
        embedder_application().GetWebContents();
    CHECK(web_contents);
    gfx::Rect bounds = web_contents->GetContainerBounds();
    int width = bounds.width();
    int height = bounds.height();
    if (width <= 0 || height <= 0) {
      width = 1920;
      height = 1080;
    }
    DisplayInfo display_info;
    display_info.set_width_px(width);
    display_info.set_height_px(height);
    display_info.set_orientation(DisplayInfo::DEGREES_0);
    display_info.set_dpi(160);
    display_info.set_resizable(false);

    // Start bootstrap and initialize channel.
    streaming_receiver_channel_ = std::make_unique<StreamingReceiverChannel>(
        message_port_service, std::move(display_info),
        base::BindOnce(&StreamingRuntimeApplication::OnBootstrapComplete,
                       weak_factory_.GetWeakPtr()));
  }

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
  streaming_input_observer_.reset();
  streaming_input_capabilities_observer_.reset();
  streaming_receiver_channel_.reset();
  RuntimeApplicationBase::StopApplication(stop_reason, net_error_code);
}

bool StreamingRuntimeApplication::IsStreamingApplication() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

void StreamingRuntimeApplication::OnInputEvent(
    const cast_receiver::InputEvent& event) {
  if (streaming_receiver_channel_) {
    streaming_receiver_channel_->SendInputEvent(event);
  }
}

void StreamingRuntimeApplication::OnInputCapabilitiesChanged(
    const cast_receiver::InputCapabilities& caps) {
  if (streaming_receiver_channel_) {
    streaming_receiver_channel_->SendInputCapabilities(caps);
  }
}

void StreamingRuntimeApplication::OnBootstrapComplete(
    ExoBootstrapMessage request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Bootstrap complete";

  // Instantiate observers if input is supported.
  if (config().is_extended_input_supported) {
    LOG(INFO) << "Extended input is supported, setting up input observers.";

    streaming_input_observer_ = std::make_unique<StreamingInputObserver>(
        embedder_application().GetWebContents(),
        base::BindRepeating(&StreamingRuntimeApplication::OnInputEvent,
                            weak_factory_.GetWeakPtr()));

    if (ui::DeviceDataManager::HasInstance()) {
      streaming_input_capabilities_observer_ =
          std::make_unique<StreamingInputCapabilitiesObserver>(
              ui::DeviceDataManager::GetInstance(),
              base::BindRepeating(
                  &StreamingRuntimeApplication::OnInputCapabilitiesChanged,
                  weak_factory_.GetWeakPtr()));
    } else {
      LOG(INFO) << "DeviceDataManager instance is unavailable. "
                   "StreamingInputCapabilitiesObserver will not be created.";
    }
  }
}

}  // namespace cast_receiver
