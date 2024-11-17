// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/pointer_lock_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_permission_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/features.h"
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

PointerLockController::PointerLockController(ExclusiveAccessManager* manager)
    : ExclusiveAccessControllerBase(manager),
      pointer_lock_state_(POINTERLOCK_UNLOCKED),
      fake_pointer_lock_for_test_(false) {}

PointerLockController::~PointerLockController() = default;

bool PointerLockController::IsPointerLocked() const {
  return pointer_lock_state_ == POINTERLOCK_LOCKED ||
         pointer_lock_state_ == POINTERLOCK_LOCKED_SILENTLY;
}

bool PointerLockController::IsPointerLockedSilently() const {
  return pointer_lock_state_ == POINTERLOCK_LOCKED_SILENTLY;
}

void PointerLockController::RequestToLockPointer(WebContents* web_contents,
                                                 bool user_gesture,
                                                 bool last_unlocked_by_target) {
  DCHECK(!IsPointerLocked());

  // To prevent misbehaving sites from constantly re-locking the pointer, the
  // lock-requesting page must have transient user activation and it must not
  // request for a lock within |kEffectiveUserEscapeDuration| time since the
  // user successfully escaped from a previous lock.  Exceptions are when the
  // page has unlocked (i.e. not the user), or if we're in tab fullscreen (which
  // requires its own transient user activation).
  if (!last_unlocked_by_target && !web_contents->IsFullscreen()) {
    if (!user_gesture) {
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kRequiresUserGesture);
      if (lock_state_callback_for_test_)
        std::move(lock_state_callback_for_test_).Run();
      return;
    }
    if (base::TimeTicks::Now() <
        last_user_escape_time_ + kEffectiveUserEscapeDuration) {
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kUserRejected);
      if (lock_state_callback_for_test_)
        std::move(lock_state_callback_for_test_).Run();
      return;
    }
  }

  content::GlobalRenderFrameHostId rfh_id =
      web_contents->GetPrimaryMainFrame()->GetGlobalId();

  if (!base::FeatureList::IsEnabled(
          permissions::features::kKeyboardAndPointerLockPrompt)) {
    LockPointer(web_contents->GetWeakPtr(), rfh_id, last_unlocked_by_target);
    return;
  }

  DCHECK(!IsWaitingForPointerLockPrompt(web_contents));
  hosts_waiting_for_pointer_lock_permission_prompt_.insert(rfh_id);

  exclusive_access_manager()->permission_manager().QueuePermissionRequest(
      blink::PermissionType::POINTER_LOCK,
      base::BindOnce(&PointerLockController::LockPointer,
                     weak_ptr_factory_.GetWeakPtr(), web_contents->GetWeakPtr(),
                     rfh_id, last_unlocked_by_target),
      base::BindOnce(&PointerLockController::RejectRequestToLockPointer,
                     weak_ptr_factory_.GetWeakPtr(), web_contents->GetWeakPtr(),
                     rfh_id),
      web_contents);
}

bool PointerLockController::IsWaitingForPointerLockPrompt(
    WebContents* web_contents) {
  return IsWaitingForPointerLockPromptHelper(
      web_contents->GetPrimaryMainFrame()->GetGlobalId());
}

void PointerLockController::ExitExclusiveAccessIfNecessary() {
  NotifyTabExclusiveAccessLost();
}

void PointerLockController::NotifyTabExclusiveAccessLost() {
  WebContents* tab = exclusive_access_tab();
  if (tab) {
    UnlockPointer();
    SetTabWithExclusiveAccess(nullptr);
    pointer_lock_state_ = POINTERLOCK_UNLOCKED;
    exclusive_access_manager()->UpdateBubble(base::NullCallback());
  }
}

bool PointerLockController::HandleUserPressedEscape() {
  if (IsPointerLocked()) {
    ExitExclusiveAccessIfNecessary();
    last_user_escape_time_ = base::TimeTicks::Now();
    return true;
  }

  return false;
}

void PointerLockController::HandleUserHeldEscape() {
  HandleUserPressedEscape();
}

void PointerLockController::HandleUserReleasedEscapeEarly() {}

bool PointerLockController::RequiresPressAndHoldEscToExit() const {
  return false;
}

void PointerLockController::ExitExclusiveAccessToPreviousState() {
  if (lock_state_callback_for_test_)
    std::move(lock_state_callback_for_test_).Run();

  pointer_lock_state_ = POINTERLOCK_UNLOCKED;
  SetTabWithExclusiveAccess(nullptr);

  if (!ShouldSuppressBubbleReshowForStateChange()) {
    exclusive_access_manager()->UpdateBubble(base::NullCallback());
  }
}

