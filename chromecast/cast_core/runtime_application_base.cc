// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime_application_base.h"

#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/cast_core/grpc_method.h"
#include "chromecast/cast_core/url_rewrite_rules_adapter.h"
#include "third_party/cast_core/public/src/proto/runtime/runtime_service.grpc.pb.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {

RuntimeApplicationBase::RuntimeApplicationBase(
    mojom::RendererType renderer_type_used,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : GrpcServer(task_runner),
      web_service_(web_service),
      task_runner_(std::move(task_runner)),
      renderer_type_(renderer_type_used) {
  DCHECK(web_service_);
  DCHECK(task_runner_);
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  CHECK(is_application_stopped_);
}

bool RuntimeApplicationBase::Load(
    const cast::runtime::LoadApplicationRequest& request) {
  if (!request.has_application_config() ||
      !request.has_runtime_application_service_info()) {
    return false;
  }

  set_application_config(request.application_config());
  set_cast_session_id(request.cast_session_id());

  LOG(INFO) << "Loading application: " << *this;

  const std::string& grpc_address =
      request.runtime_application_service_info().grpc_endpoint();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(grpc_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&grpc_app_service_);
  builder.RegisterService(&grpc_message_port_service_);
  SetCompletionQueue(builder.AddCompletionQueue());
  SetServer(builder.BuildAndStart());
  if (!grpc_server_) {
    LOG(ERROR) << "Failed to start server on path: " << *this
               << ", endpoint=" << grpc_address;
    return false;
  }
  StartRuntimeApplicationServiceMethods(
      &grpc_app_service_, weak_factory_.GetWeakPtr(), grpc_cq_, &is_shutdown_);
  StartRuntimeMessagePortApplicationServiceMethods(&grpc_message_port_service_,
                                                   weak_factory_.GetWeakPtr(),
                                                   grpc_cq_, &is_shutdown_);
  GrpcServer::Start();
  LOG(INFO) << "Runtime application server started: " << *this
            << ", endpoint=" << grpc_address;

  CreateCastWebView();
  url_rewrite_adapter_ =
      std::make_unique<UrlRewriteRulesAdapter>(request.url_rewrite_rules());

  auto* web_service = cast_web_service();
  if (web_service) {
    MojoIdentificationSettings params(request.url_rewrite_rules());
    web_service->CreateSessionWithSubstitutions(
        cast_session_id(), std::move(params.substitutable_params));
    web_service->UpdateAppSettingsForSession(
        cast_session_id(), std::move(params.application_settings));
    web_service->UpdateDeviceSettingsForSession(
        cast_session_id(), std::move(params.device_settings));
    LOG(INFO) << "CastWebService initialized: " << *this;
  }

  LOG(INFO) << "Successfully loaded: " << *this;

  return true;
}

bool RuntimeApplicationBase::Launch(
    const cast::runtime::LaunchApplicationRequest& request) {
  LOG(INFO) << "Launching application: " << *this;

  if (!request.has_cast_media_service_info()) {
    return false;
  }

  set_cast_media_service_grpc_endpoint(
      request.cast_media_service_info().grpc_endpoint());

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RuntimeApplicationBase::FinishLaunch,
                     weak_factory_.GetWeakPtr(),
                     request.core_application_service_info().grpc_endpoint()));
  return true;
}

void RuntimeApplicationBase::FinishLaunch(
    std::string core_application_service_endpoint) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto core_channel = grpc::CreateChannel(core_application_service_endpoint,
                                          grpc::InsecureChannelCredentials());
  core_app_stub_ =
      cast::v2::CoreApplicationService::NewStub(std::move(core_channel));

  GURL cast_application_url = InitializeAndGetInitialURL(
      core_app_stub_.get(), cast_web_view_->cast_web_contents());
  LOG(INFO) << "Initialized initial URL: " << *this
            << ", url=" << cast_application_url;
  const std::vector<int32_t> feature_permissions;
  const std::vector<std::string> additional_feature_permission_origins;
  // TODO(b/203580094): Currently we assume the app is not audio only.
  cast_web_view_->cast_web_contents()->SetAppProperties(
      app_config().app_id(), cast_session_id(), false /*is_audio_app*/,
      cast_application_url, false /*enforce_feature_permissions*/,
      feature_permissions, additional_feature_permission_origins);
  cast_web_view_->cast_web_contents()->LoadUrl(cast_application_url);
  cast_web_view_->window()->GrantScreenAccess();
  cast_web_view_->window()->CreateWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);

  LOG(INFO) << "Launch finished: " << *this;
}

void RuntimeApplicationBase::PostMessage(const cast::web::Message& request,
                                         cast::web::MessagePortStatus* response,
                                         GrpcMethod* callback) {
  if (cast_session_id().empty()) {
    callback->StepGRPC(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                    "No active cast session for PostMessage"));
    return;
  }
  HandleMessage(request, response);
  callback->StepGRPC(grpc::Status::OK);
}

void RuntimeApplicationBase::SetUrlRewriteRules(
    const cast::v2::SetUrlRewriteRulesRequest& request,
    cast::v2::SetUrlRewriteRulesResponse* response,
    GrpcMethod* callback) {
  if (cast_session_id().empty()) {
    callback->StepGRPC(
        grpc::Status(grpc::StatusCode::NOT_FOUND,
                     "No active cast session for SetUrlRewriteRules"));
    return;
  }
  if (request.has_rules()) {
    url_rewrite_adapter_->UpdateRules(request.rules());
  }
  callback->StepGRPC(grpc::Status::OK);
}

void RuntimeApplicationBase::SetApplicationStarted() {
  grpc::ClientContext context;
  cast::v2::ApplicationStatusRequest app_status;
  cast::v2::ApplicationStatusResponse unused;
  app_status.set_cast_session_id(cast_session_id());
  app_status.set_state(cast::v2::ApplicationStatusRequest_State_STARTED);
  grpc::Status status =
      core_app_stub_->SetApplicationStatus(&context, app_status, &unused);

  if (!status.ok()) {
    LOG(ERROR) << "Failed to call SetApplicationStatus() when starting: "
               << *this << ", status=" << status.error_message();
  } else {
    LOG(INFO) << "Application is started: " << *this;
  }
}

void RuntimeApplicationBase::CreateCastWebView() {
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
  LOG(INFO) << "Stopping application: " << *this;

  is_application_stopped_ = true;
  if (cast_session_id().empty()) {
    return;
  }

  if (core_app_stub_) {
    grpc::ClientContext context;
    cast::v2::ApplicationStatusRequest app_status;
    cast::v2::ApplicationStatusResponse unused;
    app_status.set_cast_session_id(cast_session_id());
    app_status.set_state(cast::v2::ApplicationStatusRequest_State_STOPPED);
    app_status.set_stop_reason(
        cast::v2::ApplicationStatusRequest_StopReason_USER_REQUEST);
    grpc::Status status =
        core_app_stub_->SetApplicationStatus(&context, app_status, &unused);

    if (!status.ok()) {
      LOG(ERROR) << "Failed to call SetApplicationStatus() when stopping: "
                 << *this;
    }
  }

  if (cast_web_view_) {
    cast_web_view_->cast_web_contents()->ClosePage();
  }

  if (web_service_) {
    web_service_->OnSessionDestroyed(cast_session_id());
  }

  GrpcServer::Stop();
  LOG(INFO) << "Application is stopped: " << *this;

  set_cast_session_id(std::string());
}

}  // namespace chromecast
