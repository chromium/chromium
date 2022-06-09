// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"

#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_factory.h"
#include "chromecast/common/feature_constants.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromecast {

WebRuntimeApplication::WebRuntimeApplication(
    std::string cast_session_id,
    cast::common::ApplicationConfig config,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RuntimeApplicationBase(std::move(cast_session_id),
                             std::move(config),
                             mojom::RendererType::MOJO_RENDERER,
                             web_service,
                             std::move(task_runner)),
      app_url_(GetAppConfig().cast_web_app_config().url()) {}

WebRuntimeApplication::~WebRuntimeApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(cast::v2::ApplicationStatusRequest::USER_REQUEST);
}

cast::utils::GrpcStatusOr<cast::web::MessagePortStatus>
WebRuntimeApplication::HandlePortMessage(cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bindings_manager_->HandleMessage(std::move(message));
}

void WebRuntimeApplication::LaunchApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Register GrpcWebUI for handling Cast apps with URLs in the form
  // chrome*://* that use WebUIs.
  const std::vector<std::string> hosts = {"home", "error", "cast_resources"};
  content::WebUIControllerFactory::RegisterFactory(
      new GrpcWebUiControllerFactory(std::move(hosts), core_app_stub()));

  auto call =
      core_message_port_app_stub()
          ->CreateCall<
              cast::v2::CoreMessagePortApplicationServiceStub::GetAll>();
  std::move(call).InvokeAsync(base::BindPostTask(
      task_runner(),
      base::BindOnce(&WebRuntimeApplication::OnAllBindingsReceived,
                     weak_factory_.GetWeakPtr())));
}

bool WebRuntimeApplication::IsStreamingApplication() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

void WebRuntimeApplication::InnerContentsCreated(
    CastWebContents* inner_contents,
    CastWebContents* outer_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(inner_contents);
  DCHECK_EQ(outer_contents, GetCastWebContents());

  LOG(INFO) << "Inner web contents created";

#if DCHECK_IS_ON()
  base::Value features(base::Value::Type::DICTIONARY);
  base::Value dev_mode_config(base::Value::Type::DICTIONARY);
  dev_mode_config.SetKey(feature::kDevModeOrigin, base::Value(app_url_.spec()));
  features.SetKey(feature::kEnableDevMode, std::move(dev_mode_config));
  inner_contents->AddRendererFeatures(std::move(features));
#endif

  // Bind inner CastWebContents with the same session id and app id as the
  // root CastWebContents so that the same url rewrites are applied.
  inner_contents->SetAppProperties(
      GetAppConfig().app_id(), GetCastSessionId(), GetIsAudioOnly(), app_url_,
      GetEnforceFeaturePermissions(), GetFeaturePermissions(),
      GetAdditionalFeaturePermissionOrigins());
  CastWebContents::Observer::Observe(inner_contents);

  // Attach URL request rewrire rules to the inner CastWebContents.
  outer_contents->url_rewrite_rules_manager()->AddWebContents(
      inner_contents->web_contents());
}

void WebRuntimeApplication::PageStateChanged(PageState page_state) {
  DLOG(INFO) << "Page state changed: page_state=" << page_state << ", "
             << *this;
  switch (page_state) {
    case PageState::IDLE:
    case PageState::LOADING:
    case PageState::CLOSED:
    case PageState::DESTROYED:
      return;

    case PageState::LOADED:
      OnApplicationLaunched();
      return;

    case PageState::ERROR:
      StopApplication(cast::v2::ApplicationStatusRequest::HTTP_ERROR);
      return;
  }
}

void WebRuntimeApplication::PageStopped(PageState page_state,
                                        int32_t error_code) {
  LOG(INFO) << "Page stopped: page_state=" << page_state
            << ", error_code=" << error_code << ", " << *this;
  StopApplication(cast::v2::ApplicationStatusRequest::APPLICATION_REQUEST);
}

void WebRuntimeApplication::OnAllBindingsReceived(
    cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_or.ok()) {
    LOG(ERROR) << "Failed to get all bindings: " << response_or.ToString();
    StopApplication(cast::v2::ApplicationStatusRequest::RUNTIME_ERROR);
    return;
  }

  CastWebContents::Observer::Observe(GetCastWebContents());
  bindings_manager_ =
      std::make_unique<BindingsManagerWebRuntime>(core_message_port_app_stub());
  for (int i = 0; i < response_or->bindings_size(); ++i) {
    bindings_manager_->AddBinding(
        response_or->bindings(i).before_load_script());
  }
  GetCastWebContents()->ConnectToBindingsService(
      bindings_manager_->CreateRemote());

  // Application is initialized now - we can load the URL.
  LoadPage(app_url_);
}

}  // namespace chromecast
