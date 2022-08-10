// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "chromecast/browser/visibility_types.h"
#include "chromecast/cast_core/grpc/grpc_status_or.h"
#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"
#include "chromecast/common/feature_constants.h"

namespace chromecast {

namespace {

// Parses renderer features.
const cast::common::Dictionary::Entry* FindEntry(
    const std::string& key,
    const cast::common::Dictionary& dict) {
  auto iter = base::ranges::find_if(
      dict.entries(), [&key](const auto& entry) { return entry.key() == key; });
  if (iter == dict.entries().end()) {
    return nullptr;
  }
  return &*iter;
}

}  // namespace

RuntimeApplicationBase::RuntimeApplicationBase(
    std::string cast_session_id,
    cast::common::ApplicationConfig app_config,
    mojom::RendererType renderer_type_used,
    CastWebService* web_service,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
      web_service_(web_service),
      task_runner_(std::move(task_runner)),
      renderer_type_(renderer_type_used) {
  DCHECK(web_service_);
  DCHECK(task_runner_);
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_application_running_);
}

CastWebContents* RuntimeApplicationBase::GetCastWebContents() {
  DCHECK(cast_web_view_);
  return cast_web_view_->cast_web_contents();
}

const std::string& RuntimeApplicationBase::GetCastMediaServiceEndpoint() const {
  DCHECK(cast_media_service_grpc_endpoint_);
  return *cast_media_service_grpc_endpoint_;
}

const cast::common::ApplicationConfig& RuntimeApplicationBase::GetAppConfig()
    const {
  return app_config_;
}

const std::string& RuntimeApplicationBase::GetCastSessionId() const {
  return cast_session_id_;
}

void RuntimeApplicationBase::Load(cast::runtime::LoadApplicationRequest request,
                                  StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!grpc_server_);

  if (request.runtime_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Application service endpoint is missing"));
    return;
  }

  LOG(INFO) << "Loading application: " << *this;
  is_application_running_ = true;

  // Start the gRPC server.
  grpc_server_.emplace();
  grpc_server_->SetHandler<
      cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationBase::HandleSetUrlRewriteRules,
                              weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetMediaState>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeApplicationBase::HandleSetMediaState,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetVisibility>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeApplicationBase::HandleSetVisibility,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_
      ->SetHandler<cast::v2::RuntimeApplicationServiceHandler::SetTouchInput>(
          base::BindPostTask(
              task_runner_,
              base::BindRepeating(&RuntimeApplicationBase::HandleSetTouchInput,
                                  weak_factory_.GetWeakPtr())));
  grpc_server_->SetHandler<
      cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage>(
      base::BindPostTask(
          task_runner_,
          base::BindRepeating(&RuntimeApplicationBase::HandlePostMessage,
                              weak_factory_.GetWeakPtr())));
  grpc_server_->Start(
      request.runtime_application_service_info().grpc_endpoint());
  LOG(INFO) << "Runtime application server started: " << *this << ", endpoint="
            << request.runtime_application_service_info().grpc_endpoint();

  // Initialize web view.
  cast_web_view_ = CreateCastWebView();

  // Set initial URL rewrite rules.
  url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
      mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
          request.url_rewrite_rules());
  GetCastWebContents()->SetUrlRewriteRules(std::move(mojom_rules));

  LOG(INFO) << "Successfully loaded: " << *this;
  std::move(callback).Run(grpc::Status::OK);
}

void RuntimeApplicationBase::Launch(
    cast::runtime::LaunchApplicationRequest request,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!cast_web_view_ || !cast_web_view_->window()) {
    std::move(callback).Run(grpc::Status(grpc::StatusCode::INTERNAL,
                                         "Cast web view is not initialized"));
    return;
  }
  if (request.core_application_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Core application service endpoint is missing"));
    return;
  }
  if (request.cast_media_service_info().grpc_endpoint().empty()) {
    std::move(callback).Run(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                         "Media service endpoint is missing"));
    return;
  }

  LOG(INFO) << "Launching application: " << *this;

  // Create stubs for Core*ApplicationServices.
  auto core_channel = grpc::CreateChannel(
      request.core_application_service_info().grpc_endpoint(),
      grpc::InsecureChannelCredentials());
  core_app_stub_.emplace(core_channel);
  core_message_port_app_stub_.emplace(core_channel);

  // Initialize MZ data.
  cast_media_service_grpc_endpoint_.emplace(
      request.cast_media_service_info().grpc_endpoint());

  SetMediaState(request.media_state());
  SetVisibility(request.visibility());
  SetTouchInput(request.touch_input());

  // Report that Cast application launch is initiated.
  std::move(callback).Run(grpc::Status::OK);

  LaunchApplication();
}

