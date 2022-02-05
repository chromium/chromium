// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"

#include "base/bind.h"
#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"

namespace chromecast {

RuntimeApplicationBase::RuntimeApplicationBase(
    std::string cast_session_id,
    cast::common::ApplicationConfig app_config,
    mojom::RendererType renderer_type_used,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
      web_service_(web_service),
      task_runner_(std::move(task_runner)),
      renderer_type_(renderer_type_used) {
  DCHECK(web_service_);
  DCHECK(task_runner_);
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_application_running_);
}

CastWebContents* RuntimeApplicationBase::GetCastWebContents() {
  DCHECK(cast_web_view_);
  return cast_web_view_->cast_web_contents();
}

const std::string& RuntimeApplicationBase::GetCastMediaServiceEndpoint() const {
  DCHECK(cast_media_service_grpc_endpoint_);
  return *cast_media_service_grpc_endpoint_;
}

const cast::common::ApplicationConfig& RuntimeApplicationBase::GetAppConfig()
    const {
  return app_config_;
}

const std::string& RuntimeApplicationBase::GetCastSessionId() const {
  return cast_session_id_;
}

void RuntimeApplicationBase::Load(cast::runtime::LoadApplicationRequest request,
                                  StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  if (request.runtime_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Application service endpoint is missing"));
    return;
  }

  LOG(INFO) << "Loading application: " << *this;
  is_application_running_ = true;

  // Start the gRPC server.
  grpc_server_.emplace();
  grpc_server_->SetHandler<
      cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationBase::HandleSetUrlRewriteRules,
                              weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationBase::HandlePostMessage,
                              weak_factory_.GetWeakPtr())));
  grpc_server_->Start(
      request.runtime_application_service_info().grpc_endpoint());
  LOG(INFO) << "Runtime application server started: " << *this << ", endpoint="
            << request.runtime_application_service_info().grpc_endpoint();

  // Initialize web view.
  CreateCastWebView();

  // Set initial URL rewrite rules.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          request.url_rewrite_rules());
  GetCastWebContents()->SetUrlRewriteRules(std::move(mojom_rules));

  LOG(INFO) << "Successfully loaded: " << *this;
  std::move(callback).Run(grpc::Status::OK);
}

void RuntimeApplicationBase::Launch(
    cast::runtime::LaunchApplicationRequest request,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Launching application: " << *this;

  if (request.core_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Core application service endpoint is missing"));
    return;
  }
  if (request.cast_media_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                         "Media service endpoint is missing"));
    return;
  }

  // Create stubs for Core*ApplicationServices.
  auto core_channel = grpc::CreateChannel(
      request.core_application_service_info().grpc_endpoint(),
      grpc::InsecureChannelCredentials());
  core_app_stub_.emplace(core_channel);
  core_message_port_app_stub_.emplace(core_channel);

  // Initialize MZ data.
  cast_media_service_grpc_endpoint_.emplace(
      request.cast_media_service_info().grpc_endpoint());

  // Report that Cast application launch is initiated.
  std::move(callback).Run(grpc::Status::OK);

  // Initiate application initialization flow where bindings and any extra setup
  // happens.
  InitializeApplication(
      base::BindOnce(&RuntimeApplicationBase::OnApplicationInitialized,
                     weak_factory_.GetWeakPtr()));
}

void RuntimeApplicationBase::OnApplicationInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Load the application URL (now that we have the bindings set up).
  const std::vector<int32_t> feature_permissions;
  const std::vector<std::string> additional_feature_permission_origins;
  // TODO(b/203580094): Currently we assume the app is not audio only.
  GetCastWebContents()->SetAppProperties(
      GetAppConfig().app_id(), GetCastSessionId(), false /*is_audio_app*/,
      GetApplicationUrl(), false /*enforce_feature_permissions*/,
      feature_permissions, additional_feature_permission_origins);
  GetCastWebContents()->LoadUrl(GetApplicationUrl());

  // Show the web view.
  cast_web_view_->window()->GrantScreenAccess();
  cast_web_view_->window()->CreateWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);
}

void RuntimeApplicationBase::HandlePostMessage(
    cast::web::Message request,
    cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_application_running_) {
    reactor->Write(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                "No active cast session for PostMessage"));
    return;
  }

  auto response_or = HandlePortMessage(std::move(request));
  if (response_or.ok()) {
    reactor->Write(std::move(response_or).value());
  } else {
    reactor->Write(response_or.status());
  }
}

void RuntimeApplicationBase::HandleSetUrlRewriteRules(
    cast::v2::SetUrlRewriteRulesRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_application_running_) {
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND,
                     "No active cast session for SetUrlRewriteRules"));
    return;
  }
  if (request.has_rules()) {
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
            request.rules());
    GetCastWebContents()->SetUrlRewriteRules(std::move(mojom_rules));
  }
  reactor->Write(cast::v2::SetUrlRewriteRulesResponse());
}

void RuntimeApplicationBase::SetApplicationState(
    cast::v2::ApplicationStatusRequest::State state,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(state == cast::v2::ApplicationStatusRequest::STARTED ||
         state == cast::v2::ApplicationStatusRequest::STOPPED);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::SetApplicationStatus>();
  call.request().set_cast_session_id(GetCastSessionId());
  call.request().set_state(state);
  if (state == cast::v2::ApplicationStatusRequest::STOPPED) {
    call.request().set_stop_reason(
        cast::v2::ApplicationStatusRequest::USER_REQUEST);
  }
  std::move(call).InvokeAsync(base::BindOnce(
      [](StatusCallback callback,
         cast::utils::GrpcStatusOr<cast::v2::ApplicationStatusResponse>
             response_or) { std::move(callback).Run(response_or.status()); },
      std::move(callback)));
}

void RuntimeApplicationBase::CreateCastWebView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->renderer_type = renderer_type_;
  params->handle_inner_contents = true;
  params->session_id = GetCastSessionId();
#if DCHECK_IS_ON()
  params->enabled_for_dev = true;
#endif
  cast_web_view_ = web_service_->CreateWebViewInternal(std::move(params));
}

void RuntimeApplicationBase::StopApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(grpc_server_);

  LOG(INFO) << "Stopping application: " << *this;

  if (!is_application_running_) {
    return;
  }
  is_application_running_ = false;

  if (core_app_stub_) {
    SetApplicationState(cast::v2::ApplicationStatusRequest_State_STOPPED,
                        base::DoNothing());
  }

  if (cast_web_view_) {
    GetCastWebContents()->ClosePage();
  }

  grpc_server_->Stop();
  grpc_server_.reset();
  LOG(INFO) << "Application is stopped: " << *this;
}

}  // namespace chromecast
