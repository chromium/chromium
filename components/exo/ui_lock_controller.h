// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_
#define COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_

#include "ash/shell.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/exo/seat_observer.h"
#include "components/exo/wm_helper.h"
#include "ui/base/user_activity/user_activity_observer.h"

namespace exo {

class Pointer;
class Seat;

// Helps users to break out of various kinds of "locks" that a window may hold
// (fullscreen, pointer lock).
//
// In some cases this is achieved by pressing and holding Escape, similar to
// Chromium's Keyboard Lock feature
// (see https://chromestatus.com/feature/5642959835889664). In other cases we
// nudge the user to use Overview.
class UILockController : public SeatObserver,
                         public ash::SessionObserver,
                         public WMHelper::PowerObserver,
                         public ui::UserActivityObserver {
 public:
  // Interface for classes that display notifications based on UI lock states.
  class Notifier : public base::CheckedObserver {
   public:
    // Called when any UI-lock-related notifications must be shown again.
    //
    // If a state that normally shows a notification on entry is currently
    // active, show that notification again. Otherwise, reset any cooldowns
    // so that the notification will show next time.
    virtual void NotifyAgain() = 0;

    virtual void OnUILockControllerDestroying() = 0;
  };

  explicit UILockController(Seat* seat);
  UILockController(const UILockController&) = delete;
  UILockController& operator=(const UILockController&) = delete;
  ~UILockController() override;

  // Overridden from WMHelper::PowerObserver:
  void SuspendDone() override;
  void ScreenBrightnessChanged(double percent) override;
  void LidEventReceived(bool opened) override;

  // Overridden from ash::SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // Overridden from SeatObserver:
  void OnSurfaceFocused(Surface* gained_focus,
                        Surface* lost_focus,
                        bool has_focued_surface) override;
  void OnPointerCaptureEnabled(Pointer* pointer,
                               aura::Window* capture_window) override;
  void OnPointerCaptureDisabled(Pointer* pointer,
                                aura::Window* capture_window) override;

  // Overridden from ui::UserActivityObserver:
  void OnUserActivity(const ui::Event* event) override;

  views::Widget* GetEscNotificationForTesting(aura::Window* window);
  views::Widget* GetPointerCaptureNotificationForTesting(aura::Window* window);

  void AddObserver(Notifier* notifier);
  void RemoveObserver(Notifier* notifier);

 private:
  void ReshowAllNotifications();

  raw_ptr<Seat> seat_;

  // Whether the screen brightness is low enough to make the display dark.
  bool device_in_dark_ = false;

  // Pointers currently being captured.
  base::flat_set<raw_ptr<Pointer>> captured_pointers_;

  base::ObserverList<Notifier> notifiers_;

  // Time of last user-generated input event. Used to display notifications
  // again if the user goes idle and then becomes active again.
  //
  // Note TimeTicks may stand still if the device is suspended, but that's OK
  // because device suspend/resume events also retrigger notifications.
  base::TimeTicks last_activity_time_;
};

}  // namespace exo

#endif  // COMPONENTS_EXO_UI_LOCK_CONTROLLER_H_
