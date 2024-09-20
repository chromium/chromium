// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_COCOA_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_COCOA_H_

#import <AppKit/AppKit.h>

#include <optional>

#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_overlay_nswindow.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom-shared.h"

@class ImmersiveModeMapper;

namespace remote_cocoa {
class ImmersiveModeControllerCocoa;
}  // namespace remote_cocoa

// Host of the overlay view.
@interface ImmersiveModeTitlebarViewController
    : NSTitlebarAccessoryViewController {
  NSView* __strong _blank_separator_view;
  base::WeakPtr<remote_cocoa::ImmersiveModeControllerCocoa>
      _immersive_mode_controller;
}
@end

namespace gfx {
class Rect;
}

namespace remote_cocoa {

// TODO(mek): This should not be exported and used outside of remote_cocoa. So
// figure out how to restructure code so callers outside of remote_cocoa can
// stop existing.
REMOTE_COCOA_APP_SHIM_EXPORT bool IsNSToolbarFullScreenWindow(NSWindow* window);

// Manages a single fullscreen session.
class REMOTE_COCOA_APP_SHIM_EXPORT ImmersiveModeControllerCocoa {
 public:
  explicit ImmersiveModeControllerCocoa(
      NativeWidgetMacNSWindow* browser_window,
      NativeWidgetMacOverlayNSWindow* overlay_window);
  virtual ~ImmersiveModeControllerCocoa();

  // Must be called once and only once after construction. Prevents the side-
  // effects of adding a toolbar accessory from accessing partially constructed
  // objects.
  virtual void Init();
  bool is_initialized() { return initialized_; }

  virtual void FullscreenTransitionCompleted();
  virtual void OnTopViewBoundsChanged(const gfx::Rect& bounds);
  virtual void UpdateToolbarVisibility(
      std::optional<mojom::ToolbarVisibilityStyle> style);

  // Reveal top chrome leaving it visible until all outstanding calls to
  // RevealLock() are balanced with RevealUnlock(). Reveal locks will persist
  // through calls to UpdateToolbarVisibility(). For example, the current
  // ToolbarVisibilityStyle is set to kAlways and RevealLock() has been called.
  // If ToolbarVisibilityStyle is then changed to kAutohide, top
  // chrome will stay on screen until RevealUnlock() is called. At that point
  // top chrome will autohide.
  void RevealLock();
  void RevealUnlock();
  int reveal_lock_count() { return reveal_lock_count_; }

  // When set to true, calls to RevealLock() and RevealUnlock() will have no
  // visual effect. The lock/unlock calls are still counted and will go into
  // effect when this function is called with false.
  void SetIgnoreRevealLocks(bool ignore);

  // Called when a reveal lock/unlock is ready to take effect.
  virtual void RevealLocked();
  virtual void RevealUnlocked();

  // Returns true if the toolbar is visible, either because there are
  // outstanding reveal locks, or the user hovers over the upper border of the
  // screen.
  bool IsToolbarRevealed();

  // Called when the reveal status changes.
  void OnToolbarRevealMaybeChanged();

  // Called when the menu bar reveal status changes.
  void OnMenuBarRevealChanged();

  // Called when a child window is added to the observed windows.
  // `ObserveChildWindows` controls which windows are being observed.
  virtual void OnChildWindowAdded(NSWindow* child);

  // Called when a child window is removed from the observed windows.
  virtual void OnChildWindowRemoved(NSWindow* child);

  // Start observing child windows of `window`.
  void ObserveChildWindows(NSWindow* window);

  // Stop observing child windows of `window`.
  void StopObservingChildWindows(NSWindow* window);

  // Return true if the child window should trigger OnChildWindowAdded and
  // OnChildWindowRemoved events, otherwise return false.
  // Called for browser window children that survive the transition to
  // fullscreen.
  virtual bool ShouldObserveChildWindow(NSWindow* child);

  NSWindow* browser_window();
  NativeWidgetMacOverlayNSWindow* overlay_window();
  BridgedContentView* overlay_content_view();

