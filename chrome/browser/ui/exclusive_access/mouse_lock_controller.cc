// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

using content::RenderViewHost;
using content::WebContents;

namespace {

const char kMouseLockBubbleReshowsHistogramName[] =
    "ExclusiveAccess.BubbleReshowsPerSession.MouseLock";

}  // namespace

MouseLockController::MouseLockController(ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager),
      mouse_lock_state_(MOUSELOCK_UNLOCKED),
      fake_mouse_lock_for_test_(false),
      bubble_hide_callback_for_test_() {}

MouseLockController::~MouseLockController() {
}

bool MouseLockController::IsMouseLocked() const {
  return mouse_lock_state_ == MOUSELOCK_LOCKED ||
         mouse_lock_state_ == MOUSELOCK_LOCKED_SILENTLY;
}

bool MouseLockController::IsMouseLockedSilently() const {
  return mouse_lock_state_ == MOUSELOCK_LOCKED_SILENTLY;
}

void MouseLockController::RequestToLockMouse(WebContents* web_contents,
                                             bool user_gesture,
                                             bool last_unlocked_by_target) {
  DCHECK(!IsMouseLocked());
  if (lock_state_callback_for_test_)
    std::move(lock_state_callback_for_test_).Run();

  // Must have a user gesture to prevent misbehaving sites from constantly
  // re-locking the mouse. Exceptions are when the page has unlocked
  // (i.e. not the user), or if we're in tab fullscreen (user gesture required
  // for that)
  if (!last_unlocked_by_target && !user_gesture &&
      !exclusive_access_manager()
           ->fullscreen_controller()
           ->IsFullscreenForTabOrPending(web_contents)) {
    web_contents->GotResponseToLockMouseRequest(false);
    return;
  }
  SetTabWithExclusiveAccess(web_contents);

  // Lock mouse.
  if (fake_mouse_lock_for_test_ ||
      web_contents->GotResponseToLockMouseRequest(true)) {
    if (last_unlocked_by_target &&
        web_contents_granted_silent_mouse_lock_permission_ == web_contents) {
      mouse_lock_state_ = MOUSELOCK_LOCKED_SILENTLY;
    } else {
      mouse_lock_state_ = MOUSELOCK_LOCKED;
    }
  } else {
    SetTabWithExclusiveAccess(nullptr);
    mouse_lock_state_ = MOUSELOCK_UNLOCKED;
  }

  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent(
      base::BindOnce(&MouseLockController::OnBubbleHidden,
                     weak_ptr_factory_.GetWeakPtr(), web_contents));
}

void MouseLockController::ExitExclusiveAccessIfNecessary() {
  NotifyTabExclusiveAccessLost();
}

void MouseLockController::NotifyTabExclusiveAccessLost() {
  WebContents* tab = exclusive_access_tab();
  if (tab) {
    UnlockMouse();
    SetTabWithExclusiveAccess(nullptr);
    mouse_lock_state_ = MOUSELOCK_UNLOCKED;
    exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent(
        ExclusiveAccessBubbleHideCallback());
  }
}

void MouseLockController::RecordBubbleReshowsHistogram(
    int bubble_reshow_count) {
  UMA_HISTOGRAM_COUNTS_100(kMouseLockBubbleReshowsHistogramName,
                           bubble_reshow_count);
}

bool MouseLockController::HandleUserPressedEscape() {
  if (IsMouseLocked()) {
    ExitExclusiveAccessIfNecessary();
    return true;
  }

  return false;
}

void MouseLockController::ExitExclusiveAccessToPreviousState() {
  // Nothing to do for mouse lock.
}

void MouseLockController::LostMouseLock() {
  if (lock_state_callback_for_test_)
    std::move(lock_state_callback_for_test_).Run();

  RecordExitingUMA();
  mouse_lock_state_ = MOUSELOCK_UNLOCKED;
  SetTabWithExclusiveAccess(nullptr);
  exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent(
      ExclusiveAccessBubbleHideCallback());
}

void MouseLockController::UnlockMouse() {
  WebContents* tab = exclusive_access_tab();

  if (!tab)
    return;

  content::RenderWidgetHostView* mouse_lock_view = nullptr;
  FullscreenController* fullscreen_controller =
      exclusive_access_manager()->fullscreen_controller();
  if ((fullscreen_controller->exclusive_access_tab() == tab) &&
      fullscreen_controller->IsPrivilegedFullscreenForTab()) {
    mouse_lock_view =
        exclusive_access_tab()->GetFullscreenRenderWidgetHostView();
  }

  if (!mouse_lock_view) {
    RenderViewHost* const rvh = exclusive_access_tab()->GetRenderViewHost();
    if (rvh)
      mouse_lock_view = rvh->GetWidget()->GetView();
  }

  if (mouse_lock_view)
    mouse_lock_view->UnlockMouse();
}

void MouseLockController::OnBubbleHidden(
    WebContents* web_contents,
    ExclusiveAccessBubbleHideReason reason) {
  if (bubble_hide_callback_for_test_)
    bubble_hide_callback_for_test_.Run(reason);

  // Allow silent mouse lock if the bubble has been display for a period of
  // time and dismissed due to timeout.
  if (reason == ExclusiveAccessBubbleHideReason::kTimeout)
    web_contents_granted_silent_mouse_lock_permission_ = web_contents;
}
