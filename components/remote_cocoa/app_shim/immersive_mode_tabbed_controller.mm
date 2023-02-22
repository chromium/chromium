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
  if (@available(macOS 11.0, *)) {
    // TODO(https://crbug.com/1414521): Support macOS versions older than
    // macOS 11.
    browser_window.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
  }
  browser_window.titleVisibility = NSWindowTitleHidden;
  browser_window.toolbar = [[[NSToolbar alloc] init] autorelease];

  tab_titlebar_view_controller_.reset([[TabTitlebarViewController alloc] init]);
  tab_titlebar_view_controller_.get().view =
      [[[NSView alloc] init] autorelease];
  tab_titlebar_view_controller_.get().layoutAttribute =
      (browser_window.windowTitlebarLayoutDirection ==
       NSUserInterfaceLayoutDirectionRightToLeft)
          ? NSLayoutAttributeRight
          : NSLayoutAttributeLeft;
}

ImmersiveModeTabbedController::~ImmersiveModeTabbedController() {
  NSWindow* browser_window = ImmersiveModeController::browser_window();
  browser_window.toolbar = nil;
  BridgedContentView* tab_content_view =
      base::mac::ObjCCastStrict<BridgedContentView>(
          tab_titlebar_view_controller_.get().view.subviews.firstObject);
  [tab_content_view retain];
  [tab_content_view removeFromSuperview];
  tab_window_.contentView = tab_content_view;
  [tab_content_view release];
  [tab_titlebar_view_controller_ removeFromParentViewController];
  tab_titlebar_view_controller_.reset();
}

void ImmersiveModeTabbedController::Enable() {
  ImmersiveModeController::Enable();
  BridgedContentView* tab_content_view =
      base::mac::ObjCCastStrict<BridgedContentView>(tab_window_.contentView);
  [tab_content_view retain];
  [tab_content_view removeFromSuperview];
  [tab_titlebar_view_controller_.get().view addSubview:tab_content_view];
  [tab_content_view release];
  [tab_titlebar_view_controller_.get().view
      setFrameSize:tab_window_.frame.size];
  tab_titlebar_view_controller_.get().fullScreenMinHeight =
      tab_window_.frame.size.height;

  tab_window_.contentView =
      [[[BridgedContentView alloc] initWithBridge:tab_content_view.bridge
                                           bounds:gfx::Rect()] autorelease];
  [browser_window()
      addTitlebarAccessoryViewController:tab_titlebar_view_controller_];
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
      browser_window().toolbar = nil;
      break;
  }
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
  if (@available(macOS 10.15, *)) {
    browser_window().titlebarHeight = tab_window_.frame.size.height - 1;
  }
  browser_window().toolbar = [[[NSToolbar alloc] init] autorelease];
  if (@available(macOS 10.15, *)) {
    browser_window().titlebarHeight = tab_window_.frame.size.height;
  }
}

void ImmersiveModeTabbedController::TitlebarHide() {
  // Similarly this -1 hack will cause the titlebar to hide.
  // TODO(https://crbug.com/1414521): Get rid of this shrink hack.
  if (@available(macOS 10.15, *)) {
    browser_window().titlebarHeight = tab_window_.frame.size.height - 1;
  }
  browser_window().toolbar = nil;
  if (@available(macOS 10.15, *)) {
    browser_window().titlebarHeight = tab_window_.frame.size.height;
  }
}

// TODO(https://crbug.com/1414521) TitlebarLock and TitlebarUnlock mean
// something entirely different in ImmersiveModeController. Override them here
// and do nothing for now. In order to handle non-focusing child widgets
// TitlebarLock and TitlebarUnlock will need to be handled.
void ImmersiveModeTabbedController::TitlebarLock() {}

void ImmersiveModeTabbedController::TitlebarUnlock() {}

}  // namespace remote_cocoa
