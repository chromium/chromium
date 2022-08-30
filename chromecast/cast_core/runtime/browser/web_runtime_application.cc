// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/web_runtime_application.h"

#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/runtime/browser/bindings_manager_web_runtime.h"
#include "chromecast/cast_core/runtime/browser/grpc_webui_controller_factory.h"
#include "chromecast/common/feature_constants.h"
#include "content/public/browser/navigation_entry.h"
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
  StopApplication(cast::common::StopReason::USER_REQUEST, net::OK);
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

void WebRuntimeApplication::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(inner_web_contents);

  CastWebContents* inner_cast_contents =
      CastWebContents::FromWebContents(inner_web_contents);
  CastWebContents* outer_cast_contents = GetCastWebContents();
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
      GetAppConfig().app_id(), GetCastSessionId(), GetIsAudioOnly(), app_url_,
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
  NotifyMediaPlaybackChanged(true);
}

void WebRuntimeApplication::MediaStoppedPlaying(
    const MediaPlayerInfo& video_type,
    const content::MediaPlayerId& id,
    content::WebContentsObserver::MediaStoppedReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NotifyMediaPlaybackChanged(false);
}

void WebRuntimeApplication::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // This logic is a subset of that for DidFinishLoad() in CastWebContentsImpl.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int http_status_code = 0;
  content::NavigationEntry* nav_entry =
      web_contents()->GetController().GetVisibleEntry();
  if (nav_entry) {
    http_status_code = nav_entry->GetHttpStatusCode();
  }

  if (http_status_code != 0 && http_status_code / 100 != 2) {
    DLOG(INFO) << "Stopping after receiving http failure status code: "
               << http_status_code;
    StopApplication(cast::common::StopReason::HTTP_ERROR,
                    net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    return;
  }

  OnPageLoaded();
}

void WebRuntimeApplication::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  // This logic is a subset of that for DidFailLoad() in CastWebContentsImpl.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (render_frame_host->GetParent()) {
    DLOG(ERROR) << "Got error on sub-iframe: url=" << validated_url.spec()
                << ", error=" << error_code;
    return;
  }
  if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED means download was aborted by the app, typically this happens
    // when flinging URL for direct playback, the initial URLRequest gets
    // cancelled/aborted and then the same URL is requested via the buffered
    // data source for media::Pipeline playback.
    DLOG(INFO) << "Load canceled: url=" << validated_url.spec();

    // We consider the page to be fully loaded in this case, since the app has
    // intentionally entered this state. If the app wanted to stop, it would
    // have called window.close() instead.
    OnPageLoaded();
    return;
  }

  StopApplication(cast::common::StopReason::HTTP_ERROR, error_code);
}

void WebRuntimeApplication::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::WebContentsObserver::Observe(nullptr);
  StopApplication(cast::common::StopReason::APPLICATION_REQUEST, net::OK);
}

void WebRuntimeApplication::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(cast::common::StopReason::HTTP_ERROR, net::ERR_UNEXPECTED);
}

void WebRuntimeApplication::OnAllBindingsReceived(
    cast::utils::GrpcStatusOr<cast::bindings::GetAllResponse> response_or) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!response_or.ok()) {
    LOG(ERROR) << "Failed to get all bindings: " << response_or.ToString();
    StopApplication(cast::common::StopReason::RUNTIME_ERROR, net::ERR_FAILED);
    return;
  }

  content::WebContentsObserver::Observe(GetCastWebContents()->web_contents());
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
