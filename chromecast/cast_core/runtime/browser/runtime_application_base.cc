// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_base.h"

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/common/feature_constants.h"
#include "components/cast_receiver/browser/permissions_manager_impl.h"
#include "components/media_control/browser/media_blocker.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {

RuntimeApplicationBase::Delegate::~Delegate() = default;

RuntimeApplicationBase::RuntimeApplicationBase(
    std::string cast_session_id,
    cast_receiver::ApplicationConfig app_config,
    cast_receiver::ApplicationClient& application_client)
    : cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
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
  return config().display_name;
}

const std::string& RuntimeApplicationBase::GetAppId() const {
  return config().app_id;
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
  StopApplication(Delegate::ApplicationStopReason::kUserRequest,
                  /*net_error_code=*/0);
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

void RuntimeApplicationBase::LoadPage(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  delegate().LoadPage(url);

  SetWebVisibilityAndPaint(false);
}

void RuntimeApplicationBase::SetContentPermissions(
    content::WebContents& web_contents) {
  cast_receiver::PermissionsManagerImpl* permissions_manager =
      cast_receiver::PermissionsManagerImpl::CreateInstance(web_contents,
                                                            GetAppId());
  if (config().url.has_value()) {
    auto app_url_origin = url::Origin::Create(config().url.value());
    if (!app_url_origin.opaque()) {
      permissions_manager->AddOrigin(app_url_origin);
    }
  }
  for (blink::PermissionType permission : config().permissions.permissions) {
    permissions_manager->AddPermission(permission);
  }
  for (auto& origin : config().permissions.additional_origins) {
    DCHECK(!origin.opaque());
    permissions_manager->AddOrigin(origin);
  }
}

void RuntimeApplicationBase::OnPageLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "Page loaded: " << *this;

  auto* window_controls = delegate_->GetContentWindowControls();
  DCHECK(window_controls);
  window_controls->AddVisibilityChangeObserver(*this);
  if (is_touch_input_enabled_) {
    window_controls->EnableTouchInput();
  } else {
    window_controls->DisableTouchInput();
  }

  // Create the window and show the web view.
  if (is_visible_) {
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

void RuntimeApplicationBase::SetMediaBlocking(bool load_blocked,
                                              bool start_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_media_load_blocked_ = load_blocked;
  is_media_start_blocked_ = start_blocked;
  LOG(INFO) << "Media state updated: is_load_blocked=" << load_blocked
            << ", is_start_blocked=" << start_blocked << ", " << *this;

  if (!delegate_->GetWebContents()) {
    return;
  }

  auto* application_controls = GetApplicationControls();
  DCHECK(application_controls);
  media_control::MediaBlocker& media_blocker =
      application_controls->GetMediaBlocker();

  media_blocker.BlockMediaLoading(is_media_load_blocked_);

  // TODO(crbug.com/1359584): Block media starting.
}

void RuntimeApplicationBase::SetVisibility(bool is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_visible_ = is_visible;
  LOG(INFO) << "Visibility updated: is_visible_=" << is_visible_ << ", "
            << *this;

  auto* window_controls = delegate_->GetContentWindowControls();
  if (!window_controls) {
    return;
  }

  if (is_visible_) {
    window_controls->ShowWindow();
  } else {
    window_controls->HideWindow();
  }
}

void RuntimeApplicationBase::SetTouchInputEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_touch_input_enabled_ = enabled;
  LOG(INFO) << "Touch input updated: is_touch_input_enabled_= "
            << is_touch_input_enabled_ << ", " << *this;

  auto* window_controls = delegate_->GetContentWindowControls();
  if (!window_controls) {
    return;
  }

  if (is_touch_input_enabled_) {
    window_controls->EnableTouchInput();
  } else {
    window_controls->DisableTouchInput();
  }
}

bool RuntimeApplicationBase::IsApplicationRunning() const {
  return is_application_running_;
}

void RuntimeApplicationBase::StopApplication(
    Delegate::ApplicationStopReason stop_reason,
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

  LOG(INFO) << "Application is stopped: stop_reason=" << stop_reason << ", "
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

std::ostream& operator<<(
    std::ostream& os,
    RuntimeApplicationBase::Delegate::ApplicationStopReason reason) {
  switch (reason) {
    case RuntimeApplicationBase::Delegate::ApplicationStopReason::kUndefined:
      return os << "Undefined";
    case RuntimeApplicationBase::Delegate::ApplicationStopReason::
        kApplicationRequest:
      return os << "Application Request";
    case RuntimeApplicationBase::Delegate::ApplicationStopReason::kIdleTimeout:
      return os << "Idle Timeout";
    case RuntimeApplicationBase::Delegate::ApplicationStopReason::kUserRequest:
      return os << "Use Request";
    case RuntimeApplicationBase::Delegate::ApplicationStopReason::kHttpError:
      return os << "HTTP Error";
    case RuntimeApplicationBase::Delegate::ApplicationStopReason::kRuntimeError:
      return os << "Runtime Error";
  }
}

}  // namespace chromecast
