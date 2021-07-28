// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/web_runtime_application_service.h"

#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/cast_core/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/url_rewrite_rules_adapter.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/grpc/src/include/grpcpp/channel.h"
#include "third_party/grpc/src/include/grpcpp/create_channel.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"
#include "third_party/openscreen/src/cast/cast_core/api/runtime/runtime_service.grpc.pb.h"

namespace chromecast {

WebRuntimeApplicationService::WebRuntimeApplicationService(
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : GrpcServer(std::move(task_runner)), web_service_(web_service) {}

WebRuntimeApplicationService::~WebRuntimeApplicationService() {
  if (cast_session_id_.empty()) {
    return;
  }

  if (core_app_stub_) {
    grpc::ClientContext context;
    cast::v2::ApplicationStatusRequest status;
    cast::v2::ApplicationStatusResponse unused;
    status.set_cast_session_id(cast_session_id_);
    status.set_state(cast::v2::ApplicationStatusRequest_State_STOPPED);
    status.set_stop_reason(
        cast::v2::ApplicationStatusRequest_StopReason_USER_REQUEST);
    core_app_stub_->SetApplicationStatus(&context, status, &unused);
  }

  if (cast_web_view_) {
    cast_web_view_->cast_web_contents()->ClosePage();
  }
  GrpcServer::Stop();
}

bool WebRuntimeApplicationService::Load(
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

  cast_session_id_ = request.cast_session_id();
  app_id_ = request.application_config().app_id();
  url_rewrite_adapter_ =
      std::make_unique<UrlRewriteRulesAdapter>(request.url_rewrite_rules());
  display_name_ = request.application_config().display_name();
  app_url_ = request.application_config().cast_web_app_config().url();

  return true;
}

bool WebRuntimeApplicationService::Launch(
    const cast::runtime::LaunchApplicationRequest& request) {
  if (!request.has_cast_media_service_info()) {
    return false;
  }

  cast_media_service_grpc_endpoint_ =
      request.cast_media_service_info().grpc_endpoint();

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRuntimeApplicationService::FinishLaunch,
                     base::Unretained(this),
                     request.core_application_service_info().grpc_endpoint()));
  return true;
}

void WebRuntimeApplicationService::SetUrlRewriteRules(
    const cast::v2::SetUrlRewriteRulesRequest& request,
    cast::v2::SetUrlRewriteRulesResponse* response,
    GrpcMethod* callback) {
  if (cast_session_id_.empty()) {
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

void WebRuntimeApplicationService::PostMessage(
    const cast::web::Message& request,
    cast::web::MessagePortStatus* response,
    GrpcMethod* callback) {
  if (cast_session_id_.empty()) {
    callback->StepGRPC(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                    "No active cast session for PostMessage"));
    return;
  }
  bindings_manager_->HandleMessage(request, response);
  callback->StepGRPC(grpc::Status::OK);
}

void WebRuntimeApplicationService::OnWindowDestroyed() {}

bool WebRuntimeApplicationService::CanHandleGesture(GestureType gesture_type) {
  return false;
}

void WebRuntimeApplicationService::ConsumeGesture(
    GestureType gesture_type,
    GestureHandledCallback handled_callback) {
  std::move(handled_callback).Run(false);
}

void WebRuntimeApplicationService::OnVisibilityChange(
    VisibilityType visibility_type) {}

void WebRuntimeApplicationService::RenderFrameCreated(
    int render_process_id,
    int render_frame_id,
    service_manager::InterfaceProvider* frame_interfaces,
    blink::AssociatedInterfaceProvider* frame_associated_interfaces) {
  mojo::AssociatedRemote<mojom::IdentificationSettingsManager>
      remote_settings_manager;
  frame_associated_interfaces->GetInterface(&remote_settings_manager);
  url_rewrite_adapter_->AddRenderFrame(std::move(remote_settings_manager));
}

void WebRuntimeApplicationService::FinishLaunch(
    const std::string& core_application_service_address) {
  auto core_channel = grpc::CreateChannel(core_application_service_address,
                                          grpc::InsecureChannelCredentials());
  core_app_stub_ =
      cast::v2::CoreApplicationService::NewStub(std::move(core_channel));

  cast::bindings::GetAllResponse bindings_response;
  {
    grpc::ClientContext context;
    cast::bindings::GetAllRequest bindings_request;
    grpc::Status bindings_status =
        core_app_stub_->GetAll(&context, bindings_request, &bindings_response);
  }

  CastWebView::CreateParams create_params;
  create_params.delegate = weak_factory_.GetWeakPtr();
  create_params.web_contents_delegate = weak_factory_.GetWeakPtr();
  create_params.window_delegate = weak_factory_.GetWeakPtr();

  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->enabled_for_dev = true;
  params->renderer_type = mojom::RendererType::MOJO_RENDERER;

  cast_web_view_ =
      web_service_->CreateWebViewInternal(create_params, std::move(params));

  CastWebContents::Observer::Observe(cast_web_view_->cast_web_contents());

  bindings_manager_ = std::make_unique<BindingsManagerWebRuntime>(
      grpc_cq_, core_app_stub_.get());
  for (int i = 0; i < bindings_response.bindings_size(); ++i) {
    bindings_manager_->AddBinding(
        bindings_response.bindings(i).before_load_script());
  }
  cast_web_view_->cast_web_contents()->ConnectToBindingsService(
      bindings_manager_->CreateRemote());

  cast_web_view_->cast_web_contents()->LoadUrl(GURL(app_url_));
  cast_web_view_->window()->GrantScreenAccess();
  cast_web_view_->window()->CreateWindow(
      ::chromecast::mojom::ZOrder::APP,
      chromecast::VisibilityPriority::STICKY_ACTIVITY);

  {
    grpc::ClientContext context;
    cast::v2::ApplicationStatusRequest app_status;
    cast::v2::ApplicationStatusResponse unused;
    app_status.set_cast_session_id(cast_session_id_);
    app_status.set_state(cast::v2::ApplicationStatusRequest_State_STARTED);
    grpc::Status status =
        core_app_stub_->SetApplicationStatus(&context, app_status, &unused);
    DCHECK(status.ok());
  }
}

}  // namespace chromecast