base::Value RuntimeApplicationBase::GetRendererFeatures() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry = FindEntry(feature::kCastCoreRendererFeatures,
                                GetAppConfig().extra_features());
  if (!entry) {
    return base::Value();
  }
  DCHECK(entry->value().has_dictionary());

  base::Value::Dict renderer_features;
  for (const cast::common::Dictionary::Entry& feature :
       entry->value().dictionary().entries()) {
    base::Value::Dict dict;
    if (feature.has_value()) {
      DCHECK(feature.value().has_dictionary());
      for (const cast::common::Dictionary::Entry& feature_arg :
           feature.value().dictionary().entries()) {
        DCHECK(feature_arg.has_value());
        if (feature_arg.value().value_case() == cast::common::Value::kFlag) {
          dict.Set(feature_arg.key(), feature_arg.value().flag());
        } else if (feature_arg.value().value_case() ==
                   cast::common::Value::kText) {
          dict.Set(feature_arg.key(), feature_arg.value().text());
        } else {
          LOG(FATAL) << "No or unsupported value was set for the feature: "
                     << feature.key();
        }
      }
    }
    DVLOG(1) << "Renderer feature created: " << feature.key();
    renderer_features.Set(feature.key(), std::move(dict));
  }

  return base::Value(std::move(renderer_features));
}

bool RuntimeApplicationBase::GetIsAudioOnly() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry =
      FindEntry(feature::kCastCoreIsAudioOnly, GetAppConfig().extra_features());
  if (!entry) {
    return false;
  }

  DCHECK(entry->value().value_case() == cast::common::Value::kFlag);
  return entry->value().flag();
}

bool RuntimeApplicationBase::GetIsRemoteControlMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry = FindEntry(feature::kCastCoreIsRemoteControlMode,
                                GetAppConfig().extra_features());
  if (!entry) {
    return false;
  }

  DCHECK(entry->value().value_case() == cast::common::Value::kFlag);
  return entry->value().flag();
}

bool RuntimeApplicationBase::GetEnforceFeaturePermissions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry = FindEntry(feature::kCastCoreEnforceFeaturePermissions,
                                GetAppConfig().extra_features());
  if (!entry) {
    return false;
  }

  DCHECK(entry->value().value_case() == cast::common::Value::kFlag);
  return entry->value().flag();
}

std::vector<int> RuntimeApplicationBase::GetFeaturePermissions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<int> feature_permissions;
  const auto* entry = FindEntry(feature::kCastCoreFeaturePermissions,
                                GetAppConfig().extra_features());
  if (!entry) {
    return feature_permissions;
  }

  DCHECK(entry->value().value_case() == cast::common::Value::kArray);
  base::ranges::for_each(
      entry->value().array().values(),
      [&feature_permissions](const cast::common::Value& value) {
        DCHECK(value.value_case() == cast::common::Value::kNumber);
        feature_permissions.push_back(value.number());
      });
  return feature_permissions;
}

std::vector<std::string>
RuntimeApplicationBase::GetAdditionalFeaturePermissionOrigins() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> feature_permission_origins;
  const auto* entry = FindEntry(feature::kCastCoreFeaturePermissionOrigins,
                                GetAppConfig().extra_features());
  if (!entry) {
    return feature_permission_origins;
  }

  DCHECK(entry->value().value_case() == cast::common::Value::kArray);
  base::ranges::for_each(
      entry->value().array().values(),
      [&feature_permission_origins](const cast::common::Value& value) {
        DCHECK(value.value_case() == cast::common::Value::kText);
        feature_permission_origins.push_back(value.text());
      });
  return feature_permission_origins;
}

bool RuntimeApplicationBase::GetEnabledForDev() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry = FindEntry(feature::kCastCoreRendererFeatures,
                                GetAppConfig().extra_features());
  if (!entry) {
    return false;
  }
  DCHECK(entry->value().has_dictionary());

  return FindEntry(chromecast::feature::kEnableDevMode,
                   entry->value().dictionary()) != nullptr;
}

void RuntimeApplicationBase::LoadPage(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetCastWebContents()->AddRendererFeatures(GetRendererFeatures());
  GetCastWebContents()->SetAppProperties(
      GetAppConfig().app_id(), GetCastSessionId(), GetIsAudioOnly(), url,
      GetEnforceFeaturePermissions(), GetFeaturePermissions(),
      GetAdditionalFeaturePermissionOrigins());

  // Start loading the URL while JS visibility is disabled and no window is
  // created. This way users won't see the progressive UI updates as the page is
  // formed and styles are applied. The actual window will be created in
  // OnApplicationStarted when application is fully launched.
  GetCastWebContents()->LoadUrl(url);

  // This needs to be called to get the PageState::LOADED event as it's fully
  // loaded.
  GetCastWebContents()->SetWebVisibilityAndPaint(false);
}

