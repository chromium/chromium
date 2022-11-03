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
  void UpdateToolbarVisibility(bool always_show);

  // Reveal top chrome leaving it visible until all outstanding calls to
  // RevealLock() are balanced with RevealUnlock().
  void RevealLock();
  void RevealUnlock();

  int revealed_lock_count() { return revealed_lock_count_; }

 private:
  // Pin or unpin the titlebar.
  void SetTitlebarPinned(bool pinned);

  // Start observing child windows of overlay_widget_.
  void ObserveOverlayChildWindows();

  bool enabled_ = false;

  NSWindow* const browser_widget_;
  NSWindow* const overlay_widget_;

  base::scoped_nsobject<ImmersiveModeTitlebarViewController>
      immersive_mode_titlebar_view_controller_;
  base::scoped_nsobject<ClearTitlebarViewController>
      clear_titlebar_view_controller_;
  base::scoped_nsobject<ImmersiveModeMapper> immersive_mode_mapper_;
  base::scoped_nsobject<ImmersiveModeWindowObserver>
      immersive_mode_window_observer_;

  int revealed_lock_count_ = 0;
  bool always_show_toolbar_ = false;

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
REMOTE_COCOA_APP_SHIM_EXPORT @interface ImmersiveModeTitlebarObserver
    : NSObject {
  NSWindow* _overlay_window;
  NSView* _overlay_view;
}
- (instancetype)initWithOverlayWindow:(NSWindow*)overlay_window
                          overlayView:(NSView*)overlay_view;
@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_CONTROLLER_H_
