// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_

#include <utility>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"
#include "components/content_settings/core/common/content_settings.h"

// This class implements mouselock behavior.
class MouseLockController : public ExclusiveAccessControllerBase {
 public:
  explicit MouseLockController(ExclusiveAccessManager* manager);

  MouseLockController(const MouseLockController&) = delete;
  MouseLockController& operator=(const MouseLockController&) = delete;

  ~MouseLockController() override;

  // Returns true if the mouse is locked.
  bool IsMouseLocked() const;

  // Returns true if the mouse was locked and no notification should be
  // displayed to the user. This is the case when a notice has already been
  // displayed to the user, and the application voluntarily unlocks, then
  // re-locks the mouse (a duplicate notification should not be given). See
  // content::MouseLockDispatcher::LockMouse.
  bool IsMouseLockedSilently() const;

  void RequestToLockMouse(content::WebContents* web_contents,
                          bool user_gesture,
                          bool last_unlocked_by_target);

  // Override from ExclusiveAccessControllerBase
  bool HandleUserPressedEscape() override;

  void ExitExclusiveAccessToPreviousState() override;

  // Called by Browser::LostMouseLock.
  void LostMouseLock();

  void UnlockMouse();

  void set_lock_state_callback_for_test(base::OnceClosure callback) {
    lock_state_callback_for_test_ = std::move(callback);
  }

 private:
  friend class ExclusiveAccessTest;

  enum MouseLockState {
    MOUSELOCK_UNLOCKED,
    // Mouse has been locked.
    MOUSELOCK_LOCKED,
    // Mouse has been locked silently, with no notification to user.
    MOUSELOCK_LOCKED_SILENTLY
  };

  void ExitExclusiveAccessIfNecessary() override;
  void NotifyTabExclusiveAccessLost() override;
  void RecordBubbleReshowsHistogram(int bubble_reshow_count) override;

  void OnBubbleHidden(content::WebContents*, ExclusiveAccessBubbleHideReason);

  bool ShouldSuppressBubbleReshowForStateChange();

  MouseLockState mouse_lock_state_;

  // Optionally a WebContents instance that is granted permission to silently
  // lock the mouse. This is granted only if the WebContents instance has
  // previously locked and displayed the permission bubble until the bubble
  // time out has expired. https://crbug.com/725370
  raw_ptr<content::WebContents, DanglingUntriaged>
      web_contents_granted_silent_mouse_lock_permission_ = nullptr;

  // If true, does not call into the WebContents to lock the mouse. Just assumes
  // that it works. This may be necessary when calling
  // Browser::RequestToLockMouse in tests, because the proper signal will not
  // have been passed to the RenderViewHost.
  bool fake_mouse_lock_for_test_;

  // If set, |bubble_hide_callback_for_test_| will be called during
  // |OnBubbleHidden()|.
  ExclusiveAccessBubbleHideCallbackForTest bubble_hide_callback_for_test_;

  // Called when the page requests (successfully or not) or loses mouse lock.
  base::OnceClosure lock_state_callback_for_test_;

  // Timestamp when the user last successfully escaped from a lock request.
  base::TimeTicks last_user_escape_time_;

  base::WeakPtrFactory<MouseLockController> weak_ptr_factory_{this};
};

#endif  //  CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_MOUSE_LOCK_CONTROLLER_H_
