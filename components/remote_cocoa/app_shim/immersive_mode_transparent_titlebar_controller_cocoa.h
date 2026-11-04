// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TRANSPARENT_TITLEBAR_CONTROLLER_COCOA_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TRANSPARENT_TITLEBAR_CONTROLLER_COCOA_H_

#import <AppKit/AppKit.h>

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"

@class ImmersiveModeTransparentTitlebarViewController;

namespace remote_cocoa {

// Manages a single immersive fullscreen session in which top chrome (tab
// strip, toolbar, bookmarks bar) stays in the browser window. Used by
// immersive fullscreen v2 (features::kMacImmersiveFullscreenV2).
//
// While a window is in fullscreen, AppKit moves the titlebar (including the
// window controls, a.k.a. traffic lights) into a separate
// NSToolbarFullScreenWindow that slides in and out together with the menu
// bar. This controller:
//  * Makes the NSToolbarFullScreenWindow transparent, so that only the window
//    controls are visible and top chrome, which is drawn in the browser window
//    underneath it, shows through. The transparent parts of the
//    NSToolbarFullScreenWindow are click-through, so no event routing is
//    needed.
//  * Adds an empty, transparent titlebar accessory that makes the
//    NSToolbarFullScreenWindow as tall as top chrome, so that AppKit's reveal
//    animation and hover tracking cover all of top chrome.
//  * Reports the frame of the titlebar container (which AppKit slides and
//    grows during the reveal animation), so that top chrome can follow it
//    frame by frame.
//
// The transparency relies on the private
// -[NSTitlebarContainerView setTransparencyAllowedInFullScreen:] (the backing
// of -[NSWindow setTitlebarAppearsTransparentWhenFullscreen:]). The flag must
// only be set after the fullscreen transition completed: if it is set when
// -[_NSEnterFullScreenTransitionController doAfterEnterFullScreen] runs, AppKit
// detaches the titlebar from the menu bar and reveals an opaque
// _NSFullScreenToolbarSurrogateWindow instead. For the same reason, the flag
// must be reset before the window enters fullscreen again, see
// ResetTitlebarTransparency().
class REMOTE_COCOA_APP_SHIM_EXPORT
    ImmersiveModeTransparentTitlebarControllerCocoa {
 public:
  // Called with the frame of the titlebar container in the coordinates of the
  // browser window's content view (origin at the top-left) whenever it
  // changes. A frame above the top of the content view means that the
  // titlebar is (partially) hidden.
  using TitlebarFrameChangedCallback =
      base::RepeatingCallback<void(const gfx::Rect& frame)>;

  ImmersiveModeTransparentTitlebarControllerCocoa(
      NSWindow* browser_window,
      TitlebarFrameChangedCallback titlebar_frame_changed_callback);
  ImmersiveModeTransparentTitlebarControllerCocoa(
      const ImmersiveModeTransparentTitlebarControllerCocoa&) = delete;
  ImmersiveModeTransparentTitlebarControllerCocoa& operator=(
      const ImmersiveModeTransparentTitlebarControllerCocoa&) = delete;
  ~ImmersiveModeTransparentTitlebarControllerCocoa();

  // Must be called once and only once after construction. Prevents the side-
  // effects of adding a titlebar accessory from accessing a partially
  // constructed object.
  void Init();
  bool is_initialized() const { return initialized_; }

  void FullscreenTransitionCompleted();
  void OnTopViewBoundsChanged(const gfx::Rect& bounds);
  void UpdateToolbarVisibility(mojom::ToolbarVisibilityStyle style);

  // Returns true if the window is in content fullscreen, i.e. top chrome is
  // hidden.
  bool IsContentFullscreen() const;

  // Called when the accessory view is moved to `window`. AppKit moves the
  // titlebar accessories to a fullscreen transition window and finally to the
  // NSToolbarFullScreenWindow while entering fullscreen.
  void OnAccessoryViewMovedToWindow(NSWindow* window);

  // Called whenever the frame of the titlebar container in the
  // NSToolbarFullScreenWindow may have changed.
  void OnTitlebarGeometryChanged();

  // Resets the titlebar transparency of `window`. Must be called before
  // `window` enters fullscreen again. See the class comment.
  static void ResetTitlebarTransparency(NSWindow* window);

  ImmersiveModeTransparentTitlebarViewController*
  accessory_view_controller_for_testing() {
    return accessory_view_controller_;
  }
  NSWindow* toolbar_window_for_testing() { return toolbar_window_; }
  std::optional<gfx::Rect> last_reported_titlebar_frame_for_testing() const {
    return last_reported_titlebar_frame_;
  }

 private:
  // Starts or stops observing the NSToolbarFullScreenWindow and the titlebar
  // container hosted in it.
  void StartTrackingToolbarWindow(NSWindow* toolbar_window);
  void StopTrackingToolbarWindow();

  // Returns the NSTitlebarContainerView of the browser window. In fullscreen,
  // it lives in the NSToolbarFullScreenWindow.
  NSView* GetTitlebarContainer() const;

  // Returns the height of the titlebar, which hosts the window controls.
  CGFloat GetTitlebarHeight() const;

  // Makes the titlebar transparent or opaque depending on the toolbar
  // visibility style. In content fullscreen there is no top chrome to show
  // through, so the regular opaque titlebar is used.
  void UpdateTitlebarTransparency();

  // Makes the accessory as tall as top chrome minus the titlebar.
  void UpdateAccessoryHeight();

  // Runs `titlebar_frame_changed_callback_` if the frame of the titlebar
  // container changed.
  void ReportTitlebarFrame();

  NSWindow* __weak browser_window_;
  NSWindow* __weak toolbar_window_;
  NSView* __weak titlebar_container_;

  TitlebarFrameChangedCallback titlebar_frame_changed_callback_;

  // An empty and transparent titlebar accessory below the titlebar.
  ImmersiveModeTransparentTitlebarViewController* __strong
      accessory_view_controller_;

  // Notification observers registered with NSNotificationCenter.
  NSMutableArray<id<NSObject>>* __strong notification_observers_;

  NSTitlebarSeparatorStyle original_titlebar_separator_style_ =
      NSTitlebarSeparatorStyleAutomatic;

  bool initialized_ = false;
  bool fullscreen_transition_complete_ = false;
  std::optional<mojom::ToolbarVisibilityStyle> style_;
  int top_container_height_ = 0;
  std::optional<gfx::Rect> last_reported_titlebar_frame_;

  base::WeakPtrFactory<ImmersiveModeTransparentTitlebarControllerCocoa>
      weak_ptr_factory_{this};
};

}  // namespace remote_cocoa

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_IMMERSIVE_MODE_TRANSPARENT_TITLEBAR_CONTROLLER_COCOA_H_
