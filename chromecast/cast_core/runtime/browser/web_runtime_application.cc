// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"

#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/runtime/browser/message_port_service.h"
#include "chromecast/common/feature_constants.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromecast {

WebRuntimeApplication::WebRuntimeApplication(
    std::string cast_session_id,
    cast::common::ApplicationConfig config,
    cast_receiver::ApplicationClient& application_client)
    : RuntimeApplicationBase(std::move(cast_session_id),
                             std::move(config),
                             mojom::RendererType::MOJO_RENDERER,
                             application_client),
      app_url_(RuntimeApplicationBase::config().cast_web_app_config().url()) {}

WebRuntimeApplication::~WebRuntimeApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(cast::common::StopReason::USER_REQUEST, net::OK);
}

bool WebRuntimeApplication::OnMessagePortMessage(cast::web::Message message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!bindings_manager_) {
    return false;
  }
  return bindings_manager_->HandleMessage(std::move(message)).ok();
}

void WebRuntimeApplication::Launch(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(INFO) << "Launching application: " << *this;

  // Register GrpcWebUI for handling Cast apps with URLs in the form
  // chrome*://* that use WebUIs.
  const std::vector<std::string> hosts = {"home", "error", "cast_resources"};
  content::WebUIControllerFactory::RegisterFactory(
      delegate().CreateWebUIControllerFactory(std::move(hosts)).release());

  delegate().GetAllBindings(base::BindPostTask(
      task_runner(),
      base::BindOnce(&WebRuntimeApplication::OnAllBindingsReceived,
                     weak_factory_.GetWeakPtr())));

  // Signal that application is launching.
  std::move(callback).Run(cast_receiver::OkStatus());
}

bool WebRuntimeApplication::IsStreamingApplication() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

void WebRuntimeApplication::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(inner_web_contents);

  CastWebContents* inner_cast_contents =
      CastWebContents::FromWebContents(inner_web_contents);
  CastWebContents* outer_cast_contents =
      CastWebContents::FromWebContents(delegate().GetWebContents());
  DCHECK(inner_cast_contents);
  DCHECK(outer_cast_contents);

  DLOG(INFO) << "Inner web contents created";

#if DCHECK_IS_ON()
  base::Value features(base::Value::Type::DICTIONARY);
  base::Value dev_mode_config(base::Value::Type::DICTIONARY);
  dev_mode_config.SetKey(feature::kDevModeOrigin, base::Value(app_url_.spec()));
  features.SetKey(feature::kEnableDevMode, std::move(dev_mode_config));
  inner_cast_contents->AddRendererFeatures(std::move(features));
#endif

  // Bind inner CastWebContents with the same session id and app id as the
  // root CastWebContents so that the same url rewrites are applied.
  inner_cast_contents->SetAppProperties(
      config().app_id(), GetCastSessionId(), GetIsAudioOnly(), app_url_,
      GetEnforceFeaturePermissions(), GetFeaturePermissions(),
      GetAdditionalFeaturePermissionOrigins());
  content::WebContentsObserver::Observe(inner_web_contents);

  // Attach URL request rewrire rules to the inner CastWebContents.
  outer_cast_contents->url_rewrite_rules_manager()->AddWebContents(
      inner_cast_contents->web_contents());
}

void WebRuntimeApplication::MediaStartedPlaying(
    const MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate().NotifyMediaPlaybackChanged(true);
}

void WebRuntimeApplication::MediaStoppedPlaying(
    const MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate().NotifyMediaPlaybackChanged(false);
}

void WebRuntimeApplication::OnAllBindingsReceived(
    cast_receiver::Status status,
    std::vector<std::string> bindings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!status.ok()) {
    LOG(ERROR) << "Failed to get all bindings: " << status;
    StopApplication(cast::common::StopReason::RUNTIME_ERROR, net::ERR_FAILED);
    return;
  }

  content::WebContentsObserver::Observe(delegate().GetWebContents());
  cast_receiver::PageStateObserver::Observe(delegate().GetWebContents());
  bindings_manager_ = std::make_unique<BindingsManagerWebRuntime>(
      *this, delegate().CreateMessagePortService());
  for (auto& binding : bindings) {
    bindings_manager_->AddBinding(std::move(binding));
  }
  bindings_manager_->ConfigureWebContents(delegate().GetWebContents());

  // Application is initialized now - we can load the URL.
  LoadPage(app_url_);
}

void WebRuntimeApplication::OnPageLoadComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPageLoaded();
}

void WebRuntimeApplication::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(cast::common::StopReason::RUNTIME_ERROR, 0);
}

void WebRuntimeApplication::OnPageStopped(StopReason reason,
                                          int32_t error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (reason) {
    case cast_receiver::PageStateObserver::StopReason::kUnknown:
      StopApplication(cast::common::StopReason::RUNTIME_ERROR, error_code);
      break;
    case cast_receiver::PageStateObserver::StopReason::kApplicationRequest:
      StopApplication(cast::common::StopReason::APPLICATION_REQUEST,
                      error_code);
      break;
    case cast_receiver::PageStateObserver::StopReason::kHttpError:
      StopApplication(cast::common::StopReason::HTTP_ERROR, error_code);
      break;
  }
}

}  // namespace chromecast
