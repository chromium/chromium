// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime_application_base.h"

#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/cast_core/grpc_method.h"
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

  const std::string& grpc_address =
      request.runtime_application_service_info().grpc_endpoint();
  grpc::ServerBuilder builder;
  builder.AddListeningPort(grpc_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&grpc_app_service_);
  builder.RegisterService(&grpc_message_port_service_);
  SetCompletionQueue(builder.AddCompletionQueue());
  SetServer(builder.BuildAndStart());
  if (!grpc_server_) {
    LOG(ERROR) << "Failed to start server on path: " << grpc_address;
    return false;
  }
  StartRuntimeApplicationServiceMethods(
      &grpc_app_service_, weak_factory_.GetWeakPtr(), grpc_cq_, &is_shutdown_);
  StartRuntimeMessagePortApplicationServiceMethods(&grpc_message_port_service_,
                                                   weak_factory_.GetWeakPtr(),
                                                   grpc_cq_, &is_shutdown_);
  GrpcServer::Start();

  set_cast_session_id(request.cast_session_id());
  set_app_id(request.application_config().app_id());
  set_display_name(request.application_config().display_name());

  LOG(INFO) << *this << " successfully loaded!";

  return true;
}

bool RuntimeApplicationBase::Launch(
    const cast::runtime::LaunchApplicationRequest& request) {
  LOG(INFO) << "Beginning launch of " << *this;

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

  DLOG(INFO) << *this << "creating web view...";
  cast_web_view_ = CreateWebView(core_app_stub_.get());

  DLOG(INFO) << *this << "processing web view...";
  GURL cast_application_url =
      ProcessWebView(core_app_stub_.get(), cast_web_view_->cast_web_contents());

  cast_web_view_->cast_web_contents()->LoadUrl(cast_application_url);
  cast_web_view_->window()->GrantScreenAccess();
  cast_web_view_->window()->CreateWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);

  LOG(INFO) << *this << " successfully launched!";
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
    LOG(ERROR) << "Failed to call SetApplicationStatus() when starting "
               << *this;
  }
}

CastWebView::Scoped RuntimeApplicationBase::CreateWebView(
    CoreApplicationServiceGrpc* grpc_stub) {
  CastWebView::CreateParams create_params;
  create_params.delegate = weak_factory_.GetWeakPtr();
  create_params.window_delegate = weak_factory_.GetWeakPtr();

  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
#if DCHECK_IS_ON()
  params->enabled_for_dev = true;
#endif
  params->renderer_type = renderer_type_;

  return web_service_->CreateWebViewInternal(create_params, std::move(params));
}

bool RuntimeApplicationBase::CanHandleGesture(GestureType gesture_type) {
  return false;
}

void RuntimeApplicationBase::StopApplication() {
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
      LOG(ERROR) << "Failed to call SetApplicationStatus() when starting "
                 << *this;
    }
  }

  if (cast_web_view_) {
    cast_web_view_->cast_web_contents()->ClosePage();
  }

  GrpcServer::Stop();
  set_cast_session_id(std::string());
}

void RuntimeApplicationBase::ConsumeGesture(
    GestureType gesture_type,
    GestureHandledCallback handled_callback) {
  std::move(handled_callback).Run(false);
}

}  // namespace chromecast
