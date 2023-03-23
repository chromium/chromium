// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller.h"

#include "base/functional/callback_forward.h"
#include "base/mac/foundation_util.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

namespace remote_cocoa {

ImmersiveModeTabbedController::ImmersiveModeTabbedController(
    NSWindow* browser_window,
    NSWindow* overlay_window,
    NSWindow* tab_window,
    base::OnceClosure callback)
    : ImmersiveModeController(browser_window,
                              overlay_window,
                              std::move(callback)),
      tab_window_(tab_window) {
  browser_window.titleVisibility = NSWindowTitleHidden;

  tab_titlebar_view_controller_.reset(
      [[NSTitlebarAccessoryViewController alloc] init]);
  tab_titlebar_view_controller_.get().view =
      [[[NSView alloc] init] autorelease];

  // The view is pinned to the opposite side of the traffic lights. A view long
  // enough is able to paint underneath the traffic lights. This also works with
  // RTL setups.
  tab_titlebar_view_controller_.get().layoutAttribute =
      NSLayoutAttributeTrailing;
}

ImmersiveModeTabbedController::~ImmersiveModeTabbedController() {
  NSWindow* browser_window = ImmersiveModeController::browser_window();
  browser_window.toolbar = nil;
  [tab_content_view_ retain];
  [tab_content_view_ removeFromSuperview];
  tab_window_.contentView = tab_content_view_;
  [tab_content_view_ release];
  [tab_titlebar_view_controller_ removeFromParentViewController];
  tab_titlebar_view_controller_.reset();
}

void ImmersiveModeTabbedController::Enable() {
  ImmersiveModeController::Enable();
  tab_content_view_ =
      base::mac::ObjCCastStrict<BridgedContentView>(tab_window_.contentView);
  [tab_content_view_ retain];
  [tab_content_view_ removeFromSuperview];
  [tab_titlebar_view_controller_.get().view addSubview:tab_content_view_];
  [tab_content_view_ release];
  [tab_titlebar_view_controller_.get().view
      setFrameSize:tab_window_.frame.size];
  tab_titlebar_view_controller_.get().fullScreenMinHeight =
      tab_window_.frame.size.height;

  // Keep the tab content view's size in sync with its parent view.
  tab_content_view_.translatesAutoresizingMaskIntoConstraints = NO;
  [tab_content_view_.heightAnchor
      constraintEqualToAnchor:tab_content_view_.superview.heightAnchor]
      .active = YES;
  [tab_content_view_.widthAnchor
      constraintEqualToAnchor:tab_content_view_.superview.widthAnchor]
      .active = YES;

  tab_window_.contentView =
      [[[BridgedContentView alloc] initWithBridge:tab_content_view_.bridge
                                           bounds:gfx::Rect()] autorelease];

  // This will allow the NSToolbarFullScreenWindow to become key when
  // interacting with the tab strip.
  // The `overlay_window_` is handled the same way in ImmersiveModeController.
  // See the comment there for more details.
  tab_window_.ignoresMouseEvents = YES;
}

void ImmersiveModeTabbedController::FullscreenTransitionCompleted() {
  // The presence of a visible NSToolbar causes the titlebar to be revealed.
  // Keep the titlebar hidden until the fullscreen transition is complete.
  ImmersiveModeController::FullscreenTransitionCompleted();
  NSToolbar* toolbar = [[[NSToolbar alloc] init] autorelease];
  toolbar.visible = NO;
  ImmersiveModeController::browser_window().toolbar = toolbar;

  // `UpdateToolbarVisibility()` will make the toolbar visible as necessary.
  UpdateToolbarVisibility(ImmersiveModeController::last_used_style());
}

void ImmersiveModeTabbedController::UpdateToolbarVisibility(
    mojom::ToolbarVisibilityStyle style) {
  // TODO(https://crbug.com/1426944): A NSTitlebarAccessoryViewController hosted
  // in the titlebar, as opposed to above or below it, does not hide/show when
  // using the `hidden` property. Instead we must entirely remove the view
  // controller to make the view hide. Switch to using the `hidden` property
  // once Apple resolves this bug.
  switch (style) {
    case mojom::ToolbarVisibilityStyle::kAlways:
      AddController();
      TitlebarReveal();
      break;
    case mojom::ToolbarVisibilityStyle::kAutohide:
      AddController();
      TitlebarHide();
      break;
    case mojom::ToolbarVisibilityStyle::kNone:
      RemoveController();
      TitlebarHide();
      break;
  }
  ImmersiveModeController::UpdateToolbarVisibility(style);
}

void ImmersiveModeTabbedController::AddController() {
  NSWindow* browser_window = ImmersiveModeController::browser_window();
  if (![browser_window.titlebarAccessoryViewControllers
          containsObject:tab_titlebar_view_controller_]) {
    [browser_window
        addTitlebarAccessoryViewController:tab_titlebar_view_controller_];
  }
}

void ImmersiveModeTabbedController::RemoveController() {
  [tab_titlebar_view_controller_ removeFromParentViewController];
}

void ImmersiveModeTabbedController::OnTopViewBoundsChanged(
    const gfx::Rect& bounds) {
  ImmersiveModeController::OnTopViewBoundsChanged(bounds);
  NSRect frame = NSRectFromCGRect(bounds.ToCGRect());
  [tab_titlebar_view_controller_.get().view
      setFrameSize:NSMakeSize(frame.size.width,
                              tab_titlebar_view_controller_.get()
                                  .view.frame.size.height)];
}

void ImmersiveModeTabbedController::RevealLock() {
  TitlebarReveal();

  // Call after TitlebarReveal() for a proper layout.
  ImmersiveModeController::RevealLock();
}

void ImmersiveModeTabbedController::RevealUnlock() {
  // The reveal lock count will be updated in
  // ImmersiveModeController::RevealUnlock(), count 1 or less here as unlocked.
  if (ImmersiveModeController::reveal_lock_count() < 2 &&
      ImmersiveModeController::last_used_style() ==
          mojom::ToolbarVisibilityStyle::kAutohide) {
    TitlebarHide();
  }

  // Call after TitlebarHide() for a proper layout.
  ImmersiveModeController::RevealUnlock();
}

void ImmersiveModeTabbedController::TitlebarReveal() {
  NSWindow* browser_window = ImmersiveModeController::browser_window();
  browser_window.toolbar.visible = YES;
}

void ImmersiveModeTabbedController::TitlebarHide() {
  NSWindow* browser_window = ImmersiveModeController::browser_window();
  browser_window.toolbar.visible = NO;
}

// TODO(https://crbug.com/1414521) TitlebarLock and TitlebarUnlock mean
// something entirely different in ImmersiveModeController. Override them here
// and do nothing for now. In order to handle non-focusing child widgets
// TitlebarLock and TitlebarUnlock will need to be handled.
void ImmersiveModeTabbedController::TitlebarLock() {}

void ImmersiveModeTabbedController::TitlebarUnlock() {}

void ImmersiveModeTabbedController::OnTitlebarFrameDidChange(NSRect frame) {
  ImmersiveModeController::OnTitlebarFrameDidChange(frame);

  // Find the tab overlay view's point on screen (bottom left).
  NSPoint point_in_window = [tab_content_view_ convertPoint:NSZeroPoint
                                                     toView:nil];
  NSPoint point_on_screen =
      [tab_content_view_.window convertPointToScreen:point_in_window];
  [tab_window_ setFrameOrigin:point_on_screen];
}

void ImmersiveModeTabbedController::OnChildWindowAdded(NSWindow* child) {
  // The `tab_window_` is a child of the `overlay_window_`. Ignore the
  // `tab_window_`.
  if (child == tab_window_) {
    return;
  }
  ImmersiveModeController::OnChildWindowAdded(child);
}

bool ImmersiveModeTabbedController::IsTabbed() {
  return true;
}

}  // namespace remote_cocoa
