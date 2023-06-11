// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller.h"

#include "base/functional/callback_forward.h"
#include "base/mac/foundation_util.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace remote_cocoa {

struct ImmersiveModeTabbedController::ObjCStorage {
  NSWindow* __weak tab_window;
  BridgedContentView* __weak tab_content_view;
  NSTitlebarAccessoryViewController* __strong tab_titlebar_view_controller;
};

ImmersiveModeTabbedController::ImmersiveModeTabbedController(
    NSWindow* browser_window,
    NSWindow* overlay_window,
    NSWindow* tab_window)
    : ImmersiveModeController(browser_window, overlay_window),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->tab_window = tab_window;

  browser_window.titleVisibility = NSWindowTitleHidden;

  objc_storage_->tab_titlebar_view_controller =
      [[NSTitlebarAccessoryViewController alloc] init];
  objc_storage_->tab_titlebar_view_controller.view = [[NSView alloc] init];

  // The view is pinned to the opposite side of the traffic lights. A view long
  // enough is able to paint underneath the traffic lights. This also works with
  // RTL setups.
  objc_storage_->tab_titlebar_view_controller.layoutAttribute =
      NSLayoutAttributeTrailing;
}

ImmersiveModeTabbedController::~ImmersiveModeTabbedController() {
  StopObservingChildWindows(objc_storage_->tab_window);
  browser_window().toolbar = nil;
  BridgedContentView* tab_content_view = objc_storage_->tab_content_view;
  [tab_content_view removeFromSuperview];
  objc_storage_->tab_window.contentView = tab_content_view;
  [objc_storage_->tab_titlebar_view_controller removeFromParentViewController];
  objc_storage_->tab_titlebar_view_controller = nil;
}

void ImmersiveModeTabbedController::Enable() {
  ImmersiveModeController::Enable();
  BridgedContentView* tab_content_view =
      base::mac::ObjCCastStrict<BridgedContentView>(
          objc_storage_->tab_window.contentView);
  [tab_content_view removeFromSuperview];
  objc_storage_->tab_content_view = tab_content_view;

  // The ordering of resetting the `contentView` is important for macOS 12 and
  // below. `objc_storage_->tab_content_view` needs to be removed from the
  // `objc_storage_->tab_window.contentView` property before adding
  // `objc_storage_->tab_content_view` to a new NSView tree. We will be left
  // with a blank view if this ordering is not maintained.
  objc_storage_->tab_window.contentView = [[BridgedContentView alloc]
      initWithBridge:objc_storage_->tab_content_view.bridge
              bounds:gfx::Rect()];

  // This will allow the NSToolbarFullScreenWindow to become key when
  // interacting with the tab strip.
  // The `overlay_window_` is handled the same way in ImmersiveModeController.
  // See the comment there for more details.
  objc_storage_->tab_window.ignoresMouseEvents = YES;

  [objc_storage_->tab_titlebar_view_controller.view
      addSubview:tab_content_view];
  [objc_storage_->tab_titlebar_view_controller.view
      setFrameSize:objc_storage_->tab_window.frame.size];
  objc_storage_->tab_titlebar_view_controller.fullScreenMinHeight =
      objc_storage_->tab_window.frame.size.height;

  // Keep the tab content view's size in sync with its parent view.
  objc_storage_->tab_content_view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [objc_storage_->tab_content_view.heightAnchor
      constraintEqualToAnchor:objc_storage_->tab_content_view.superview
                                  .heightAnchor]
      .active = YES;
  [objc_storage_->tab_content_view.widthAnchor
      constraintEqualToAnchor:objc_storage_->tab_content_view.superview
                                  .widthAnchor]
      .active = YES;
  [objc_storage_->tab_content_view.centerXAnchor
      constraintEqualToAnchor:objc_storage_->tab_content_view.superview
                                  .centerXAnchor]
      .active = YES;
  [objc_storage_->tab_content_view.centerYAnchor
      constraintEqualToAnchor:objc_storage_->tab_content_view.superview
                                  .centerYAnchor]
      .active = YES;

  ObserveChildWindows(objc_storage_->tab_window);

  // The presence of a visible NSToolbar causes the titlebar to be revealed.
  NSToolbar* toolbar = [[NSToolbar alloc] init];

  // Remove the baseline separator for macOS 10.15 and earlier. This has no
  // effect on macOS 11 and above. See
  // `-[ImmersiveModeTitlebarViewController separatorView]` for removing the
  // separator on macOS 11+.
  toolbar.showsBaselineSeparator = NO;

  browser_window().toolbar = toolbar;

  // `UpdateToolbarVisibility()` will make the toolbar visible as necessary.
  UpdateToolbarVisibility(last_used_style());
}

