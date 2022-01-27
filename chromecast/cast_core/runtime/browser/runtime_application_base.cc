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
    cast::common::ApplicationConfig app_config,
    mojom::RendererType renderer_type_used,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : web_service_(web_service),
      task_runner_(std::move(task_runner)),
      renderer_type_(renderer_type_used) {
  DCHECK(web_service_);
  DCHECK(task_runner_);

  set_application_config(std::move(app_config));
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(is_application_stopped_);
}

CastWebContents* RuntimeApplicationBase::GetCastWebContents() {
  DCHECK(cast_web_view_);
  return cast_web_view_->cast_web_contents();
}

void RuntimeApplicationBase::Load(cast::runtime::LoadApplicationRequest request,
                                  StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  if (request.cast_session_id().empty()) {
    std::move(callback).Run(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                         "Application session ID is missing"));
    return;
  }
  if (!request.has_application_config()) {
    std::move(callback).Run(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                         "Application config is missing"));
    return;
  }
  if (request.runtime_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Application service endpoint is missing"));
    return;
  }

  set_application_config(request.application_config());
  set_cast_session_id(request.cast_session_id());

  LOG(INFO) << "Loading application: " << *this;

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

  // Initialize web view and URL rewrites.
  CreateCastWebView();
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

  auto core_channel = grpc::CreateChannel(
      request.core_application_service_info().grpc_endpoint(),
      grpc::InsecureChannelCredentials());
  core_app_stub_.emplace(core_channel);
  core_message_port_app_stub_.emplace(core_channel);

  set_cast_media_service_grpc_endpoint(
      request.cast_media_service_info().grpc_endpoint());

  InitializeApplication(base::BindPostTask(
      task_runner_,
      base::BindOnce(&RuntimeApplicationBase::OnApplicationInitialized,
                     weak_factory_.GetWeakPtr(), std::move(callback))));
}

void RuntimeApplicationBase::OnApplicationInitialized(StatusCallback callback,
                                                      grpc::Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!app_url().is_empty());

  if (!status.ok()) {
    LOG(ERROR) << "Failed to launch application: " << *this
               << ", status=" << cast::utils::GrpcStatusToString(status);
    std::move(callback).Run(status);
    return;
  }

  const std::vector<int32_t> feature_permissions;
  const std::vector<std::string> additional_feature_permission_origins;
  // TODO(b/203580094): Currently we assume the app is not audio only.
  GetCastWebContents()->SetAppProperties(
      app_config().app_id(), cast_session_id(), false /*is_audio_app*/,
      app_url(), false /*enforce_feature_permissions*/, feature_permissions,
      additional_feature_permission_origins);
  GetCastWebContents()->LoadUrl(app_url());
  cast_web_view_->window()->GrantScreenAccess();
  cast_web_view_->window()->CreateWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);

  LOG(INFO) << "Application is launched: " << *this;
  std::move(callback).Run(grpc::Status::OK);
}

void RuntimeApplicationBase::HandlePostMessage(
    cast::web::Message request,
    cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (cast_session_id().empty()) {
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
  if (cast_session_id().empty()) {
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
  call.request().set_cast_session_id(cast_session_id());
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
  params->session_id = cast_session_id();
#if DCHECK_IS_ON()
  params->enabled_for_dev = true;
#endif

  cast_web_view_ = web_service_->CreateWebViewInternal(std::move(params));
}

void RuntimeApplicationBase::StopApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(grpc_server_);

  LOG(INFO) << "Stopping application: " << *this;

  is_application_stopped_ = true;
  if (cast_session_id().empty()) {
    return;
  }

  if (core_app_stub_) {
    SetApplicationState(cast::v2::ApplicationStatusRequest_State_STOPPED,
                        base::DoNothing());
  }

  if (cast_web_view_) {
    GetCastWebContents()->ClosePage();
  }

  if (web_service_) {
    web_service_->OnSessionDestroyed(cast_session_id());
  }

  grpc_server_->Stop();
  grpc_server_.reset();
  LOG(INFO) << "Application is stopped: " << *this;

  set_cast_session_id(std::string());
}

}  // namespace chromecast