void RuntimeApplicationBase::OnPageLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Application started: " << *this;

  cast_web_view_->window()->AddObserver(this);
  cast_web_view_->window()->EnableTouchInput(touch_input_ ==
                                             cast::common::TouchInput::ENABLED);

  // Create the window and show the web view.
  if (visibility_ == cast::common::Visibility::FULL_SCREEN) {
    LOG(INFO) << "Loading application in full screen: " << *this;
    cast_web_view_->window()->GrantScreenAccess();
    cast_web_view_->window()->CreateWindow(mojom::ZOrder::APP,
                                           VisibilityPriority::STICKY_ACTIVITY);
  } else {
    LOG(INFO) << "Loading application in background: " << *this;
    cast_web_view_->window()->CreateWindow(mojom::ZOrder::APP,
                                           VisibilityPriority::HIDDEN);
  }

  NotifyApplicationStarted();
}

void RuntimeApplicationBase::HandlePostMessage(
    cast::web::Message request,
    cast::v2::RuntimeMessagePortApplicationServiceHandler::PostMessage::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_application_running_) {
    reactor->Write(grpc::Status(grpc::StatusCode::NOT_FOUND,
                                "No active cast session for PostMessage"));
    return;
  }

  auto response_or = HandlePortMessage(std::move(request));
  if (response_or.ok()) {
    reactor->Write(std::move(response_or).value());
  } else {
    reactor->Write(response_or.status());
  }
}

void RuntimeApplicationBase::HandleSetUrlRewriteRules(
    cast::v2::SetUrlRewriteRulesRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetUrlRewriteRules::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_application_running_) {
    reactor->Write(
        grpc::Status(grpc::StatusCode::NOT_FOUND,
                     "No active cast session for SetUrlRewriteRules"));
    return;
  }
  if (request.has_rules()) {
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules =
        mojo::ConvertTo<url_rewrite::mojom::UrlRequestRewriteRulesPtr>(
            request.rules());
    GetCastWebContents()->SetUrlRewriteRules(std::move(mojom_rules));
  }
  reactor->Write(cast::v2::SetUrlRewriteRulesResponse());
}

void RuntimeApplicationBase::HandleSetMediaState(
    cast::v2::SetMediaStateRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetMediaState::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetMediaState(request.media_state());
  reactor->Write(cast::v2::SetMediaStateResponse());
}

void RuntimeApplicationBase::HandleSetVisibility(
    cast::v2::SetVisibilityRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetVisibility::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetVisibility(request.visibility());
  reactor->Write(cast::v2::SetVisibilityResponse());
}

void RuntimeApplicationBase::HandleSetTouchInput(
    cast::v2::SetTouchInputRequest request,
    cast::v2::RuntimeApplicationServiceHandler::SetTouchInput::Reactor*
        reactor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetTouchInput(request.touch_input());
  reactor->Write(cast::v2::SetTouchInputResponse());
}

CastWebView::Scoped RuntimeApplicationBase::CreateCastWebView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->renderer_type = renderer_type_;
  params->handle_inner_contents = true;
  params->session_id = GetCastSessionId();
  params->is_remote_control_mode = GetIsRemoteControlMode();
  params->activity_id = params->is_remote_control_mode
                            ? params->session_id
                            : GetAppConfig().app_id();
  params->enabled_for_dev = GetEnabledForDev();
  return web_service_->CreateWebViewInternal(std::move(params));
}

void RuntimeApplicationBase::SetMediaState(
    cast::common::MediaState::Type media_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (media_state == cast::common::MediaState::UNDEFINED) {
    return;
  }

  media_state_ = media_state;
  LOG(INFO) << "Media state updated: state="
            << cast::common::MediaState::Type_Name(media_state_) << ", "
            << *this;

  if (!cast_web_view_ || !cast_web_view_->cast_web_contents()) {
    return;
  }

  switch (media_state_) {
    case cast::common::MediaState::LOAD_BLOCKED:
      cast_web_view_->cast_web_contents()->BlockMediaLoading(true);
      cast_web_view_->cast_web_contents()->BlockMediaStarting(true);
      break;

    case cast::common::MediaState::START_BLOCKED:
      cast_web_view_->cast_web_contents()->BlockMediaLoading(false);
      cast_web_view_->cast_web_contents()->BlockMediaStarting(true);
      break;

    case cast::common::MediaState::UNBLOCKED:
      cast_web_view_->cast_web_contents()->BlockMediaLoading(false);
      cast_web_view_->cast_web_contents()->BlockMediaStarting(false);
      break;

    default:
      NOTREACHED();
  }
}

