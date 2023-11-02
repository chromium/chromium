// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_platform_grpc.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/browser/visibility_types.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_factory.h"
#include "chromecast/cast_core/runtime/browser/message_port_service_grpc.h"
#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"
#include "chromecast/common/feature_constants.h"

namespace chromecast {

RuntimeApplicationPlatformGrpc::RuntimeApplicationPlatformGrpc(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::string session_id,
    Client& client)
    : client_(client),
      session_id_(std::move(session_id)),
      task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_);
}

RuntimeApplicationPlatformGrpc::~RuntimeApplicationPlatformGrpc() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RuntimeApplicationPlatformGrpc::Load(
    cast::runtime::LoadApplicationRequest request,
    LoadCompleteCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  if (request.runtime_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(false);
    return;
  }

  // Start the gRPC server.
  grpc_server_.emplace();
  grpc_server_->SetHandler<
      cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(
              &RuntimeApplicationPlatformGrpc::HandleSetUrlRewriteRules,
              weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetMediaState>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationPlatformGrpc::HandleSetMediaState,
                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetVisibility>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationPlatformGrpc::HandleSetVisibility,
                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetTouchInput>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(
                  &RuntimeApplicationPlatformGrpc::HandleSetTouchInput,
                  weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage>(
      base::BindPostTask(task_runner_,
                         base::BindRepeating(
                             &RuntimeApplicationPlatformGrpc::HandlePostMessage,
                             weak_factory_.GetWeakPtr())));
  grpc_server_->Start(
      request.runtime_application_service_info().grpc_endpoint());
  LOG(INFO) << "Runtime application server started: endpoint="
            << request.runtime_application_service_info().grpc_endpoint();

  std::move(callback).Run(true);

  // Set initial URL rewrite rules.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          request.url_rewrite_rules());
  client_->OnUrlRewriteRulesSet(std::move(mojom_rules));
}

void RuntimeApplicationPlatformGrpc::Launch(
    cast::runtime::LaunchApplicationRequest request,
    LaunchCompleteCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (request.core_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(false);
    return;
  }
  if (request.cast_media_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(false);
    return;
  }

  // Create stubs for Core*ApplicationServices.
  auto core_channel = grpc::CreateChannel(
      request.core_application_service_info().grpc_endpoint(),
      grpc::InsecureChannelCredentials());
  core_app_stub_.emplace(core_channel);
  core_message_port_app_stub_.emplace(core_channel);

  // TODO(b/244455581): Configure multizone.

  client_->OnMediaStateSet(request.media_state());
  client_->OnVisibilitySet(request.visibility());
  client_->OnTouchInputSet(request.touch_input());

  // Report that Cast application launch is initiated.
  std::move(callback).Run(true);
}

void RuntimeApplicationPlatformGrpc::HandlePostMessage(
    cast::web::Message request,
    cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client_->IsApplicationRunning()) {
    reactor->Write(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                "No active cast session for PostMessage"));
    return;
  }

  const auto success = client_->OnMessagePortMessage(std::move(request));
  if (success) {
    cast::web::MessagePortStatus message_port_status;
    message_port_status.set_status(cast::web::MessagePortStatus::OK);
    reactor->Write(std::move(message_port_status));
  } else {
    reactor->Write(
        grpc::Status(grpc::StatusCode::UNKNOWN, "Failed to post message"));
  }
}

void RuntimeApplicationPlatformGrpc::HandleSetUrlRewriteRules(
    cast::v2::SetUrlRewriteRulesRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!client_->IsApplicationRunning()) {
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND,
                     "No active cast session for SetUrlRewriteRules"));
    return;
  }
  if (request.has_rules()) {
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
            request.rules());
    client_->OnUrlRewriteRulesSet(std::move(mojom_rules));
  }
  reactor->Write(cast::v2::SetUrlRewriteRulesResponse());
}

void RuntimeApplicationPlatformGrpc::HandleSetMediaState(
    cast::v2::SetMediaStateRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetMediaState::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnMediaStateSet(request.media_state());
  reactor->Write(cast::v2::SetMediaStateResponse());
}

void RuntimeApplicationPlatformGrpc::HandleSetVisibility(
    cast::v2::SetVisibilityRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetVisibility::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnVisibilitySet(request.visibility());
  reactor->Write(cast::v2::SetVisibilityResponse());
}

void RuntimeApplicationPlatformGrpc::HandleSetTouchInput(
    cast::v2::SetTouchInputRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetTouchInput::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnTouchInputSet(request.touch_input());
  reactor->Write(cast::v2::SetTouchInputResponse());
}

void RuntimeApplicationPlatformGrpc::NotifyApplicationStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::ApplicationStarted>();
  call.request().set_cast_session_id(session_id_);
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::ApplicationStartedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report that application started: "
            << response_or.ToString();
      }));
}

void RuntimeApplicationPlatformGrpc::NotifyApplicationStopped(
    cast::common::StopReason::Type stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::ApplicationStopped>();
  call.request().set_cast_session_id(session_id_);
  call.request().set_stop_reason(stop_reason);
  call.request().set_error_code(net_error_code);
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::ApplicationStoppedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report that application stopped: "
            << response_or.ToString();
      }));

  grpc_server_->Stop();
  grpc_server_.reset();
}

void RuntimeApplicationPlatformGrpc::NotifyMediaPlaybackChanged(bool playing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::MediaPlaybackChanged>();
  call.request().set_cast_session_id(session_id_);
  call.request().set_media_playback_state(
      playing ? cast::common::MediaPlaybackState::PLAYING
              : cast::common::MediaPlaybackState::STOPPED);
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::MediaPlaybackChangedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report media playback changed state: "
            << response_or.ToString();
      }));
}

void RuntimeApplicationPlatformGrpc::GetAllBindingsAsync(
    GetAllBindingsCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_message_port_app_stub_);
  auto call = core_message_port_app_stub_->CreateCall<
      cast::v2::CoreMessagePortApplicationServiceStub::GetAll>();
  std::move(call).InvokeAsync(base::BindPostTask(
      task_runner_,
      base::BindOnce(&RuntimeApplicationPlatformGrpc::OnAllBindingsReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback))));
}

std::unique_ptr<MessagePortService>
RuntimeApplicationPlatformGrpc::CreateMessagePortService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_message_port_app_stub_);
  return std::make_unique<MessagePortServiceGrpc>(
      &core_message_port_app_stub_.value());
}

std::unique_ptr<content::WebUIControllerFactory>
RuntimeApplicationPlatformGrpc::CreateWebUIControllerFactory(
    std::vector<std::string> hosts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);
  return std::make_unique<GrpcWebUiControllerFactory>(std::move(hosts),
                                                      &core_app_stub_.value());
}

void RuntimeApplicationPlatformGrpc::OnAllBindingsReceived(
    GetAllBindingsCB callback,
    cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or) {
  if (!response_or.ok()) {
    std::move(callback).Run(absl::nullopt);
  } else {
    std::move(callback).Run(std::move(response_or).value());
  }
}

}  // namespace chromecast
