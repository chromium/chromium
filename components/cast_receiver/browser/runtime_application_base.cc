// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/runtime_application_base.h"

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/sequenced_task_runner.h"
#include "components/cast_receiver/browser/permissions_manager_impl.h"
#include "components/media_control/browser/media_blocker.h"
#include "components/url_rewrite/browser/url_request_rewrite_rules_manager.h"
#include "content/public/browser/web_contents.h"

namespace cast_receiver {

RuntimeApplicationBase::RuntimeApplicationBase(
    std::string cast_session_id,
    ApplicationConfig app_config,
    ApplicationClient& application_client)
    : cast_session_id_(std::move(cast_session_id)),
      app_config_(std::move(app_config)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      application_client_(application_client) {
  DCHECK(task_runner_);
}

RuntimeApplicationBase::~RuntimeApplicationBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!is_application_running_);
}

void RuntimeApplicationBase::SetEmbedderApplication(
    EmbedderApplication& embedder_application) {
  DCHECK(!embedder_application_);
  embedder_application_ = &embedder_application;
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
  DCHECK(embedder_application().GetWebContents());

  is_application_running_ = true;
  if (cached_mojom_rules_) {
    // Apply cached URL rewrite rules before anything is done with the page.
    SetUrlRewriteRules(std::move(cached_mojom_rules_));
  }

  DLOG(INFO) << "Loaded application: " << *this;
  std::move(callback).Run(OkStatus());
}

void RuntimeApplicationBase::Stop(StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopApplication(EmbedderApplication::ApplicationStopReason::kUserRequest,
                  net::ERR_ABORTED);
  std::move(callback).Run(OkStatus());
}

ApplicationClient::ApplicationControls&
RuntimeApplicationBase::GetApplicationControls() {
  DCHECK(embedder_application().GetWebContents());

  return application_client_->GetApplicationControls(
      *embedder_application().GetWebContents());
}

void RuntimeApplicationBase::NavigateToPage(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto* window_controls = embedder_application().GetContentWindowControls();
  DCHECK(window_controls);
  window_controls->AddVisibilityChangeObserver(*this);

  embedder_application().NavigateToPage(url);

  SetWebVisibilityAndPaint(is_visible_);
}

void RuntimeApplicationBase::SetContentPermissions(
    content::WebContents& web_contents) {
  PermissionsManagerImpl* permissions_manager =
      PermissionsManagerImpl::CreateInstance(web_contents, GetAppId());
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

void RuntimeApplicationBase::OnPageNavigationComplete() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DLOG(INFO) << "Page loaded: " << *this;

  embedder_application().NotifyApplicationStarted();

  SetWebVisibilityAndPaint(is_visible_);
  SetTouchInputEnabled(is_touch_input_enabled_);
  SetMediaBlocking(is_media_load_blocked_, is_media_start_blocked_);
}

void RuntimeApplicationBase::SetUrlRewriteRules(
    url_rewrite::mojom::UrlRequestRewriteRulesPtr mojom_rules) {
  if (!embedder_application().GetWebContents()) {
    cached_mojom_rules_ = std::move(mojom_rules);
    return;
  }

  url_rewrite::UrlRequestRewriteRulesManager&
      url_request_rewrite_rules_manager =
          GetApplicationControls().GetUrlRequestRewriteRulesManager();
  if (!url_request_rewrite_rules_manager.OnRulesUpdated(
          std::move(mojom_rules))) {
    LOG(ERROR) << "URL rewrite rules update failed.";
    StopApplication(EmbedderApplication::ApplicationStopReason::kRuntimeError,
                    net::Error::ERR_UNEXPECTED);
  }
}

void RuntimeApplicationBase::SetMediaBlocking(bool load_blocked,
                                              bool start_blocked) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_media_load_blocked_ = load_blocked;
  is_media_start_blocked_ = start_blocked;
  DLOG(INFO) << "Media state updated: is_load_blocked=" << load_blocked
             << ", is_start_blocked=" << start_blocked << ", " << *this;

  if (!embedder_application().GetWebContents()) {
    return;
  }

  media_control::MediaBlocker& media_blocker =
      GetApplicationControls().GetMediaBlocker();

  media_blocker.BlockMediaLoading(is_media_load_blocked_);

  // TODO(crbug.com/1359584): Block media starting.
}

void RuntimeApplicationBase::SetVisibility(bool is_visible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_visible_ = is_visible;
  DLOG(INFO) << "Visibility updated: is_visible_=" << is_visible_ << ", "
             << *this;

  auto* window_controls = embedder_application().GetContentWindowControls();
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
  DLOG(INFO) << "Touch input updated: is_touch_input_enabled_= "
             << is_touch_input_enabled_ << ", " << *this;

  auto* window_controls = embedder_application().GetContentWindowControls();
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
    EmbedderApplication::ApplicationStopReason stop_reason,
    net::Error net_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_application_running_) {
    return;
  }
  is_application_running_ = false;

  auto* web_contents = embedder_application().GetWebContents();
  if (web_contents) {
    web_contents->DispatchBeforeUnload(false /* auto_cancel */);
    web_contents->ClosePage();

    // Check if window is still available as page might have been closed before.
    auto* window_controls = embedder_application().GetContentWindowControls();
    if (window_controls) {
      window_controls->RemoveVisibilityChangeObserver(*this);
    }
  }

  embedder_application().NotifyApplicationStopped(stop_reason, net_error_code);

  DLOG(INFO) << "Application is stopped: stop_reason=" << stop_reason << ", "
             << *this;
}

void RuntimeApplicationBase::SetWebVisibilityAndPaint(bool is_visible) {
  auto* web_contents = embedder_application().GetWebContents();
  if (!web_contents) {
    return;
  }

  if (is_visible) {
    web_contents->WasShown();
  } else {
    // NOTE: Calling WasHidden() and later WasShown() does not behave properly
    // on some platforms (e.g. Linux devices using X11 platform for Ozone). In
    // such cases, the WasShown() call will execute, and the browser-side code
    // associated with this call will run, but it will never reach the Renderer
    // process, so the LayerTreeHost will never draw the surface assocaited with
    // this WebContents.
    DLOG(WARNING)
        << "WebContents hidden. NOTE: Changing from hidden to visible does not "
           "work in all cases, and such calls may not be respected.";
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

}  // namespace cast_receiver
