// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_POINTER_LOCK_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_POINTER_LOCK_CONTROLLER_H_

#include <set>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/global_routing_id.h"

// This class implements the mouse pointer lock behavior.
class PointerLockController : public ExclusiveAccessControllerBase {
 public:
  explicit PointerLockController(ExclusiveAccessManager* manager);

  PointerLockController(const PointerLockController&) = delete;
  PointerLockController& operator=(const PointerLockController&) = delete;

  ~PointerLockController() override;

  // Returns true if the mouse pointer is locked.
  bool IsPointerLocked() const;

  // Returns true if the mouse pointer was locked and no notification should be
  // displayed to the user. This is the case when a notice has already been
  // displayed to the user, and the application voluntarily unlocks, then
  // re-locks the pointer (a duplicate notification should not be given). See
  // content::PointerLockDispatcher::LockPointer.
  bool IsPointerLockedSilently() const;

  void RequestToLockPointer(content::WebContents* web_contents,
                            bool user_gesture,
                            bool last_unlocked_by_target);

  // Returns true if we are waiting for the user to make a selection on the
  // pointer lock permission request dialog.
  bool IsWaitingForPointerLockPrompt(content::WebContents* web_contents);

  // Override from ExclusiveAccessControllerBase
  bool HandleUserPressedEscape() override;
  void HandleUserHeldEscape() override;
  void HandleUserReleasedEscapeEarly() override;
  bool RequiresPressAndHoldEscToExit() const override;
  void ExitExclusiveAccessToPreviousState() override;

  void UnlockPointer();

  void set_bubble_hide_callback_for_test(
      ExclusiveAccessBubbleHideCallbackForTest callback) {
    bubble_hide_callback_for_test_ = std::move(callback);
  }

  void set_lock_state_callback_for_test(base::OnceClosure callback) {
    lock_state_callback_for_test_ = std::move(callback);
  }

 private:
  friend class ExclusiveAccessTest;

  enum PointerLockState {
    POINTERLOCK_UNLOCKED,
    // Pointer has been locked.
    POINTERLOCK_LOCKED,
    // Pointer has been locked silently, with no notification to user.
    POINTERLOCK_LOCKED_SILENTLY
  };

  void LockPointer(base::WeakPtr<content::WebContents> web_contents,
                   content::GlobalRenderFrameHostId rfh_id,
                   bool last_unlocked_by_target);
  void RejectRequestToLockPointer(
      base::WeakPtr<content::WebContents> web_contents,
      content::GlobalRenderFrameHostId rfh_id);

  void ExitExclusiveAccessIfNecessary() override;
  void NotifyTabExclusiveAccessLost() override;

  void OnBubbleHidden(content::WebContents*, ExclusiveAccessBubbleHideReason);

  bool ShouldSuppressBubbleReshowForStateChange();

  // Returns true if the RenderFrameHost identified by `rfh_id` is waiting
  // for the user to make a selection on the pointer lock prompt.
  bool IsWaitingForPointerLockPromptHelper(
      content::GlobalRenderFrameHostId rfh_id);

  PointerLockState pointer_lock_state_;

  // Optionally a WebContents instance that is granted permission to silently
  // lock the mouse pointer. This is granted only if the WebContents instance
  // has previously locked and displayed the permission bubble until the bubble
  // time out has expired. https://crbug.com/725370
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged>
      web_contents_granted_silent_pointer_lock_permission_ = nullptr;

  // If true, does not call into the WebContents to lock the mouse pointer. Just
  // assumes that it works. This may be necessary when calling
  // Browser::RequestToLockPointer in tests, because the proper signal will not
  // have been passed to the RenderViewHost.
  bool fake_pointer_lock_for_test_;

  // If set, |bubble_hide_callback_for_test_| will be called during
  // |OnBubbleHidden()|.
  ExclusiveAccessBubbleHideCallbackForTest bubble_hide_callback_for_test_;

  // Called when the page requests (successfully or not) or loses pointer lock.
  base::OnceClosure lock_state_callback_for_test_;

  // Timestamp when the user last successfully escaped from a lock request.
  base::TimeTicks last_user_escape_time_;

  // Set of RenderFrameHost IDs waiting for pointer lock permission prompt
  // selection by the user.
  std::set<content::GlobalRenderFrameHostId>
      hosts_waiting_for_pointer_lock_permission_prompt_;

  base::WeakPtrFactory<PointerLockController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_POINTER_LOCK_CONTROLLER_H_
