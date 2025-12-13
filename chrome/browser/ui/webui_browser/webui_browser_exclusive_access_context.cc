// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_exclusive_access_context.h"

#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

// TODO(webium): We support both browser and tab fullscreen by passing an enum
// through mojom in |WebUIBrowserWindow::ProcessFullscreen|, but this differs
// from normal browser behavior where the toolbar hides in browser fullscreen
// when navigating away from the NTP. Webium does not support that behavior yet.

// TODO(webium): Support immersive mode on Mac and honor platform preferences
// like "Always show toolbar in fullscreen" on macOS and other platforms.

WebUIBrowserExclusiveAccessContext::WebUIBrowserExclusiveAccessContext(
    Profile* profile,
    BrowserWindowInterface* browser,
    TabStripModel* tab_strip_model,
    views::Widget* widget,
    ui::AcceleratorProvider* accelerator_provider)
    : profile_(profile),
      browser_(browser),
      tab_strip_model_(tab_strip_model),
      widget_(widget),
      accelerator_provider_(accelerator_provider) {}

WebUIBrowserExclusiveAccessContext::~WebUIBrowserExclusiveAccessContext() =
    default;

Profile* WebUIBrowserExclusiveAccessContext::GetProfile() {
  return profile_;
}

void WebUIBrowserExclusiveAccessContext::EnterFullscreen(
    const url::Origin& origin,
    ExclusiveAccessBubbleType bubble_type,
    FullscreenTabParams fullscreen_tab_params) {
  const int64_t display_id = fullscreen_tab_params.display_id;
  if (IsFullscreen() && display_id == display::kInvalidDisplayId) {
    return;
  }

  WebUIBrowserWindow::FromBrowser(browser_)->ProcessFullscreen(true);
}

void WebUIBrowserExclusiveAccessContext::ExitFullscreen() {
  if (browser_->GetBrowserForMigrationOnly()->window()->IsForceFullscreen()) {
    return;
  }

  if (!IsFullscreen()) {
    return;
  }

  WebUIBrowserWindow::FromBrowser(browser_)->ProcessFullscreen(false);
}

void WebUIBrowserExclusiveAccessContext::UpdateExclusiveAccessBubble(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback) {
  // Trusted pinned mode does not allow to escape. So do not show the bubble.
  const bool is_trusted_pinned = platform_util::IsBrowserLockedFullscreen(
      browser_->GetBrowserForMigrationOnly());

  // Whether we should remove the bubble if it exists, or not show the bubble.
  bool should_close_bubble = is_trusted_pinned;
  if (!params.has_download) {
    // ...TYPE_NONE indicates deleting the bubble, except when used with
    // download.
    should_close_bubble |= params.type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
  }

  if (should_close_bubble) {
    if (first_hide_callback) {
      std::move(first_hide_callback)
          .Run(ExclusiveAccessBubbleHideReason::kNotShown);
    }

    // If we intend to close the bubble but it has already been deleted no
    // action is needed.
    if (!exclusive_access_bubble_) {
      return;
    }
    // Exit if we've already queued up a task to close the bubble.
    if (exclusive_access_bubble_destruction_task_id_) {
      return;
    }
    // `HideImmediately()` will trigger a callback for the current bubble with
    // `ExclusiveAccessBubbleHideReason::kInterrupted` if available.
    exclusive_access_bubble_->HideImmediately();

    // Perform the destroy async. State updates in the exclusive access bubble
    // view may call back into this method. This otherwise results in
    // premature deletion of the bubble view and UAFs. See crbug.com/1426521.
    exclusive_access_bubble_destruction_task_id_ =
        exclusive_access_bubble_cancelable_task_tracker_.PostTask(
            base::SingleThreadTaskRunner::GetCurrentDefault().get(), FROM_HERE,
            base::BindOnce(&WebUIBrowserExclusiveAccessContext::
                               DestroyAnyExclusiveAccessBubble,
                           weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (exclusive_access_bubble_) {
    if (exclusive_access_bubble_destruction_task_id_) {
      // We previously posted a destruction task, but now we want to reuse the
      // bubble. Cancel the destruction task.
      exclusive_access_bubble_cancelable_task_tracker_.TryCancel(
          exclusive_access_bubble_destruction_task_id_.value());
      exclusive_access_bubble_destruction_task_id_.reset();
    }
    exclusive_access_bubble_->Update(params, std::move(first_hide_callback));
    return;
  }

  exclusive_access_bubble_ = std::make_unique<ExclusiveAccessBubbleViews>(
      this, params, std::move(first_hide_callback));
}

bool WebUIBrowserExclusiveAccessContext::IsExclusiveAccessBubbleDisplayed()
    const {
  return exclusive_access_bubble_ && (exclusive_access_bubble_->IsShowing() ||
                                      exclusive_access_bubble_->IsVisible());
}

void WebUIBrowserExclusiveAccessContext::OnExclusiveAccessUserInput() {
  if (exclusive_access_bubble_.get()) {
    exclusive_access_bubble_->OnUserInput();
  }
}

content::WebContents*
WebUIBrowserExclusiveAccessContext::GetWebContentsForExclusiveAccess() {
  return tab_strip_model_->GetActiveWebContents();
}

bool WebUIBrowserExclusiveAccessContext::CanUserEnterFullscreen() const {
  return true;
}

bool WebUIBrowserExclusiveAccessContext::CanUserExitFullscreen() const {
  return !platform_util::IsBrowserLockedFullscreen(
      browser_->GetBrowserForMigrationOnly());
}

bool WebUIBrowserExclusiveAccessContext::IsFullscreen() const {
  return widget_->IsFullscreen();
}

ExclusiveAccessManager*
WebUIBrowserExclusiveAccessContext::GetExclusiveAccessManager() {
  // The exclusive_access_manager is created in InitPostWindowConstruction,
  // so it might not be available during early initialization.
  auto* manager = browser_->GetFeatures().exclusive_access_manager();
  DCHECK(manager) << "ExclusiveAccessManager should be initialized before use";
  return manager;
}

ui::AcceleratorProvider*
WebUIBrowserExclusiveAccessContext::GetAcceleratorProvider() {
  return accelerator_provider_;
}

gfx::NativeView WebUIBrowserExclusiveAccessContext::GetBubbleParentView()
    const {
  return widget_->GetNativeView();
}

gfx::Rect WebUIBrowserExclusiveAccessContext::GetClientAreaBoundsInScreen()
    const {
  return widget_->GetClientAreaBoundsInScreen();
}

bool WebUIBrowserExclusiveAccessContext::IsImmersiveModeEnabled() const {
  // TODO(webium): Once WebUIBrowserWindow has an ImmersiveModeController
  // instantiated in BrowserWindowFeatures, pass it into the constructor to make
  // the dependency clear. For now, return false as immersive mode is only
  // supported for BrowserView.
  NOTIMPLEMENTED();
  return false;
}

gfx::Rect WebUIBrowserExclusiveAccessContext::GetTopContainerBoundsInScreen() {
  return gfx::Rect();
}

void WebUIBrowserExclusiveAccessContext::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
  exclusive_access_bubble_destruction_task_id_.reset();
}
