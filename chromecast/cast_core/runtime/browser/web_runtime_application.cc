// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"

#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_factory.h"
#include "chromecast/common/feature_constants.h"
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

void WebRuntimeApplication::HandleMessage(
    const cast::web::Message& message,
    cast::web::MessagePortStatus* response) {
  bindings_manager_->HandleMessage(message, response);
}

void WebRuntimeApplication::InitializeApplication(
    CoreApplicationServiceGrpc* grpc_stub,
    CastWebContents* cast_web_contents) {
  DCHECK(app_url().is_empty());
  set_app_url(GURL(app_config().cast_web_app_config().url()));

  // Register GrpcWebUI for handling Cast apps with URLs in the form
  // chrome*://* that use WebUIs.
  const std::vector<std::string> hosts = {"home", "error", "cast_resources"};
  content::WebUIControllerFactory::RegisterFactory(
      new GrpcWebUiControllerFactory(std::move(hosts), *grpc_stub));

  cast::bindings::GetAllResponse bindings_response;
  {
    grpc::ClientContext context;
    cast::bindings::GetAllRequest bindings_request;
    grpc::Status bindings_status =
        grpc_stub->GetAll(&context, bindings_request, &bindings_response);
  }

  CastWebContents::Observer::Observe(cast_web_contents);

  bindings_manager_ =
      std::make_unique<BindingsManagerWebRuntime>(grpc_cq_, grpc_stub);
  for (int i = 0; i < bindings_response.bindings_size(); ++i) {
    bindings_manager_->AddBinding(
        bindings_response.bindings(i).before_load_script());
  }
  cast_web_contents->ConnectToBindingsService(
      bindings_manager_->CreateRemote());

  SetApplicationStarted();
}

void WebRuntimeApplication::InnerContentsCreated(
    CastWebContents* inner_contents,
    CastWebContents* outer_contents) {
  DCHECK(inner_contents);

  LOG(INFO) << "Inner web contents created";

#if DCHECK_IS_ON()
  base::Value features(base::Value::Type::DICTIONARY);
  base::Value dev_mode_config(base::Value::Type::DICTIONARY);
  dev_mode_config.SetKey(feature::kDevModeOrigin,
                         base::Value(app_url().spec()));
  features.SetKey(feature::kEnableDevMode, std::move(dev_mode_config));
  inner_contents->AddRendererFeatures(std::move(features));
#endif

  const std::vector<int32_t> feature_permissions;
  const std::vector<std::string> additional_feature_permission_origins;

  // Bind inner CastWebContents with the same session id and app id as the
  // root CastWebContents so that the same url rewrites are applied.
  inner_contents->SetAppProperties(
      app_config().app_id(), cast_session_id(), false /*is_audio_app*/,
      app_url(), false /*enforce_feature_permissions*/, feature_permissions,
      additional_feature_permission_origins);

  CastWebContents::Observer::Observe(inner_contents);

  // Attach URL request rewrire rules to the inner CastWebContents.
  GetUrlRewriteRulesManager()->AddWebContents(inner_contents->web_contents());
}

}  // namespace chromecast
