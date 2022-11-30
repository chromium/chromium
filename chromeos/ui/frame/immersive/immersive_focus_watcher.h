// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_FOCUS_WATCHER_H_
#define CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_FOCUS_WATCHER_H_

#include "base/memory/raw_ptr.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/wm/public/activation_change_observer.h"

namespace chromeos {
class ImmersiveFullscreenController;
class ImmersiveRevealedLock;

// ImmersiveFocusWatcher is responsible for grabbing a reveal lock based on
// activation and/or focus. This implementation grabs a lock if views focus is
// in the top view, a bubble is showing that is anchored to the top view, or
// the focused window is a transient child of the top view's widget.
class ImmersiveFocusWatcher
    : public views::FocusChangeListener,
      public aura::client::TransientWindowClientObserver,
      public ::wm::ActivationChangeObserver {
 public:
  explicit ImmersiveFocusWatcher(ImmersiveFullscreenController* controller);

  ImmersiveFocusWatcher(const ImmersiveFocusWatcher&) = delete;
  ImmersiveFocusWatcher& operator=(const ImmersiveFocusWatcher&) = delete;

  ~ImmersiveFocusWatcher() override;

  // Forces updating the status of the lock. That is, this determines whether
  // a lock should be held and updates accordingly. The lock is automatically
  // maintained, but this function may be called to force an update.
  void UpdateFocusRevealedLock();

  // Explicitly releases the lock, does nothing if a lock is not held.
  void ReleaseLock();

 private:
  class BubbleObserver;

  views::Widget* GetWidget();
  aura::Window* GetWidgetWindow();

  // Recreate |bubble_observer_| and start observing any bubbles anchored to a
  // child of |top_container_|.
  void RecreateBubbleObserver();

  // views::FocusChangeListener overrides:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // aura::client::TransientWindowClientObserver overrides:
  void OnTransientChildWindowAdded(aura::Window* window,
                                   aura::Window* transient) override;
  void OnTransientChildWindowRemoved(aura::Window* window,
                                     aura::Window* transient) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gaining_active,
      aura::Window* losing_active) override;

  raw_ptr<ImmersiveFullscreenController> immersive_fullscreen_controller_;

  // Lock which keeps the top-of-window views revealed based on the focused view
  // and the active widget. Acquiring the lock never triggers a reveal because
  // a view is not focusable till a reveal has made it visible.
  std::unique_ptr<ImmersiveRevealedLock> lock_;

  // Manages bubbles which are anchored to a child of
  // |ImmersiveFullscreenController::top_container_|.
  std::unique_ptr<BubbleObserver> bubble_observer_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_IMMERSIVE_IMMERSIVE_FOCUS_WATCHER_H_
