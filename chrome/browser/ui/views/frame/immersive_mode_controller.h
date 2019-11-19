// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"

class BrowserView;

namespace gfx {
class Rect;
class Size;
}

namespace views {
class Widget;
}

// A lock which will keep the top-of-window views revealed for its
// lifetime.
// See ImmersiveModeController::GetRevealedLock for details.
class ImmersiveRevealedLock {
 public:
  virtual ~ImmersiveRevealedLock() {}

 protected:
  ImmersiveRevealedLock() {}
};

// Controller for an "immersive mode" similar to MacOS presentation mode where
// the top-of-window views are hidden until the mouse hits the top of the
// screen. The tab strip is optionally painted with miniature "tab indicator"
// rectangles.
// Currently, immersive mode is only available for Chrome OS.
class ImmersiveModeController {
 public:
  enum AnimateReveal {
    ANIMATE_REVEAL_YES,
    ANIMATE_REVEAL_NO
  };

  class Observer {
   public:
    // Called when a reveal of the top-of-window views has been initiated.
    virtual void OnImmersiveRevealStarted() {}

    // Called when a reveal of the top-of-window views has finished.
    virtual void OnImmersiveRevealEnded() {}

    // Called when the immersive mode controller has been destroyed.
    virtual void OnImmersiveModeControllerDestroyed() {}

    // Called when immersive mode is exited.
    virtual void OnImmersiveFullscreenExited() {}

   protected:
    virtual ~Observer() {}
  };

  ImmersiveModeController();
  virtual ~ImmersiveModeController();

  // Must initialize after browser view has a Widget and native window.
  virtual void Init(BrowserView* browser_view) = 0;

  // Enables or disables immersive mode.
  virtual void SetEnabled(bool enabled) = 0;
  virtual bool IsEnabled() const = 0;

  // True when the top views are hidden due to immersive mode.
  virtual bool ShouldHideTopViews() const = 0;

  // True when the top views are fully or partially visible.
  virtual bool IsRevealed() const = 0;

  // Returns the top container's vertical offset relative to its parent. When
  // revealing or closing the top-of-window views, part of the top container is
  // offscreen.
  // This method takes in the top container's size because it is called as part
  // of computing the new bounds for the top container in
  // BrowserViewLayout::UpdateTopContainerBounds().
  virtual int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const = 0;

  // Returns a lock which will keep the top-of-window views revealed for its
  // lifetime. Several locks can be obtained. When all of the locks are
  // destroyed, if immersive mode is enabled and there is nothing else keeping
  // the top-of-window views revealed, the top-of-window views will be closed.
  // This method always returns a valid lock regardless of whether immersive
  // mode is enabled. The lock's lifetime can span immersive mode being
  // enabled / disabled.
  // If acquiring the lock causes a reveal, the top-of-window views will animate
  // according to |animate_reveal|.
  // The caller takes ownership of the returned lock.
  // This is currently only supported on Ash.
  virtual ImmersiveRevealedLock* GetRevealedLock(
      AnimateReveal animate_reveal) WARN_UNUSED_RESULT = 0;

  // Called by the find bar to indicate that its visible bounds have changed.
  // |new_visible_bounds_in_screen| should be empty if the find bar is not
  // visible.
  virtual void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) = 0;

  // Returns true if we should stay in immersive mode after exiting fullscreen.
  // This should be true unless we are leaving fullscreen while in tablet mode,
  // in which case we should stay in immersive mode.
  virtual bool ShouldStayImmersiveAfterExitingFullscreen() = 0;

  // Called by browser view to indicate the widget activation has changed.
  // Immersive mode should be enabled/disabled if the widget is
  // active/nonactive when the auto hide title bars in tablet mode feature is
  // on.
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) = 0;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 protected:
  base::ObserverList<Observer>::Unchecked observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeController);
};

namespace chrome {

// Implemented in immersive_mode_controller_factory.cc.
std::unique_ptr<ImmersiveModeController> CreateImmersiveModeController();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_H_
