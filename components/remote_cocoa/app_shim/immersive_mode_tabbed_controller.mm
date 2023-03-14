// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller.h"

#include "base/functional/callback_forward.h"
#include "base/mac/foundation_util.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

@interface NSWindow (ChromeTitleBarHeight)
// TODO(https://crbug.com/1414521): Support macOS versions older than
// macOS 10.15.
@property double titlebarHeight API_AVAILABLE(macos(10.15));
@end

@interface TabTitlebarViewController : NSTitlebarAccessoryViewController
@end

@implementation TabTitlebarViewController

- (void)viewWillAppear {
  [super viewWillAppear];
  for (NSView* sub_view in self.view.subviews) {
    if ([sub_view isKindOfClass:[BridgedContentView class]]) {
      [sub_view setFrameSize:self.view.frame.size];
    }
  }
}

@end

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

  tab_titlebar_view_controller_.reset([[TabTitlebarViewController alloc] init]);
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

  tab_window_.contentView =
      [[[BridgedContentView alloc] initWithBridge:tab_content_view_.bridge
                                           bounds:gfx::Rect()] autorelease];

  // This will allow the NSToolbarFullScreenWindow to become key when
  // interacting with the tab strip.
  // The `overlay_window_` is handled the same way in ImmersiveModeController.
  // See the comment there for more details.
  tab_window_.ignoresMouseEvents = YES;

  tab_titlebar_view_controller_.get().hidden = YES;
  [browser_window()
      addTitlebarAccessoryViewController:tab_titlebar_view_controller_];
}

void ImmersiveModeTabbedController::FullscreenTransitionCompleted() {
  // The presence of a visible NSToolbar causes the titlebar to be revealed.
  // Keep the titlebar hidden until the fullscreen transition is complete.
  ImmersiveModeController::FullscreenTransitionCompleted();
  tab_titlebar_view_controller_.get().hidden = NO;
  NSToolbar* toolbar = [[[NSToolbar alloc] init] autorelease];
  toolbar.visible = NO;
  ImmersiveModeController::browser_window().toolbar = toolbar;

  // `UpdateToolbarVisibility()` will make the toolbar visible as necessary.
  UpdateToolbarVisibility(ImmersiveModeController::last_used_style());
}

void ImmersiveModeTabbedController::UpdateToolbarVisibility(
    mojom::ToolbarVisibilityStyle style) {
  ImmersiveModeController::UpdateToolbarVisibility(style);
  switch (style) {
    case mojom::ToolbarVisibilityStyle::kAlways:
      TitlebarReveal();
      break;
    case mojom::ToolbarVisibilityStyle::kAutohide:
      TitlebarHide();
      break;
    case mojom::ToolbarVisibilityStyle::kNone:
      TitlebarHide();
      break;
  }
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
  ImmersiveModeController::RevealLock();
  TitlebarReveal();
}

void ImmersiveModeTabbedController::RevealUnlock() {
  ImmersiveModeController::RevealUnlock();
  if (ImmersiveModeController::reveal_lock_count() < 1 &&
      ImmersiveModeController::last_used_style() ==
          mojom::ToolbarVisibilityStyle::kAutohide) {
    TitlebarHide();
  }
}

void ImmersiveModeTabbedController::TitlebarReveal() {
  // This -1 hack is needed to make the titlebar visible if it is hidden.
  // TODO(https://crbug.com/1414521): Get rid of this shrink hack.
  NSWindow* browser_window = ImmersiveModeController::browser_window();
  if (@available(macOS 10.15, *)) {
    browser_window.titlebarHeight = tab_window_.frame.size.height - 1;
  }
  browser_window.toolbar.visible = YES;
  if (@available(macOS 10.15, *)) {
    browser_window.titlebarHeight = tab_window_.frame.size.height;
  }
}

void ImmersiveModeTabbedController::TitlebarHide() {
  // Similarly this -1 hack will cause the titlebar to hide.
  // TODO(https://crbug.com/1414521): Get rid of this shrink hack.
  NSWindow* browser_window = ImmersiveModeController::browser_window();
  if (@available(macOS 10.15, *)) {
    browser_window.titlebarHeight = tab_window_.frame.size.height - 1;
  }
  browser_window.toolbar.visible = NO;
  if (@available(macOS 10.15, *)) {
    browser_window.titlebarHeight = tab_window_.frame.size.height;
  }
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

}  // namespace remote_cocoa
