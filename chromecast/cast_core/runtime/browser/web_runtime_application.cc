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
    cast::common::ApplicationConfig app_config,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : RuntimeApplicationBase(std::move(app_config),
                             mojom::RendererType::MOJO_RENDERER,
                             web_service,
                             std::move(task_runner)) {}

WebRuntimeApplication::~WebRuntimeApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication();
}

cast::utils::GrpcStatusOr<cast::web::MessagePortStatus>
WebRuntimeApplication::HandlePortMessage(cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return bindings_manager_->HandleMessage(std::move(message));
}

void WebRuntimeApplication::InitializeApplication(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(app_url().is_empty());
  set_app_url(GURL(app_config().cast_web_app_config().url()));

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
                     weak_factory_.GetWeakPtr(), std::move(callback))));
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
  outer_contents->url_rewrite_rules_manager()->AddWebContents(
      inner_contents->web_contents());
}

void WebRuntimeApplication::OnAllBindingsReceived(
    StatusCallback callback,
    cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_or.ok()) {
    LOG(ERROR) << "Failed to get all bindings: " << response_or.ToString();
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INTERNAL, "Failed to get all bindings"));
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

  SetApplicationState(
      cast::v2::ApplicationStatusRequest::STARTED,
      base::BindPostTask(
          task_runner(),
          base::BindOnce(&WebRuntimeApplication::OnApplicationStateChanged,
                         weak_factory_.GetWeakPtr(), std::move(callback))));
}

void WebRuntimeApplication::OnApplicationStateChanged(StatusCallback callback,
                                                      grpc::Status status) {
  if (!status.ok()) {
    LOG(ERROR) << "Failed to set application state to started: " << *this
               << ", status=" << cast::utils::GrpcStatusToString(status);
    std::move(callback).Run(status);
    StopApplication();
    return;
  }

  std::move(callback).Run(grpc::Status::OK);
}

}  // namespace chromecast
