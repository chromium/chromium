// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"

#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/common/feature_constants.h"
#include "components/cast_receiver/browser/public/message_port_service.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace chromecast {

WebRuntimeApplication::WebRuntimeApplication(
    std::string cast_session_id,
    cast_receiver::ApplicationConfig config,
    cast_receiver::ApplicationClient& application_client)
    : RuntimeApplicationBase(std::move(cast_session_id),
                             std::move(config),
                             application_client) {
  DCHECK(app_url().is_valid());
}

WebRuntimeApplication::~WebRuntimeApplication() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(
      RuntimeApplicationBase::Delegate::ApplicationStopReason::kUserRequest,
      net::OK);
}

void WebRuntimeApplication::Launch(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LOG(INFO) << "Launching application: " << *this;

  SetContentPermissions(*delegate().GetWebContents());

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

  DLOG(INFO) << "Inner web contents created";

  CastWebContents* outer_cast_contents =
      CastWebContents::FromWebContents(delegate().GetWebContents());
  DCHECK(outer_cast_contents);

  SetContentPermissions(*inner_web_contents);

  // TODO(crbug.com/1359571): Decouple URL Rewrite support from CastWebContents.
  outer_cast_contents->url_rewrite_rules_manager()->AddWebContents(
      inner_web_contents);
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
    StopApplication(
        RuntimeApplicationBase::Delegate::ApplicationStopReason::kRuntimeError,
        net::ERR_FAILED);
    return;
  }

  content::WebContentsObserver::Observe(delegate().GetWebContents());
  cast_receiver::PageStateObserver::Observe(delegate().GetWebContents());
  auto* message_port_sevice = delegate().GetMessagePortService();
  DCHECK(message_port_sevice);
  bindings_manager_ = std::make_unique<cast_receiver::BindingsManager>(
      *this, *message_port_sevice);
  for (auto& binding : bindings) {
    bindings_manager_->AddBinding(std::move(binding));
  }
  bindings_manager_->ConfigureWebContents(delegate().GetWebContents());

  // Application is initialized now - we can load the URL.
  LoadPage(app_url());
}

void WebRuntimeApplication::OnPageLoadComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnPageLoaded();
}

void WebRuntimeApplication::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(
      RuntimeApplicationBase::Delegate::ApplicationStopReason::kRuntimeError,
      0);
}

void WebRuntimeApplication::OnPageStopped(StopReason reason,
                                          int32_t error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (reason) {
    case cast_receiver::PageStateObserver::StopReason::kUnknown:
      StopApplication(RuntimeApplicationBase::Delegate::ApplicationStopReason::
                          kRuntimeError,
                      error_code);
      break;
    case cast_receiver::PageStateObserver::StopReason::kApplicationRequest:
      StopApplication(RuntimeApplicationBase::Delegate::ApplicationStopReason::
                          kApplicationRequest,
                      error_code);
      break;
    case cast_receiver::PageStateObserver::StopReason::kHttpError:
      StopApplication(
          RuntimeApplicationBase::Delegate::ApplicationStopReason::kHttpError,
          error_code);
      break;
  }
}

}  // namespace chromecast
