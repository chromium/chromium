// Copyright 2021 The Chromium Authors
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
#include "chromecast/cast_core/runtime/browser/runtime_application_platform.h"
#include "chromecast/cast_core/runtime/browser/url_rewrite/url_request_rewrite_type_converters.h"
#include "chromecast/common/feature_constants.h"

namespace chromecast {
namespace {

// Parses renderer features.
const cast::common::Dictionary::Entry* FindEntry(
    const std::string& key,
    const cast::common::Dictionary& dict) {
  auto iter = base::ranges::find(dict.entries(), key,
                                 &cast::common::Dictionary::Entry::key);
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
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    RuntimeApplicationPlatform::Factory runtime_application_factory)
    : platform_(std::move(runtime_application_factory)
                    .Run(task_runner, cast_session_id, *this)),
      cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
      renderer_type_(renderer_type_used),
      web_service_(web_service),
      task_runner_(std::move(task_runner)) {
  DCHECK(platform_);
  DCHECK(web_service_);
  DCHECK(task_runner_);
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_application_running_);
}

const std::string& RuntimeApplicationBase::GetDisplayName() const {
  return config().display_name();
}

const std::string& RuntimeApplicationBase::GetAppId() const {
  return config().app_id();
}

const std::string& RuntimeApplicationBase::GetCastSessionId() const {
  return cast_session_id_;
}

void RuntimeApplicationBase::Load(cast::runtime::LoadApplicationRequest request,
                                  RuntimeApplication::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  platform_->Load(
      std::move(request),
      base::BindOnce(&RuntimeApplicationBase::OnApplicationLoading,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RuntimeApplicationBase::OnApplicationLoading(
    RuntimeApplication::StatusCallback callback,
    cast_receiver::Status success) {
  if (!success) {
    // TODO(crbug.com/1360597): Add details of this failure to the new Status
    // object returned.
    std::move(callback).Run(false);
    return;
  }

  is_application_running_ = true;
  cast_web_view_ = CreateCastWebView();

  LOG(INFO) << "Loaded application" << *this;
  std::move(callback).Run(true);
}

void RuntimeApplicationBase::OnApplicationLaunching(
    RuntimeApplication::StatusCallback callback,
    cast_receiver::Status success) {
  std::move(callback).Run(success);
  if (success) {
    LOG(INFO) << "Launched application" << *this;
    OnApplicationLaunched();
  }
}

void RuntimeApplicationBase::OnUrlRewriteRulesSet(
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) {
  cast_web_contents()->SetUrlRewriteRules(std::move(mojom_rules));
}

void RuntimeApplicationBase::Launch(
    cast::runtime::LaunchApplicationRequest request,
    RuntimeApplication::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  platform_->Launch(
      std::move(request),
      base::BindOnce(&RuntimeApplicationBase::OnApplicationLaunching,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

base::Value RuntimeApplicationBase::GetRendererFeatures() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry =
      FindEntry(feature::kCastCoreRendererFeatures, config().extra_features());
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
      FindEntry(feature::kCastCoreIsAudioOnly, config().extra_features());
  if (!entry) {
    return false;
  }

  DCHECK(entry->value().value_case() == cast::common::Value::kFlag);
  return entry->value().flag();
}

bool RuntimeApplicationBase::GetIsRemoteControlMode() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry = FindEntry(feature::kCastCoreIsRemoteControlMode,
                                config().extra_features());
  if (!entry) {
    return false;
  }

  DCHECK(entry->value().value_case() == cast::common::Value::kFlag);
  return entry->value().flag();
}

bool RuntimeApplicationBase::GetEnforceFeaturePermissions() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto* entry = FindEntry(feature::kCastCoreEnforceFeaturePermissions,
                                config().extra_features());
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
                                config().extra_features());
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
                                config().extra_features());
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
  const auto* entry =
      FindEntry(feature::kCastCoreRendererFeatures, config().extra_features());
  if (!entry) {
    return false;
  }
  DCHECK(entry->value().has_dictionary());

  return FindEntry(chromecast::feature::kEnableDevMode,
                   entry->value().dictionary()) != nullptr;
}

void RuntimeApplicationBase::LoadPage(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cast_web_contents()->AddRendererFeatures(GetRendererFeatures());
  cast_web_contents()->SetAppProperties(
      config().app_id(), GetCastSessionId(), GetIsAudioOnly(), url,
      GetEnforceFeaturePermissions(), GetFeaturePermissions(),
      GetAdditionalFeaturePermissionOrigins());

  // Start loading the URL while JS visibility is disabled and no window is
  // created. This way users won't see the progressive UI updates as the page is
  // formed and styles are applied. The actual window will be created in
  // OnApplicationStarted when application is fully launched.
  cast_web_contents()->LoadUrl(url);

  // This needs to be called to get the PageState::LOADED event as it's fully
  // loaded.
  cast_web_contents()->SetWebVisibilityAndPaint(false);
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

  platform_->NotifyApplicationStarted();
}

CastWebView::Scoped RuntimeApplicationBase::CreateCastWebView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojom::CastWebViewParamsPtr params = mojom::CastWebViewParams::New();
  params->renderer_type = renderer_type_;
  params->handle_inner_contents = true;
  params->session_id = GetCastSessionId();
  params->is_remote_control_mode = GetIsRemoteControlMode();
  params->activity_id =
      params->is_remote_control_mode ? params->session_id : config().app_id();
  params->enabled_for_dev = GetEnabledForDev();
  return web_service_->CreateWebViewInternal(std::move(params));
}

void RuntimeApplicationBase::OnMediaStateSet(
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

void RuntimeApplicationBase::OnVisibilitySet(
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

void RuntimeApplicationBase::OnTouchInputSet(
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

bool RuntimeApplicationBase::IsApplicationRunning() {
  return is_application_running_;
}

void RuntimeApplicationBase::StopApplication(
    cast::common::StopReason::Type stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_application_running_) {
    return;
  }
  is_application_running_ = false;

  if (cast_web_view_) {
    cast_web_contents()->ClosePage();
    // Check if window is still available as page might have been closed before.
    if (cast_web_view_->window()) {
      cast_web_view_->window()->RemoveObserver(this);
    }
  }

  platform_->NotifyApplicationStopped(stop_reason, net_error_code);

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
      cast_web_contents()->SetWebVisibilityAndPaint(true);
      break;

    default:
      LOG(INFO) << "Application is hidden now: " << *this;
      cast_web_contents()->SetWebVisibilityAndPaint(false);
      break;
  }
}

}  // namespace chromecast
