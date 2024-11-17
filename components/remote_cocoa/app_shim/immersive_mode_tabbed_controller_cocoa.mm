// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_tabbed_controller_cocoa.h"

#include "base/apple/foundation_util.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#import "components/remote_cocoa/app_shim/NSToolbar+Private.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/browser_native_widget_window_mac.h"
#include "components/remote_cocoa/app_shim/features.h"

namespace {
void SetAlwaysShowTrafficLights(NSWindow* browser_window, bool always_show) {
  if (base::FeatureList::IsEnabled(
          remote_cocoa::features::kFullscreenAlwaysShowTrafficLights)) {
    [base::apple::ObjCCast<BrowserNativeWidgetWindow>(browser_window)
        setAlwaysShowTrafficLights:always_show ? YES : NO];
  }
}
}  // namespace

namespace remote_cocoa {

ImmersiveModeTabbedControllerCocoa::ImmersiveModeTabbedControllerCocoa(
    NativeWidgetMacNSWindow* browser_window,
    NativeWidgetMacOverlayNSWindow* overlay_window,
    NativeWidgetMacOverlayNSWindow* tab_window)
    : ImmersiveModeControllerCocoa(browser_window, overlay_window) {
  tab_window_ = tab_window;
#ifndef NDEBUG
  tab_window_.title = @"tab overlay";
#endif  // NDEBUG

  browser_window.titleVisibility = NSWindowTitleHidden;

  tab_titlebar_view_controller_ =
      [[NSTitlebarAccessoryViewController alloc] init];
  tab_titlebar_view_controller_.view = [[NSView alloc] init];

  // The view is pinned to the opposite side of the traffic lights. A view long
  // enough is able to paint underneath the traffic lights. This also works with
  // RTL setups.
  tab_titlebar_view_controller_.layoutAttribute = NSLayoutAttributeTrailing;

  // During fullscreen restore or split screen restore tab window can be left
  // without a parent, leading to the window being hidden which causes
  // compositing to stop.
  if (!tab_window.parentWindow) {
    [overlay_window addChildWindow:tab_window ordered:NSWindowAbove];
  }
}

ImmersiveModeTabbedControllerCocoa::~ImmersiveModeTabbedControllerCocoa() {
  SetAlwaysShowTrafficLights(browser_window(), false);
  StopObservingChildWindows(tab_window_);
  browser_window().toolbar = nil;
  BridgedContentView* tab_content_view = tab_content_view_;
  [tab_content_view removeFromSuperview];
  tab_window_.contentView = tab_content_view;
  [tab_titlebar_view_controller_ removeFromParentViewController];
  tab_titlebar_view_controller_ = nil;
}

void ImmersiveModeTabbedControllerCocoa::Init() {
  SetAlwaysShowTrafficLights(browser_window(), true);

  ImmersiveModeControllerCocoa::Init();
  BridgedContentView* tab_content_view =
      base::apple::ObjCCastStrict<BridgedContentView>(tab_window_.contentView);
  [tab_content_view removeFromSuperview];
  tab_content_view_ = tab_content_view;

  // Use a placeholder view since the content has been moved to the
  // NSTitlebarAccessoryViewController.
  tab_window_.contentView = [[OpaqueView alloc] init];
  if (base::FeatureList::IsEnabled(
          remote_cocoa::features::kImmersiveFullscreenOverlayWindowDebug)) {
    [tab_window_ debugWithColor:NSColor.redColor];
  }

  // This will allow the NSToolbarFullScreenWindow to become key when
  // interacting with the tab strip.
  // The `overlay_window_` is handled the same way in ImmersiveModeController.
  // See the comment there for more details.
  tab_window_.ignoresMouseEvents = YES;

  [tab_titlebar_view_controller_.view addSubview:tab_content_view];
  [tab_titlebar_view_controller_.view setFrameSize:tab_window_.frame.size];
  tab_titlebar_view_controller_.fullScreenMinHeight =
      tab_window_.frame.size.height;

  // Keep the tab content view's size in sync with its parent view.
  tab_content_view_.translatesAutoresizingMaskIntoConstraints = NO;
  [tab_content_view_.heightAnchor
      constraintEqualToAnchor:tab_content_view_.superview.heightAnchor]
      .active = YES;
  [tab_content_view_.widthAnchor
      constraintEqualToAnchor:tab_content_view_.superview.widthAnchor]
      .active = YES;
  [tab_content_view_.centerXAnchor
      constraintEqualToAnchor:tab_content_view_.superview.centerXAnchor]
      .active = YES;
  [tab_content_view_.centerYAnchor
      constraintEqualToAnchor:tab_content_view_.superview.centerYAnchor]
      .active = YES;

  ObserveChildWindows(tab_window_);

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

void ImmersiveModeTabbedControllerCocoa::UpdateToolbarVisibility(
    std::optional<mojom::ToolbarVisibilityStyle> style) {
  if (!style.has_value()) {
    return;
  }

  // Don't make changes when a reveal lock is active. Do update the
  // `last_used_style` so the style will be updated once all outstanding reveal
  // locks are released.
  if (reveal_lock_count() > 0) {
    set_last_used_style(style);
    return;
  }

  // TODO(crbug.com/40261565): A NSTitlebarAccessoryViewController hosted
  // in the titlebar, as opposed to above or below it, does not hide/show when
  // using the `hidden` property. Instead we must entirely remove the view
  // controller to make the view hide. Switch to using the `hidden` property
  // once Apple resolves this bug.
  switch (style.value()) {
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
  ImmersiveModeControllerCocoa::UpdateToolbarVisibility(style);

  // Ensures that tab window is in the correct z-order.
  OrderTabWindowZOrderOnTop();

  // macOS 10.15 does not call `OnTitlebarFrameDidChange` as often as newer
  // versions of macOS. Add a layout call here and in `RevealLock` and
  // `RevealUnlock` to pickup the slack. There is no harm in extra layout calls
  // on newer versions of macOS, -setFrameOrigin: is essentially a NOP when the
  // frame size doesn't change.
  LayoutWindowWithAnchorView(tab_window_, tab_content_view_);
}

void ImmersiveModeTabbedControllerCocoa::AddController() {
  NSWindow* window = browser_window();
  if (![window.titlebarAccessoryViewControllers
          containsObject:tab_titlebar_view_controller_]) {
    [window addTitlebarAccessoryViewController:tab_titlebar_view_controller_];
  }
}

void ImmersiveModeTabbedControllerCocoa::RemoveController() {
  [tab_titlebar_view_controller_ removeFromParentViewController];
}

void ImmersiveModeTabbedControllerCocoa::OnTopViewBoundsChanged(
    const gfx::Rect& bounds) {
  ImmersiveModeControllerCocoa::OnTopViewBoundsChanged(bounds);
  NSRect frame = NSRectFromCGRect(bounds.ToCGRect());
  [tab_titlebar_view_controller_.view
      setFrameSize:NSMakeSize(
                       frame.size.width,
                       tab_titlebar_view_controller_.view.frame.size.height)];
}

void ImmersiveModeTabbedControllerCocoa::RevealLocked() {
  AddController();
  TitlebarReveal();
  // Call after TitlebarReveal() for a proper layout.
  ImmersiveModeControllerCocoa::RevealLocked();
  LayoutWindowWithAnchorView(tab_window_, tab_content_view_);
}

void ImmersiveModeTabbedControllerCocoa::RevealUnlocked() {
  if (last_used_style() == mojom::ToolbarVisibilityStyle::kAutohide) {
    TitlebarHide();
  }

  // Call after TitlebarReveal() for a proper layout.
  ImmersiveModeControllerCocoa::RevealUnlocked();
  LayoutWindowWithAnchorView(tab_window_, tab_content_view_);
}

void ImmersiveModeTabbedControllerCocoa::TitlebarReveal() {
  NSToolbar* toolbar = browser_window().toolbar;
  if (toolbar.visible) {
    return;
  }
  toolbar.visible = YES;

  // The tab controller and toolbar views are siblings. When the toolbar view
  // is removed then re-added it becomes z-order on top of the tab controller
  // view. This becomes an issue when the window is not active but we want to
  // handle the first click. The toolbar view returns NO for
  // -acceptsFirstMouse:. Prefer to send the toolbar view to the back of the
  // siblings list. If we are unable to get a handle on the toolbar view remove
  // and re-add the tab controller so its view is z-order above the toolbar
  // view. See http://crbug/40283902 for details.
  // TODO(http://crbug.com/40261565): Remove when FB12010731 is fixed in AppKit.
  if (NSView* toolbar_view = toolbar.privateToolbarView) {
    [toolbar_view.superview addSubview:toolbar_view
                            positioned:NSWindowBelow
                            relativeTo:nil];
  } else {
    // We want to know if the toolbar no longer responds to _toolbarView but
    // since we have a backup workaround DumpWithoutCrashing();
    base::debug::DumpWithoutCrashing();
    RemoveController();
    AddController();
  }
}

void ImmersiveModeTabbedControllerCocoa::TitlebarHide() {
  browser_window().toolbar.visible = NO;
}

void ImmersiveModeTabbedControllerCocoa::Reanchor() {
  ImmersiveModeControllerCocoa::Reanchor();
  LayoutWindowWithAnchorView(tab_window_, tab_content_view_);
}

void ImmersiveModeTabbedControllerCocoa::OnChildWindowAdded(NSWindow* child) {
  // The `tab_window_` is a child of the `overlay_window_`. Ignore
  // the `tab_window_`.
  if (child == tab_window_) {
    return;
  }

  OrderTabWindowZOrderOnTop();
  ImmersiveModeControllerCocoa::OnChildWindowAdded(child);
}

void ImmersiveModeTabbedControllerCocoa::OnChildWindowRemoved(NSWindow* child) {
  // The `tab_window_` is a child of the `overlay_window_`. Ignore
  // the `tab_window_`.
  if (child == tab_window_) {
    return;
  }
  ImmersiveModeControllerCocoa::OnChildWindowRemoved(child);
}

bool ImmersiveModeTabbedControllerCocoa::ShouldObserveChildWindow(
    NSWindow* child) {
  // Filter out the `tab_window_`.
  if (child == tab_window_) {
    return false;
  }
  return ImmersiveModeControllerCocoa::ShouldObserveChildWindow(child);
}

bool ImmersiveModeTabbedControllerCocoa::IsTabbed() {
  return true;
}

void ImmersiveModeTabbedControllerCocoa::OrderTabWindowZOrderOnTop() {
  // Keep the tab window on top of its siblings. This will allow children of tab
  // window to always be z-order on top of overlay window children.
  // Practically this allows for the tab preview hover card to be z-order on top
  // of omnibox results popup.
  // If the tab window does not have a parent or the parent is not the overlay
  // window, do not perform the shuffle. Otherwise we could throw off the child
  // window counts in NativeWidgetNSWindowBridge::NotifyVisibilityChangeDown
  // during immersive fullscreen exit.
  if (tab_window_.parentWindow == overlay_window() &&
      overlay_window().childWindows.lastObject != tab_window_) {
    [overlay_window() removeChildWindow:tab_window_];
    [overlay_window() addChildWindow:tab_window_ ordered:NSWindowAbove];
  }
}

}  // namespace remote_cocoa
