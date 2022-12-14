// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_H_

#import <AppKit/AppKit.h>

#include "base/callback.h"
#include "base/functional/callback_forward.h"
#include "base/mac/scoped_nsobject.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom-shared.h"

@class ClearTitlebarViewController;
@class ImmersiveModeMapper;
@class ImmersiveModeTitlebarViewController;
@class ImmersiveModeWindowObserver;

namespace gfx {
class Rect;
}

namespace remote_cocoa {

// TODO(mek): This should not be exported and used outside of remote_cocoa. So
// figure out how to restructure code so callers outside of remote_cocoa can
// stop existing.
REMOTE_COCOA_APP_SHIM_EXPORT bool IsNSToolbarFullScreenWindow(NSWindow* window);

class REMOTE_COCOA_APP_SHIM_EXPORT ImmersiveModeController {
 public:
  explicit ImmersiveModeController(NSWindow* browser_widget,
                                   NSWindow* overlay_widget,
                                   base::OnceClosure callback);
  ~ImmersiveModeController();

  void Enable();
  void OnTopViewBoundsChanged(const gfx::Rect& bounds);
  void UpdateToolbarVisibility(mojom::ToolbarVisibilityStyle style);

  // Lock the titlebar in place forcing the attached top chrome to also lock in
  // place. The titlebar will be unlocked once calls to TitlebarLock() are
  // balanced with TitlebarUnlock(). When a lock is present, both the titlebar
  // and the top chrome are visible.
  void TitlebarLock();
  void TitlebarUnlock();
  int titlebar_lock_count() { return titlebar_lock_count_; }

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

  NSWindow* browser_window() { return browser_window_; }
  NSWindow* overlay_window() { return overlay_window_; }

 private:
  // Pin or unpin the titlebar.
  void SetTitlebarPinned(bool pinned);

  // Start observing child windows of overlay_widget_.
  void ObserveOverlayChildWindows();

  // Reparent children of `source` to `target`.
  void ReparentChildWindows(NSWindow* source, NSWindow* target);

  bool enabled_ = false;

  NSWindow* const browser_window_;
  NSWindow* const overlay_window_;

  // A controller for top chrome.
  base::scoped_nsobject<ImmersiveModeTitlebarViewController>
      immersive_mode_titlebar_view_controller_;

  // A "clear" controller for locking the titlebar in place. Unfortunately
  // there is no discovered way to make a controller actually clear. The
  // controller's view is added to a discrete NSWindow controlled by AppKit.
  // Making the view clear will simply make the underling portion of the
  // NSWindow visible. To achieve "clear" this controller immediately hides
  // itself. This has the side effect of still extending the mouse capture area
  // allowing the title bar to stay visible while this controller's view is
  // hidden.
  base::scoped_nsobject<ClearTitlebarViewController>
      clear_titlebar_view_controller_;

  // A controller that keeps a small portion (0.5px) of the fullscreen AppKit
  // NSWindow on screen.
  // This controller is used as a workaround for an AppKit bug that displays a
  // black bar when changing a NSTitlebarAccessoryViewController's
  // fullScreenMinHeight from zero to non-zero.
  // TODO(https://crbug.com/1369643): Remove when fixed by Apple.
  base::scoped_nsobject<NSTitlebarAccessoryViewController>
      thin_titlebar_view_controller_;

  base::scoped_nsobject<ImmersiveModeMapper> immersive_mode_mapper_;
  base::scoped_nsobject<ImmersiveModeWindowObserver>
      immersive_mode_window_observer_;

  int titlebar_lock_count_ = 0;
  int reveal_lock_count_ = 0;

  mojom::ToolbarVisibilityStyle last_used_style_ =
      mojom::ToolbarVisibilityStyle::kAutohide;

  base::WeakPtrFactory<ImmersiveModeController> weak_ptr_factory_;
};

}  // namespace remote_cocoa

// A small class that moves the overlay window along the y axis.
//
// The overlay's content view (top chrome) is not hosted in the overlay window.
// It is moved to the AppKit controlled fullscreen window via the
// NSTitlebarAccessoryViewController API. However the overlay window is still
// important.
//  * It is the parent window for top chrome popups. Moving the overlay window
//  in turn moves the child windows.
//  * Its origin in important for dragging operations.
//
// This class will keep the position of the overlay window in sync with its
// original content (top chrome).
REMOTE_COCOA_APP_SHIM_EXPORT @interface ImmersiveModeTitlebarObserver : NSObject

- (instancetype)initWithController:
                    (base::WeakPtr<remote_cocoa::ImmersiveModeController>)
                        controller
                       overlayView:(NSView*)overlay_view;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_H_
