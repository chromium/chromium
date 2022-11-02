// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/browser/visibility_types.h"
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

RuntimeApplicationBase::Delegate::~Delegate() = default;

RuntimeApplicationBase::RuntimeApplicationBase(
    std::string cast_session_id,
    cast::common::ApplicationConfig app_config,
    mojom::RendererType renderer_type)
    : cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
      renderer_type_(renderer_type),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(task_runner_);
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_application_running_);
}

void RuntimeApplicationBase::SetDelegate(Delegate& delegate) {
  DCHECK(!delegate_);
  delegate_ = &delegate;
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

void RuntimeApplicationBase::Load(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_->GetWebContents());

  is_application_running_ = true;
  if (cached_mojom_rules_) {
    // Apply cached URL rewrite rules before anything is done with the page.
    auto* cast_web_contents =
        CastWebContents::FromWebContents(delegate_->GetWebContents());
    DCHECK(cast_web_contents);
    cast_web_contents->SetUrlRewriteRules(std::move(cached_mojom_rules_));
  }

  LOG(INFO) << "Loaded application: " << *this;
  std::move(callback).Run(true);
}

void RuntimeApplicationBase::Stop(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(cast::common::StopReason::USER_REQUEST, /*net_error_code=*/0);
  std::move(callback).Run(true);
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
  DCHECK(delegate_->GetWebContents());
  auto* cast_web_contents =
      CastWebContents::FromWebContents(delegate_->GetWebContents());
  DCHECK(cast_web_contents);

  cast_web_contents->AddRendererFeatures(GetRendererFeatures());
  cast_web_contents->SetAppProperties(
      config().app_id(), GetCastSessionId(), GetIsAudioOnly(), url,
      GetEnforceFeaturePermissions(), GetFeaturePermissions(),
      GetAdditionalFeaturePermissionOrigins());

  // Start loading the URL while JS visibility is disabled and no window is
  // created. This way users won't see the progressive UI updates as the page is
  // formed and styles are applied. The actual window will be created in
  // OnApplicationStarted when application is fully launched.
  cast_web_contents->LoadUrl(url);

  // This needs to be called to get the PageState::LOADED event as it's fully
  // loaded.
  cast_web_contents->SetWebVisibilityAndPaint(false);
}

void RuntimeApplicationBase::OnPageLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "Page loaded: " << *this;

  DCHECK(delegate_->GetCastContentWindow());
  delegate_->GetCastContentWindow()->AddObserver(this);
  delegate_->GetCastContentWindow()->EnableTouchInput(
      touch_input_ == cast::common::TouchInput::ENABLED);

  // Create the window and show the web view.
  if (visibility_ == cast::common::Visibility::FULL_SCREEN) {
    LOG(INFO) << "Loading page in full screen: " << *this;
    delegate_->GetCastContentWindow()->GrantScreenAccess();
    delegate_->GetCastContentWindow()->CreateWindow(
        mojom::ZOrder::APP, VisibilityPriority::STICKY_ACTIVITY);
  } else {
    LOG(INFO) << "Loading page in background: " << *this;
    delegate_->GetCastContentWindow()->CreateWindow(mojom::ZOrder::APP,
                                                    VisibilityPriority::HIDDEN);
  }

  delegate().NotifyApplicationStarted();
}

void RuntimeApplicationBase::SetUrlRewriteRules(
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) {
  if (!delegate_->GetWebContents()) {
    cached_mojom_rules_ = std::move(mojom_rules);
    return;
  }

  auto* cast_web_contents =
      CastWebContents::FromWebContents(delegate_->GetWebContents());
  DCHECK(cast_web_contents);
  cast_web_contents->SetUrlRewriteRules(std::move(mojom_rules));
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

  if (!delegate_->GetWebContents()) {
    return;
  }

  auto* cast_web_contents =
      CastWebContents::FromWebContents(delegate_->GetWebContents());
  DCHECK(cast_web_contents);
  switch (media_state_) {
    case cast::common::MediaState::LOAD_BLOCKED:
      cast_web_contents->BlockMediaLoading(true);
      cast_web_contents->BlockMediaStarting(true);
      break;

    case cast::common::MediaState::START_BLOCKED:
      cast_web_contents->BlockMediaLoading(false);
      cast_web_contents->BlockMediaStarting(true);
      break;

    case cast::common::MediaState::UNBLOCKED:
      cast_web_contents->BlockMediaLoading(false);
      cast_web_contents->BlockMediaStarting(false);
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

  if (!delegate_->GetCastContentWindow()) {
    return;
  }

  switch (visibility_) {
    case cast::common::Visibility::FULL_SCREEN:
      delegate_->GetCastContentWindow()->RequestVisibility(
          VisibilityPriority::STICKY_ACTIVITY);
      delegate_->GetCastContentWindow()->GrantScreenAccess();
      break;

    case cast::common::Visibility::HIDDEN:
      delegate_->GetCastContentWindow()->RequestVisibility(
          VisibilityPriority::HIDDEN);
      delegate_->GetCastContentWindow()->RevokeScreenAccess();
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

  if (!delegate_->GetCastContentWindow()) {
    return;
  }

  delegate_->GetCastContentWindow()->EnableTouchInput(
      touch_input_ == cast::common::TouchInput::ENABLED);
}

bool RuntimeApplicationBase::IsApplicationRunning() const {
  return is_application_running_;
}

mojom::RendererType RuntimeApplicationBase::GetRendererType() const {
  return renderer_type_;
}

void RuntimeApplicationBase::StopApplication(
    cast::common::StopReason::Type stop_reason,
    int32_t net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_application_running_) {
    return;
  }
  is_application_running_ = false;

  if (delegate_->GetWebContents()) {
    auto* cast_web_contents =
        CastWebContents::FromWebContents(delegate_->GetWebContents());
    DCHECK(cast_web_contents);
    cast_web_contents->ClosePage();

    // Check if window is still available as page might have been closed before.
    if (delegate_->GetCastContentWindow()) {
      delegate_->GetCastContentWindow()->RemoveObserver(this);
    }
  }

  delegate().NotifyApplicationStopped(stop_reason, net_error_code);

  LOG(INFO) << "Application is stopped: stop_reason="
            << cast::common::StopReason::Type_Name(stop_reason) << ", "
            << *this;
}

void RuntimeApplicationBase::OnVisibilityChange(
    VisibilityType visibility_type) {
  DCHECK(delegate_->GetWebContents());
  auto* cast_web_contents =
      CastWebContents::FromWebContents(delegate_->GetWebContents());
  DCHECK(cast_web_contents);
  switch (visibility_type) {
    case VisibilityType::FULL_SCREEN:
    case VisibilityType::PARTIAL_OUT:
    case VisibilityType::TRANSIENTLY_HIDDEN:
      LOG(INFO) << "Application is visible now: " << *this;
      cast_web_contents->SetWebVisibilityAndPaint(true);
      break;

    default:
      LOG(INFO) << "Application is hidden now: " << *this;
      cast_web_contents->SetWebVisibilityAndPaint(false);
      break;
  }
}

}  // namespace chromecast