  // Called when `immersive_mode_titlebar_view_controller_`'s view is moved to
  // a different window.
  void ImmersiveModeViewWillMoveToWindow(NSWindow* window);

  // Returns true if kImmersiveFullscreenTabs is being used.
  virtual bool IsTabbed();
  bool IsContentFullscreen();

  ImmersiveModeTitlebarViewController*
  immersive_mode_titlebar_view_controller_for_testing() {
    return immersive_mode_titlebar_view_controller_;
  }

 protected:
  // Used by derived classes to manually set last_used_style_. Typically this is
  // used while a RevealLock is active, allowing for a style change after the
  // last RevealLock has been released.
  void set_last_used_style(std::optional<mojom::ToolbarVisibilityStyle> style) {
    last_used_style_ = style;
  }
  std::optional<mojom::ToolbarVisibilityStyle> last_used_style() {
    return last_used_style_;
  }

  // Layout the `window` on top of the `anchor_view`. The `window` will occupy
  // the same place on screen as the `anchor_view`, completely occluding the
  // `anchor_view`. The `window` is clear but needs to overlay the `anchor_view`
  // to handle drag events.
  // If the `anchor_view` is offscreen, the `window` will be moved offscreen.
  void LayoutWindowWithAnchorView(NSWindow* window, NSView* anchor_view);

  // Reanchor the overlay window with its anchor view.
  virtual void Reanchor();

 private:
  // Get offscreen y origin. Used for moving overlay windows offscreen.
  double GetOffscreenYOrigin();

  // Notify the browser window that the reveal status changes.
  void NotifyBrowserWindowAboutToolbarRevealChanged();

  // Updates the visibility of the thin controller. The thin controller will
  // only become visible when the toolbar is hidden.
  // TODO(crbug.com/40240734): Remove when fixed by Apple.
  void UpdateThinControllerVisibility();

  // Calls either RevealLocked() or RevealUnlocked() based on the current
  // reveal_lock_count_.
  void ApplyRevealLockState();

  // `UpdateToolbarVisibility(style)` will skip updating if `last_used_style_`
  // and `style` are the same. `ForceToolbarVisibilityUpdate()` forces a toolbar
  // visibility update of `last_used_style_`.
  void ForceToolbarVisibilityUpdate();

  bool initialized_ = false;

  int reveal_lock_count_ = 0;
  bool ignore_reveal_locks_ = false;

  bool is_toolbar_revealed_ = false;

  int menu_bar_height_ = 0;

  std::optional<mojom::ToolbarVisibilityStyle> last_used_style_;
  // Keeps the view controllers hidden until the fullscreen transition is
  // complete.
  bool fullscreen_transition_complete_ = false;

  NativeWidgetMacNSWindow* __weak browser_window_;
  NativeWidgetMacOverlayNSWindow* __weak overlay_window_;
  BridgedContentView* __weak overlay_content_view_;

  // A controller for top chrome.
  ImmersiveModeTitlebarViewController* __strong
      immersive_mode_titlebar_view_controller_;

  // A controller that keeps a small portion (0.5px) of the fullscreen AppKit
  // NSWindow on screen.
  // This controller is used as a workaround for an AppKit bug that displays a
  // black bar when changing a NSTitlebarAccessoryViewController's
  // fullScreenMinHeight from zero to non-zero.
  // TODO(crbug.com/40240734): Remove when fixed by Apple.
  NSTitlebarAccessoryViewController* __strong thin_titlebar_view_controller_;

  ImmersiveModeMapper* __strong immersive_mode_mapper_;

  // Keeps track of which windows have received titlebar and reveal locks.
  std::set<NSWindow*> window_lock_received_;

  base::WeakPtrFactory<ImmersiveModeControllerCocoa> weak_ptr_factory_;
};

}  // namespace remote_cocoa

// An empty NSView that is also opaque.
@interface OpaqueView : NSView
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_COCOA_H_
