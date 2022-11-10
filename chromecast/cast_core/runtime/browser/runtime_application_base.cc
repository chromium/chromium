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
#include "components/media_control/browser/media_blocker.h"
#include "content/public/browser/web_contents.h"

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
    mojom::RendererType renderer_type,
    cast_receiver::ApplicationClient& application_client)
    : cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
      renderer_type_(renderer_type),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      application_client_(application_client) {
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
  std::move(callback).Run(cast_receiver::OkStatus());
}

void RuntimeApplicationBase::Stop(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(cast::common::StopReason::USER_REQUEST, /*net_error_code=*/0);
  std::move(callback).Run(cast_receiver::OkStatus());
}

cast_receiver::ApplicationClient::ApplicationControls*
RuntimeApplicationBase::GetApplicationControls() {
  if (!delegate().GetWebContents()) {
    return nullptr;
  }

  return &application_client_->GetApplicationControls(
      *delegate().GetWebContents());
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
  SetWebVisibilityAndPaint(false);
}

void RuntimeApplicationBase::OnPageLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "Page loaded: " << *this;

  auto* window_controls = delegate_->GetContentWindowControls();
  DCHECK(window_controls);
  window_controls->AddVisibilityChangeObserver(*this);
  if (touch_input_ == cast::common::TouchInput::ENABLED) {
    window_controls->EnableTouchInput();
  } else {
    window_controls->DisableTouchInput();
  }

  // Create the window and show the web view.
  if (visibility_ == cast::common::Visibility::FULL_SCREEN) {
    LOG(INFO) << "Loading page in full screen: " << *this;
    window_controls->ShowWindow();
  } else {
    LOG(INFO) << "Loading page in background: " << *this;
    window_controls->HideWindow();
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

  auto* application_controls = GetApplicationControls();
  DCHECK(application_controls);
  media_control::MediaBlocker& media_blocker =
      application_controls->GetMediaBlocker();
  switch (media_state_) {
    case cast::common::MediaState::LOAD_BLOCKED:
      media_blocker.BlockMediaLoading(true);
      // TODO(crbug.com/1359584): Block media starting.
      break;

    case cast::common::MediaState::START_BLOCKED:
      media_blocker.BlockMediaLoading(false);
      // TODO(crbug.com/1359584): Block media starting.
      break;

    case cast::common::MediaState::UNBLOCKED:
      media_blocker.BlockMediaLoading(false);
      // TODO(crbug.com/1359584): Allow media starting.
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

  auto* window_controls = delegate_->GetContentWindowControls();
  if (!window_controls) {
    return;
  }

  switch (visibility_) {
    case cast::common::Visibility::FULL_SCREEN:
      window_controls->ShowWindow();
      break;

    case cast::common::Visibility::HIDDEN:
      window_controls->HideWindow();
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

  auto* window_controls = delegate_->GetContentWindowControls();
  if (!window_controls) {
    return;
  }

  if (touch_input_ == cast::common::TouchInput::ENABLED) {
    window_controls->EnableTouchInput();
  } else {
    window_controls->DisableTouchInput();
  }
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
    auto* window_controls = delegate_->GetContentWindowControls();
    if (window_controls) {
      window_controls->RemoveVisibilityChangeObserver(*this);
    }
  }

  delegate().NotifyApplicationStopped(stop_reason, net_error_code);

  LOG(INFO) << "Application is stopped: stop_reason="
            << cast::common::StopReason::Type_Name(stop_reason) << ", "
            << *this;
}

void RuntimeApplicationBase::SetWebVisibilityAndPaint(bool is_visible) {
  auto* web_contents = delegate_->GetWebContents();
  if (!web_contents) {
    return;
  }

  if (is_visible) {
    web_contents->WasShown();
  } else {
    web_contents->WasHidden();
  }

  if (web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    // Since we are managing the visibility, we need to ensure pages are
    // unfrozen in the event this occurred while in the background.
    web_contents->SetPageFrozen(false);
  }
}

void RuntimeApplicationBase::OnWindowShown() {
  SetWebVisibilityAndPaint(true);
}

void RuntimeApplicationBase::OnWindowHidden() {
  SetWebVisibilityAndPaint(false);
}

}  // namespace chromecast