void RuntimeApplicationBase::SetVisibility(
    cast::common::Visibility::Type visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (visibility == cast::common::Visibility::UNDEFINED) {
    // No actual update happened.
    return;
  }

  visibility_ = visibility;
  LOG(INFO) << "Visibility updated: state="
            << cast::common::Visibility::Type_Name(visibility_) << ", "
            << *this;

  if (!cast_web_view_ || !cast_web_view_->window()) {
    return;
  }

  switch (visibility_) {
    case cast::common::Visibility::FULL_SCREEN:
      cast_web_view_->window()->RequestVisibility(
          VisibilityPriority::STICKY_ACTIVITY);
      cast_web_view_->window()->GrantScreenAccess();
      break;

    case cast::common::Visibility::HIDDEN:
      cast_web_view_->window()->RequestVisibility(VisibilityPriority::HIDDEN);
      cast_web_view_->window()->RevokeScreenAccess();
      break;

    default:
      NOTREACHED();
  }
}

void RuntimeApplicationBase::SetTouchInput(
    cast::common::TouchInput::Type touch_input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (touch_input == cast::common::TouchInput::UNDEFINED) {
    // No actual update happened.
    return;
  }

  touch_input_ = touch_input;
  LOG(INFO) << "Touch input updated: state= "
            << cast::common::TouchInput::Type_Name(touch_input_) << ", "
            << *this;

  if (!cast_web_view_ || !cast_web_view_->window()) {
    return;
  }

  cast_web_view_->window()->EnableTouchInput(touch_input_ ==
                                             cast::common::TouchInput::ENABLED);
}

void RuntimeApplicationBase::StopApplication(
    cast::common::StopReason::Type stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(grpc_server_);

  if (!is_application_running_) {
    return;
  }
  is_application_running_ = false;

  if (cast_web_view_) {
    GetCastWebContents()->ClosePage();
    // Check if window is still available as page might have been closed before.
    if (cast_web_view_->window()) {
      cast_web_view_->window()->RemoveObserver(this);
    }
  }

  if (core_app_stub_) {
    NotifyApplicationStopped(stop_reason, net_error_code);
  }

  grpc_server_->Stop();
  grpc_server_.reset();

  LOG(INFO) << "Application is stopped: stop_reason="
            << cast::common::StopReason::Type_Name(stop_reason) << ", "
            << *this;
}

void RuntimeApplicationBase::OnVisibilityChange(
    VisibilityType visibility_type) {
  switch (visibility_type) {
    case VisibilityType::FULL_SCREEN:
    case VisibilityType::PARTIAL_OUT:
    case VisibilityType::TRANSIENTLY_HIDDEN:
      LOG(INFO) << "Application is visible now: " << *this;
      GetCastWebContents()->SetWebVisibilityAndPaint(true);
      break;

    default:
      LOG(INFO) << "Application is hidden now: " << *this;
      GetCastWebContents()->SetWebVisibilityAndPaint(false);
      break;
  }
}

void RuntimeApplicationBase::NotifyApplicationStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::ApplicationStarted>();
  call.request().set_cast_session_id(GetCastSessionId());
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::ApplicationStartedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report that application started: "
            << response_or.ToString();
      }));
}

void RuntimeApplicationBase::NotifyApplicationStopped(
    cast::common::StopReason::Type stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::ApplicationStopped>();
  call.request().set_cast_session_id(GetCastSessionId());
  call.request().set_stop_reason(stop_reason);
  call.request().set_error_code(net_error_code);
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::ApplicationStoppedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report that application stopped: "
            << response_or.ToString();
      }));
}

void RuntimeApplicationBase::NotifyMediaPlaybackChanged(bool playing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(core_app_stub_);

  auto call = core_app_stub_->CreateCall<
      cast::v2::CoreApplicationServiceStub::MediaPlaybackChanged>();
  call.request().set_cast_session_id(GetCastSessionId());
  call.request().set_media_playback_state(
      playing ? cast::common::MediaPlaybackState::PLAYING
              : cast::common::MediaPlaybackState::STOPPED);
  std::move(call).InvokeAsync(base::BindOnce(
      [](cast::utils::GrpcStatusOr<cast::v2::MediaPlaybackChangedResponse>
             response_or) {
        LOG_IF(ERROR, !response_or.ok())
            << "Failed to report media playback changed state: "
            << response_or.ToString();
      }));
}

}  // namespace chromecast