void ImmersiveModeTabbedController::UpdateToolbarVisibility(
    mojom::ToolbarVisibilityStyle style) {
  // Don't make changes when a reveal lock is active. Do update the
  // `last_used_style` so the style will be updated once all outstanding reveal
  // locks are released.
  if (reveal_lock_count() > 0) {
    set_last_used_style(style);
    return;
  }

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

  // During fullscreen restore or split screen restore tab window can be left
  // without a parent, leading to the window being hidden which causes
  // compositing to stop. This call ensures that tab window is parented to
  // overlay window and is in the correct z-order.
  OrderTabWindowZOrderOnTop();

  // macOS 10.15 does not call `OnTitlebarFrameDidChange` as often as newer
  // versions of macOS. Add a layout call here and in `RevealLock` and
  // `RevealUnlock` to pickup the slack. There is no harm in extra layout calls
  // on newer versions of macOS, -setFrameOrigin: is essentially a NOP when the
  // frame size doesn't change.
  LayoutWindowWithAnchorView(objc_storage_->tab_window,
                             objc_storage_->tab_content_view);
}

void ImmersiveModeTabbedController::AddController() {
  NSWindow* window = browser_window();
  if (![window.titlebarAccessoryViewControllers
          containsObject:objc_storage_->tab_titlebar_view_controller]) {
    [window addTitlebarAccessoryViewController:objc_storage_->
                                               tab_titlebar_view_controller];
  }
}

void ImmersiveModeTabbedController::RemoveController() {
  [objc_storage_->tab_titlebar_view_controller removeFromParentViewController];
}

void ImmersiveModeTabbedController::OnTopViewBoundsChanged(
    const gfx::Rect& bounds) {
  ImmersiveModeController::OnTopViewBoundsChanged(bounds);
  NSRect frame = NSRectFromCGRect(bounds.ToCGRect());
  [objc_storage_->tab_titlebar_view_controller.view
      setFrameSize:NSMakeSize(frame.size.width,
                              objc_storage_->tab_titlebar_view_controller.view
                                  .frame.size.height)];
}

void ImmersiveModeTabbedController::RevealLock() {
  TitlebarReveal();

  // Call after TitlebarReveal() for a proper layout.
  ImmersiveModeController::RevealLock();
  LayoutWindowWithAnchorView(objc_storage_->tab_window,
                             objc_storage_->tab_content_view);
}

void ImmersiveModeTabbedController::RevealUnlock() {
  // The reveal lock count will be updated in
  // ImmersiveModeController::RevealUnlock(), count 1 or less here as unlocked.
  if (reveal_lock_count() < 2 &&
      last_used_style() == mojom::ToolbarVisibilityStyle::kAutohide) {
    TitlebarHide();
  }

  // Call after TitlebarHide() for a proper layout.
  ImmersiveModeController::RevealUnlock();
  LayoutWindowWithAnchorView(objc_storage_->tab_window,
                             objc_storage_->tab_content_view);
}

void ImmersiveModeTabbedController::TitlebarReveal() {
  browser_window().toolbar.visible = YES;
}

void ImmersiveModeTabbedController::TitlebarHide() {
  browser_window().toolbar.visible = NO;
}

void ImmersiveModeTabbedController::OnTitlebarFrameDidChange(NSRect frame) {
  ImmersiveModeController::OnTitlebarFrameDidChange(frame);
  LayoutWindowWithAnchorView(objc_storage_->tab_window,
                             objc_storage_->tab_content_view);
}

void ImmersiveModeTabbedController::OnChildWindowAdded(NSWindow* child) {
  // The `objc_storage_->tab_window` is a child of the `overlay_window_`. Ignore
  // the `objc_storage_->tab_window`.
  if (child == objc_storage_->tab_window) {
    return;
  }

  OrderTabWindowZOrderOnTop();
  ImmersiveModeController::OnChildWindowAdded(child);
}

void ImmersiveModeTabbedController::OnChildWindowRemoved(NSWindow* child) {
  // The `objc_storage_->tab_window` is a child of the `overlay_window_`. Ignore
  // the `objc_storage_->tab_window`.
  if (child == objc_storage_->tab_window) {
    return;
  }
  ImmersiveModeController::OnChildWindowRemoved(child);
}

bool ImmersiveModeTabbedController::ShouldObserveChildWindow(NSWindow* child) {
  // Filter out the `objc_storage_->tab_window`.
  if (child == objc_storage_->tab_window) {
    return false;
  }
  return ImmersiveModeController::ShouldObserveChildWindow(child);
}

bool ImmersiveModeTabbedController::IsTabbed() {
  return true;
}

void ImmersiveModeTabbedController::OrderTabWindowZOrderOnTop() {
  // Keep the tab window on top of its siblings. This will allow children of tab
  // window to always be z-order on top of overlay window children.
  // Practically this allows for the tab preview hover card to be z-order on top
  // of omnibox results popup.
  if (overlay_window().childWindows.lastObject != objc_storage_->tab_window) {
    [overlay_window() removeChildWindow:objc_storage_->tab_window];
    [overlay_window() addChildWindow:objc_storage_->tab_window
                             ordered:NSWindowAbove];
  }
}

}  // namespace remote_cocoa
