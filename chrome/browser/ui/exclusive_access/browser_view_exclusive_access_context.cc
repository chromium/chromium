// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/browser_view_exclusive_access_context.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/fullscreen/browser_window_fullscreen_controller.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/views/exclusive_access/exclusive_access_bubble_views.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/chromeos/locked_state/locked_state_controller.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"

namespace {

// Browser fullscreen uses immersive mode except in kiosk mode.
bool ShouldUseImmersiveFullscreen(ExclusiveAccessBubbleType type) {
  if (IsRunningInAppMode()) {
    return false;
  }
  return type ==
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION;
}

}  // namespace
#endif

DEFINE_USER_DATA(BrowserViewExclusiveAccessContext);

// static
BrowserViewExclusiveAccessContext* BrowserViewExclusiveAccessContext::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

BrowserViewExclusiveAccessContext::BrowserViewExclusiveAccessContext(
    BrowserWindowInterface& browser_window,
    BrowserView& browser_view)
    : browser_window_(browser_window),
      browser_view_(browser_view),
      scoped_unowned_user_data_(browser_window.GetUnownedUserDataHost(),
                                *this) {}

BrowserViewExclusiveAccessContext::~BrowserViewExclusiveAccessContext() =
    default;

Profile* BrowserViewExclusiveAccessContext::GetProfile() {
  return browser_window_->GetProfile();
}

bool BrowserViewExclusiveAccessContext::IsFullscreen() const {
  return browser_view_->IsFullscreen();
}

void BrowserViewExclusiveAccessContext::UpdateUIForTabFullscreen() {
  browser_view_->GetFrameView()->UpdateFullscreenTopUI();
  // Layout can change in vertical tabstrip mode when going from fullscreen
  // to tab fullscreen and vice-versa.
  browser_view_->InvalidateLayout();
}

void BrowserViewExclusiveAccessContext::EnterFullscreen(
    const url::Origin& origin,
    ExclusiveAccessBubbleType bubble_type,
    FullscreenTabParams fullscreen_tab_params) {
  int64_t display_id = fullscreen_tab_params.display_id;
  if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    browser_view_->RequestFullscreen(true, display_id);
  } else {
    auto* screen = display::Screen::Get();
    auto display =
        screen->GetDisplayNearestWindow(browser_view_->GetNativeWindow());
    const bool requesting_another_screen =
        display_id != display.id() && display_id != display::kInvalidDisplayId;
    if (IsFullscreen() && !requesting_another_screen) {
      return;
    }
    browser_view_->ProcessFullscreen(true, display_id);
  }
}

void BrowserViewExclusiveAccessContext::ExitFullscreen() {
  if (BrowserWindowFullscreenController::From(&browser_window_.get())
          ->IsForceFullscreen()) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kAsyncFullscreenWindowState)) {
    browser_view_->RequestFullscreen(false, display::kInvalidDisplayId);
  } else {
    if (!IsFullscreen()) {
      return;
    }
    browser_view_->ProcessFullscreen(false, display::kInvalidDisplayId);
  }
}

void BrowserViewExclusiveAccessContext::UpdateExclusiveAccessBubble(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback) {
  // Whether we should remove the bubble if it exists, or not show the bubble.
  // TODO(jamescook): Figure out what to do with mouse-lock.
  bool should_close_bubble = false;
#if BUILDFLAG(IS_CHROMEOS)
  // Trusted pinned mode does not allow to escape. So do not show the bubble.
  should_close_bubble = !CanUserExitFullscreen();
#endif
  if (!params.has_download) {
    // ...TYPE_NONE indicates deleting the bubble, except when used with
    // download.
    should_close_bubble |= params.type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
#if BUILDFLAG(IS_CHROMEOS)
    // Immersive mode allows the toolbar to be shown, so do not show the
    // bubble. However, do show the bubble in a managed guest session (see
    // crbug.com/741069).
    // Immersive mode logic for downloads is handled by the download
    // controller.
    should_close_bubble |= ShouldUseImmersiveFullscreen(params.type) &&
                           !chromeos::IsManagedGuestSession();
#endif
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
    // premature deletion of the bubble view and UAFs. See crbug.com/40063714.
    exclusive_access_bubble_destruction_task_id_ =
        exclusive_access_bubble_cancelable_task_tracker_.PostTask(
            base::SingleThreadTaskRunner::GetCurrentDefault().get(), FROM_HERE,
            base::BindOnce(&BrowserViewExclusiveAccessContext::
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

bool BrowserViewExclusiveAccessContext::IsExclusiveAccessBubbleDisplayed()
    const {
  return exclusive_access_bubble_ && (exclusive_access_bubble_->IsShowing() ||
                                      exclusive_access_bubble_->IsVisible());
}

void BrowserViewExclusiveAccessContext::OnExclusiveAccessUserInput() {
  if (exclusive_access_bubble_) {
    exclusive_access_bubble_->OnUserInput();
  }
}

content::WebContents*
BrowserViewExclusiveAccessContext::GetWebContentsForExclusiveAccess() {
  return browser_view_->GetActiveWebContents();
}

bool BrowserViewExclusiveAccessContext::CanUserEnterFullscreen() const {
  return browser_view_->CanFullscreen();
}

bool BrowserViewExclusiveAccessContext::CanUserExitFullscreen() const {
#if BUILDFLAG(IS_CHROMEOS)
  if (features::IsUseUnifiedLockedStateControllerEnabled()) {
    return !chromeos::LockedStateController::From(&browser_window_.get())
                ->IsLockedFullscreen();
  }
  return !platform_util::IsBrowserLockedFullscreen(&browser_window_.get());
#else
  return true;
#endif
}

ExclusiveAccessManager*
BrowserViewExclusiveAccessContext::GetExclusiveAccessManager() {
  // The manager is constructed after this context and borrows it. Look it up
  // only when the bubble needs it, rather than during context construction.
  return ExclusiveAccessManager::From(&browser_window_.get());
}

ui::AcceleratorProvider*
BrowserViewExclusiveAccessContext::GetAcceleratorProvider() {
  return &browser_view_.get();
}

gfx::NativeView BrowserViewExclusiveAccessContext::GetBubbleParentView() const {
  return browser_view_->GetWidget()->GetNativeView();
}

gfx::Rect BrowserViewExclusiveAccessContext::GetClientAreaBoundsInScreen()
    const {
  return browser_view_->GetWidget()->GetClientAreaBoundsInScreen();
}

bool BrowserViewExclusiveAccessContext::IsImmersiveModeEnabled() const {
  return ImmersiveModeController::From(&browser_window_.get())->IsEnabled();
}

gfx::Rect BrowserViewExclusiveAccessContext::GetTopContainerBoundsInScreen() {
  return browser_view_->top_container()->GetBoundsInScreen();
}

void BrowserViewExclusiveAccessContext::DestroyAnyExclusiveAccessBubble() {
  exclusive_access_bubble_.reset();
  exclusive_access_bubble_destruction_task_id_.reset();
}
