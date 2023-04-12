// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/mouse_lock_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
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

// The amount of time to disallow repeated pointer lock calls after the user
// successfully escapes from one lock request.
constexpr base::TimeDelta kEffectiveUserEscapeDuration =
    base::Milliseconds(1250);

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

  // To prevent misbehaving sites from constantly re-locking the mouse, the
  // lock-requesting page must have transient user activation and it must not
  // request for a lock within |kEffectiveUserEscapeDuration| time since the
  // user successfully escaped from a previous lock.  Exceptions are when the
  // page has unlocked (i.e. not the user), or if we're in tab fullscreen (which
  // requires its own transient user activation).
  if (!last_unlocked_by_target && !web_contents->IsFullscreen()) {
    if (!user_gesture) {
      web_contents->GotResponseToLockMouseRequest(
          blink::mojom::PointerLockResult::kRequiresUserGesture);
      if (lock_state_callback_for_test_)
        std::move(lock_state_callback_for_test_).Run();
      return;
    }
    if (base::TimeTicks::Now() <
        last_user_escape_time_ + kEffectiveUserEscapeDuration) {
      web_contents->GotResponseToLockMouseRequest(
          blink::mojom::PointerLockResult::kUserRejected);
      if (lock_state_callback_for_test_)
        std::move(lock_state_callback_for_test_).Run();
      return;
    }
  }
  SetTabWithExclusiveAccess(web_contents);

  // Lock mouse.
  if (fake_mouse_lock_for_test_ ||
      web_contents->GotResponseToLockMouseRequest(
          blink::mojom::PointerLockResult::kSuccess)) {
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

  if (!ShouldSuppressBubbleReshowForStateChange()) {
    exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent(
        base::BindOnce(&MouseLockController::OnBubbleHidden,
                       weak_ptr_factory_.GetWeakPtr(), web_contents));
  }

  if (lock_state_callback_for_test_)
    std::move(lock_state_callback_for_test_).Run();
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

bool MouseLockController::HandleUserPressedEscape() {
  if (IsMouseLocked()) {
    ExitExclusiveAccessIfNecessary();
    last_user_escape_time_ = base::TimeTicks::Now();
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

  mouse_lock_state_ = MOUSELOCK_UNLOCKED;
  SetTabWithExclusiveAccess(nullptr);

  if (!ShouldSuppressBubbleReshowForStateChange()) {
    exclusive_access_manager()->UpdateExclusiveAccessExitBubbleContent(
        ExclusiveAccessBubbleHideCallback());
  }
}

void MouseLockController::UnlockMouse() {
  WebContents* tab = exclusive_access_tab();

  if (!tab)
    return;

  content::RenderWidgetHostView* mouse_lock_view = nullptr;
  RenderViewHost* const rvh =
      exclusive_access_tab()->GetPrimaryMainFrame()->GetRenderViewHost();
  if (rvh)
    mouse_lock_view = rvh->GetWidget()->GetView();

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

bool MouseLockController::ShouldSuppressBubbleReshowForStateChange() {
  ExclusiveAccessBubbleType bubble_type =
      exclusive_access_manager()->GetExclusiveAccessExitBubbleType();
  return (mouse_lock_state_ == MOUSELOCK_LOCKED &&
          bubble_type ==
              EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_MOUSELOCK_EXIT_INSTRUCTION) ||
         (mouse_lock_state_ == MOUSELOCK_UNLOCKED &&
          bubble_type ==
              EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION);
}