void PointerLockController::UnlockPointer() {
  WebContents* tab = exclusive_access_tab();

  if (!tab)
    return;

  hosts_waiting_for_pointer_lock_permission_prompt_.erase(
      tab->GetPrimaryMainFrame()->GetGlobalId());

  content::RenderWidgetHostView* pointer_lock_view = nullptr;
  RenderViewHost* const rvh =
      exclusive_access_tab()->GetPrimaryMainFrame()->GetRenderViewHost();
  if (rvh)
    pointer_lock_view = rvh->GetWidget()->GetView();

  if (pointer_lock_view)
    pointer_lock_view->UnlockPointer();
}

void PointerLockController::LockPointer(
    base::WeakPtr<content::WebContents> web_contents,
    content::GlobalRenderFrameHostId rfh_id,
    bool last_unlocked_by_target) {
  hosts_waiting_for_pointer_lock_permission_prompt_.erase(rfh_id);

  if (!web_contents) {
    if (lock_state_callback_for_test_) {
      std::move(lock_state_callback_for_test_).Run();
    }
    return;
  }
  SetTabWithExclusiveAccess(web_contents.get());
  // Focus may have moved to the modal, so move it back to the WebContents.
  web_contents->Focus();

  // Lock the mouse pointer.
  if (fake_pointer_lock_for_test_ ||
      web_contents->GotResponseToPointerLockRequest(
          blink::mojom::PointerLockResult::kSuccess)) {
    if (last_unlocked_by_target &&
        web_contents_granted_silent_pointer_lock_permission_ ==
            web_contents.get()) {
      pointer_lock_state_ = POINTERLOCK_LOCKED_SILENTLY;
    } else {
      pointer_lock_state_ = POINTERLOCK_LOCKED;
    }
  } else {
    SetTabWithExclusiveAccess(nullptr);
    pointer_lock_state_ = POINTERLOCK_UNLOCKED;
  }

  if (!ShouldSuppressBubbleReshowForStateChange()) {
    exclusive_access_manager()->UpdateBubble(
        base::BindOnce(&PointerLockController::OnBubbleHidden,
                       weak_ptr_factory_.GetWeakPtr(), web_contents.get()));
  }
  if (lock_state_callback_for_test_) {
    std::move(lock_state_callback_for_test_).Run();
  }
}

void PointerLockController::RejectRequestToLockPointer(
    base::WeakPtr<content::WebContents> web_contents,
    content::GlobalRenderFrameHostId rfh_id) {
  DCHECK(IsWaitingForPointerLockPromptHelper(rfh_id));
  hosts_waiting_for_pointer_lock_permission_prompt_.erase(rfh_id);

  if (!web_contents) {
    if (lock_state_callback_for_test_) {
      std::move(lock_state_callback_for_test_).Run();
    }
    return;
  }

  // Focus has moved to the modal, so move it back to the WebContents.
  web_contents->Focus();
  web_contents->GotResponseToPointerLockRequest(
      blink::mojom::PointerLockResult::kUserRejected);
  if (lock_state_callback_for_test_) {
    std::move(lock_state_callback_for_test_).Run();
  }
}

void PointerLockController::OnBubbleHidden(
    WebContents* web_contents,
    ExclusiveAccessBubbleHideReason reason) {
  if (bubble_hide_callback_for_test_)
    bubble_hide_callback_for_test_.Run(reason);

  // Allow silent pointer lock if the bubble has been display for a period of
  // time and dismissed due to timeout.
  if (reason == ExclusiveAccessBubbleHideReason::kTimeout) {
    web_contents_granted_silent_pointer_lock_permission_ = web_contents;
  } else {
    web_contents_granted_silent_pointer_lock_permission_ = nullptr;
  }
}

bool PointerLockController::ShouldSuppressBubbleReshowForStateChange() {
  ExclusiveAccessBubbleType bubble_type =
      exclusive_access_manager()->GetExclusiveAccessExitBubbleType();
  return (pointer_lock_state_ == POINTERLOCK_LOCKED &&
          bubble_type ==
              EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION) ||
         (pointer_lock_state_ == POINTERLOCK_UNLOCKED &&
          bubble_type ==
              EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION);
}

bool PointerLockController::IsWaitingForPointerLockPromptHelper(
    content::GlobalRenderFrameHostId rfh_id) {
  return hosts_waiting_for_pointer_lock_permission_prompt_.contains(rfh_id);
}
