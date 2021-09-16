// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/web_runtime_application.h"

#include "chromecast/cast_core/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/grpc_webui_controller_factory.h"
#include "chromecast/cast_core/url_rewrite_rules_adapter.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromecast {

WebRuntimeApplication::WebRuntimeApplication(
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RuntimeApplicationBase(mojom::RendererType::MOJO_RENDERER,
                             web_service,
                             std::move(task_runner)) {}

WebRuntimeApplication::~WebRuntimeApplication() {
  StopApplication();
}

bool WebRuntimeApplication::Load(
    const cast::runtime::LoadApplicationRequest& request) {
  if (!RuntimeApplicationBase::Load(request)) {
    return false;
  }

  url_rewrite_adapter_ =
      std::make_unique<UrlRewriteRulesAdapter>(request.url_rewrite_rules());
  app_url_ = request.application_config().cast_web_app_config().url();

  return true;
}

void WebRuntimeApplication::SetUrlRewriteRules(
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

  RuntimeApplicationBase::SetUrlRewriteRules(request, response, callback);
}

void WebRuntimeApplication::HandleMessage(
    const cast::web::Message& message,
    cast::web::MessagePortStatus* response) {
  bindings_manager_->HandleMessage(message, response);
}

void WebRuntimeApplication::RenderFrameCreated(
    int render_process_id,
    int render_frame_id,
    mojo::PendingAssociatedRemote<
        chromecast::mojom::IdentificationSettingsManager> settings_manager) {
  mojo::AssociatedRemote<mojom::IdentificationSettingsManager>
      remote_settings_manager(std::move(settings_manager));
  url_rewrite_adapter_->AddRenderFrame(std::move(remote_settings_manager));
}

CastWebView::Scoped WebRuntimeApplication::CreateWebView(
    CoreApplicationServiceGrpc* grpc_stub) {
  // Register GrpcWebUI for handling Cast apps with URLs in the form
  // chrome*://* that use WebUIs.
  const std::vector<std::string> hosts = {"home", "error", "cast_resources"};
  content::WebUIControllerFactory::RegisterFactory(
      new GrpcWebUiControllerFactory(std::move(hosts), *grpc_stub));

  return RuntimeApplicationBase::CreateWebView(grpc_stub);
}

GURL WebRuntimeApplication::ProcessWebView(
    CoreApplicationServiceGrpc* grpc_stub,
    CastWebContents* cast_web_contents) {
  cast::bindings::GetAllResponse bindings_response;
  {
    grpc::ClientContext context;
    cast::bindings::GetAllRequest bindings_request;
    grpc::Status bindings_status =
        grpc_stub->GetAll(&context, bindings_request, &bindings_response);
  }

  // Call to CastWebContents::Observer::Observe().
  Observe(cast_web_contents);

  bindings_manager_ =
      std::make_unique<BindingsManagerWebRuntime>(grpc_cq_, grpc_stub);
  for (int i = 0; i < bindings_response.bindings_size(); ++i) {
    bindings_manager_->AddBinding(
        bindings_response.bindings(i).before_load_script());
  }
  cast_web_contents->ConnectToBindingsService(
      bindings_manager_->CreateRemote());

  SetApplicationStarted();

  return GURL(app_url_);
}

}  // namespace chromecast
