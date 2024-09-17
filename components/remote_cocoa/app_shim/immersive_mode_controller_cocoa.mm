// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller_cocoa.h"

#include <utility>

#include "base/apple/foundation_util.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "components/remote_cocoa/app_shim/features.h"
#import "components/remote_cocoa/app_shim/immersive_mode_delegate_mac.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/gfx/geometry/rect.h"

namespace {
// Workaround for https://crbug.com/1369643
const double kThinControllerHeight = 0.5;
}  // namespace

// A stub NSWindowDelegate class that will be used to map the AppKit controlled
// NSWindow to the overlay view widget's NSWindow. The delegate will be used to
// help with input routing.
@interface ImmersiveModeMapper : NSObject <ImmersiveModeDelegate>
@property(weak) NSWindow* originalHostingWindow;
@end

@implementation ImmersiveModeMapper
@synthesize originalHostingWindow = _originalHostingWindow;
@end

// Private API that reflects the "reveal amount" of the toolbar.
// Ranges from [0, 1] where value 0 indicates the controller has a
// height of `fullScreenMinHeight`.
@interface NSTitlebarAccessoryViewController ()
- (double)revealAmount;
- (void)setRevealAmount:(double)revealAmount;
@end

@implementation ImmersiveModeTitlebarViewController

- (instancetype)initWithController:
    (base::WeakPtr<remote_cocoa::ImmersiveModeControllerCocoa>)
        immersiveModeController {
  if ((self = [super init])) {
    _blank_separator_view = [[NSView alloc] init];
    _immersive_mode_controller = immersiveModeController;
  }
  return self;
}

- (void)viewWillAppear {
  [super viewWillAppear];

  // Sometimes AppKit incorrectly positions NSToolbarFullScreenWindow entirely
  // offscreen (particularly when this is a out-of-process app shim). Toggling
  // visibility when appearing in the right window seems to fix the positioning.
  // Only toggle the visibility if fullScreenMinHeight is not zero though, as
  // triggering the repositioning when the toolbar is set to auto hide would
  // result in it being incorrectly positioned in that case.
  if (remote_cocoa::IsNSToolbarFullScreenWindow(self.view.window) &&
      self.fullScreenMinHeight > 0 && !self.hidden) {
    self.hidden = YES;
    self.hidden = NO;
  }
}

// This is a private API method that will be used on macOS 11+.
// Remove a small 1px blur between the overlay view and tabbed overlay view by
// returning a blank NSView.
- (NSView*)separatorView {
  return _blank_separator_view;
}

- (void)setRevealAmount:(double)revealAmount {
  [super setRevealAmount:revealAmount];
  _immersive_mode_controller->OnMenuBarRevealChanged();
  _immersive_mode_controller->OnToolbarRevealMaybeChanged();
}

- (void)setVisibility:(remote_cocoa::mojom::ToolbarVisibilityStyle)style {
  switch (style) {
    case remote_cocoa::mojom::ToolbarVisibilityStyle::kAlways:
      self.hidden = NO;
      self.fullScreenMinHeight = self.view.frame.size.height;
      break;
    case remote_cocoa::mojom::ToolbarVisibilityStyle::kAutohide:
      self.hidden = NO;
      self.fullScreenMinHeight = 0;
      break;
    case remote_cocoa::mojom::ToolbarVisibilityStyle::kNone:
      self.hidden = YES;
      break;
  }
  _immersive_mode_controller->OnToolbarRevealMaybeChanged();
}

- (void)forceVisibilityRefresh {
  // Toggling the controller visibility will allow the content view to resize
  // below Top Chrome and fix the positioning issue of the toolbar.
  self.hidden = YES;
  self.hidden = NO;
}

@end

// An NSView that will set the ImmersiveModeDelegate on the AppKit created
// window that ends up hosting this view via the
// NSTitlebarAccessoryViewController API.
@interface ImmersiveModeView : NSView
- (instancetype)initWithController:
    (base::WeakPtr<remote_cocoa::ImmersiveModeControllerCocoa>)controller;
@end

@implementation ImmersiveModeView {
  base::WeakPtr<remote_cocoa::ImmersiveModeControllerCocoa> _controller;
}

- (instancetype)initWithController:
    (base::WeakPtr<remote_cocoa::ImmersiveModeControllerCocoa>)controller {
  self = [super init];
  if (self) {
    _controller = std::move(controller);
  }
  return self;
}

- (void)viewWillMoveToWindow:(NSWindow*)window {
  if (_controller) {
    _controller->ImmersiveModeViewWillMoveToWindow(window);
  }
}

@end

@implementation OpaqueView
- (BOOL)isOpaque {
  return YES;
}
@end

namespace remote_cocoa {

bool IsNSToolbarFullScreenWindow(NSWindow* window) {
  // TODO(bur): Investigate other approaches to detecting
  // NSToolbarFullScreenWindow. This is a private class and the name could
  // change.
  return [window isKindOfClass:NSClassFromString(@"NSToolbarFullScreenWindow")];
}

ImmersiveModeControllerCocoa::ImmersiveModeControllerCocoa(
    NativeWidgetMacNSWindow* browser_window,
    NativeWidgetMacOverlayNSWindow* overlay_window)
    : weak_ptr_factory_(this) {
  browser_window_ = browser_window;
  overlay_window_ = overlay_window;
#ifndef NDEBUG
  overlay_window_.title = @"overlay";
#endif  // NDEBUG
  // Record this now, since it will be 0 at the end of the transition if the
  // menu bar is set to autohide.
  menu_bar_height_ =
      [[[NSApplication sharedApplication] mainMenu] menuBarHeight];

  overlay_window_.commandDispatchParentOverride = browser_window_;

  // A style of NSTitlebarSeparatorStyleAutomatic (default) will show a black
  // line separator when removing the NSWindowStyleMaskFullSizeContentView style
  // bit. We do not want a separator.
  browser_window_.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;

  // Create a new NSTitlebarAccessoryViewController that will host the
  // overlay_view_.
  immersive_mode_titlebar_view_controller_ =
      [[ImmersiveModeTitlebarViewController alloc]
          initWithController:weak_ptr_factory_.GetWeakPtr()];

  // Create a NSWindow delegate that will be used to map the AppKit created
  // NSWindow to the overlay view widget's NSWindow.
  immersive_mode_mapper_ = [[ImmersiveModeMapper alloc] init];
  immersive_mode_mapper_.originalHostingWindow = overlay_window_;
  immersive_mode_titlebar_view_controller_.view = [[ImmersiveModeView alloc]
      initWithController:weak_ptr_factory_.GetWeakPtr()];

  // Remove the content view from the overlay view widget's NSWindow, and hold a
  // local strong reference. This view will be re-parented into the AppKit
  // created NSWindow.
  BridgedContentView* overlay_content_view =
      base::apple::ObjCCastStrict<BridgedContentView>(
          overlay_window_.contentView);
  overlay_content_view_ = overlay_content_view;
  [overlay_content_view removeFromSuperview];

  // Use a placeholder view since the content has been moved to the
  // ImmersiveModeTitlebarViewController.
  overlay_window_.contentView = [[OpaqueView alloc] init];
  if (base::FeatureList::IsEnabled(
          remote_cocoa::features::kImmersiveFullscreenOverlayWindowDebug)) {
    [overlay_window_ debugWithColor:NSColor.greenColor];
  }

  // The overlay window will become a child of NSToolbarFullScreenWindow and sit
  // above it in the z-order. Allow mouse events that are not handled by the
  // BridgedContentView to passthrough the overlay window to the
  // NSToolbarFullScreenWindow. This will allow the NSToolbarFullScreenWindow to
  // become key when interacting with "top chrome".
  overlay_window_.ignoresMouseEvents = YES;

  // Add the overlay view to the accessory view controller getting ready to
  // hand everything over to AppKit.
  [immersive_mode_titlebar_view_controller_.view
      addSubview:overlay_content_view];
  immersive_mode_titlebar_view_controller_.layoutAttribute =
      NSLayoutAttributeBottom;

  thin_titlebar_view_controller_ =
      [[NSTitlebarAccessoryViewController alloc] init];
  thin_titlebar_view_controller_.view = [[NSView alloc] init];
  thin_titlebar_view_controller_.view.wantsLayer = YES;
  thin_titlebar_view_controller_.view.layer.backgroundColor =
      NSColor.blackColor.CGColor;
  thin_titlebar_view_controller_.layoutAttribute = NSLayoutAttributeBottom;
  thin_titlebar_view_controller_.fullScreenMinHeight = kThinControllerHeight;
}

ImmersiveModeControllerCocoa::~ImmersiveModeControllerCocoa() {
  overlay_window_.commandDispatchParentOverride = nil;
  StopObservingChildWindows(overlay_window_);

  // Rollback the view shuffling from enablement.
  [thin_titlebar_view_controller_ removeFromParentViewController];
  [overlay_content_view_ removeFromSuperview];
  overlay_window_.contentView = overlay_content_view_;
  [immersive_mode_titlebar_view_controller_ removeFromParentViewController];
  browser_window_.styleMask |= NSWindowStyleMaskFullSizeContentView;
  browser_window_.titlebarSeparatorStyle = NSTitlebarSeparatorStyleAutomatic;
}

void ImmersiveModeControllerCocoa::Init() {
  DCHECK(!initialized_);
  initialized_ = true;
  [browser_window_ addTitlebarAccessoryViewController:
                       immersive_mode_titlebar_view_controller_];

  // Keep the overlay content view's size in sync with its parent view.
  overlay_content_view_.translatesAutoresizingMaskIntoConstraints = NO;
  [overlay_content_view_.heightAnchor
      constraintEqualToAnchor:overlay_content_view_.superview.heightAnchor]
      .active = YES;
  [overlay_content_view_.widthAnchor
      constraintEqualToAnchor:overlay_content_view_.superview.widthAnchor]
      .active = YES;
  [overlay_content_view_.centerXAnchor
      constraintEqualToAnchor:overlay_content_view_.superview.centerXAnchor]
      .active = YES;
  [overlay_content_view_.centerYAnchor
      constraintEqualToAnchor:overlay_content_view_.superview.centerYAnchor]
      .active = YES;

  thin_titlebar_view_controller_.hidden = YES;
  [browser_window_
      addTitlebarAccessoryViewController:thin_titlebar_view_controller_];
}

void ImmersiveModeControllerCocoa::FullscreenTransitionCompleted() {
  fullscreen_transition_complete_ = true;
  ForceToolbarVisibilityUpdate();
  //  Establish reveal locks for windows that exist before entering fullscreen,
  //  such as permission popups and the find bar. Do this after the fullscreen
  //  transition has ended to avoid graphical flashes during the animation.
  for (NSWindow* child in overlay_window_.childWindows) {
    if (!ShouldObserveChildWindow(child)) {
      continue;
    }
    OnChildWindowAdded(child);
  }

  // Watch for child windows. When they are added the overlay view will be
  // revealed as appropriate.
  ObserveChildWindows(overlay_window_);

  NotifyBrowserWindowAboutToolbarRevealChanged();
  if (NativeWidgetNSWindowBridge* bridge =
          NativeWidgetNSWindowBridge::GetFromNativeWindow(browser_window_)) {
    NSScreen* screen = browser_window_.screen;
    // We have an autohiding menu bar if:
    // - We don't have a notch AND
    // - "Automatically Hide and Show the Menu Bar" in settings is "Always" or
    //    "In Full Screen Only".
    // There's no good API, public or private to test the latter. Instead, we
    // can infer it if the screen's visible frame is shorter than the frame.
    // Since we hide the dock in fullscreen, this is relatively reliable. It
    // would be even better if we could verify that the difference is the height
    // of the menu bar, but in practice, it's one pixel off, so let's not
    // encode that.
    BOOL autohiding_menu =
        screen.frame.size.height == screen.visibleFrame.size.height;
    bridge->OnAutohidingMenuBarHeightChanged(autohiding_menu ? menu_bar_height_
                                                             : 0);
  }
}

void ImmersiveModeControllerCocoa::OnTopViewBoundsChanged(
    const gfx::Rect& bounds) {
  // Set the height of the AppKit fullscreen view. The width will be
  // automatically handled by AppKit.
  NSRect frame = NSRectFromCGRect(bounds.ToCGRect());
  NSView* overlay_view = immersive_mode_titlebar_view_controller_.view;
  NSSize size = overlay_view.window.frame.size;
  if (frame.size.height != size.height) {
    size.height = frame.size.height;
    [overlay_view setFrameSize:size];
  }

  // If the toolbar is always visible, update the fullscreen min height.
  // Also update the fullscreen min height if the toolbar auto hides, but only
  // if the toolbar is currently revealed.
  if (fullscreen_transition_complete_ &&
      (last_used_style_ == mojom::ToolbarVisibilityStyle::kAlways ||
       (last_used_style_ == mojom::ToolbarVisibilityStyle::kAutohide &&
        reveal_lock_count_ > 0))) {
    [immersive_mode_titlebar_view_controller_
        setVisibility:mojom::ToolbarVisibilityStyle::kAlways];
  }
}

void ImmersiveModeControllerCocoa::UpdateToolbarVisibility(
    std::optional<mojom::ToolbarVisibilityStyle> style) {
  // Remember the last used style for internal use of UpdateToolbarVisibility.
  std::optional<mojom::ToolbarVisibilityStyle> old_style =
      std::exchange(last_used_style_, style);
  if (!style.has_value() || old_style == style) {
    return;
  }

  // Only make changes if there are no outstanding reveal locks.
  if (reveal_lock_count_ > 0) {
    return;
  }

  switch (style.value()) {
    case mojom::ToolbarVisibilityStyle::kAlways:
      SetIgnoreRevealLocks(false);
      [immersive_mode_titlebar_view_controller_
          setVisibility:mojom::ToolbarVisibilityStyle::kAlways];

      // Top chrome is removed from the content view when the browser window
      // starts the fullscreen transition, however the request is asynchronous
      // and sometimes top chrome is still present in the content view when the
      // animation starts. This results in top chrome being painted twice during
      // the fullscreen animation, once in the content view and once in
      // `immersive_mode_titlebar_view_controller_`. Keep
      // NSWindowStyleMaskFullSizeContentView active during the fullscreen
      // transition to allow
      // `immersive_mode_titlebar_view_controller_` to be
      // displayed z-order on top of the content view. This will cover up any
      // perceived jank.
      // TODO(crbug.com/40873399): Handle fullscreen exit.
      if (!fullscreen_transition_complete_) {
        break;
      }

      // Only force a visibility refresh if necessary.
      if (browser_window_.styleMask & NSWindowStyleMaskFullSizeContentView) {
        browser_window_.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
        [immersive_mode_titlebar_view_controller_ forceVisibilityRefresh];
      }
      break;
    case mojom::ToolbarVisibilityStyle::kAutohide:
      SetIgnoreRevealLocks(false);
      [immersive_mode_titlebar_view_controller_
          setVisibility:mojom::ToolbarVisibilityStyle::kAutohide];
      browser_window_.styleMask |= NSWindowStyleMaskFullSizeContentView;
      break;
    case mojom::ToolbarVisibilityStyle::kNone:
      SetIgnoreRevealLocks(true);
      [immersive_mode_titlebar_view_controller_
          setVisibility:mojom::ToolbarVisibilityStyle::kNone];
      UpdateThinControllerVisibility();
      break;
  }
}

void ImmersiveModeControllerCocoa::ForceToolbarVisibilityUpdate() {
  // If a style has not been set, there is nothing else to do.
  if (!last_used_style_.has_value()) {
    return;
  }

  // Set `last_used_style_` to std::nullopt so that the passed in style and
  // `last_used_style_` are different, forcing a visibility update.
  UpdateToolbarVisibility(std::exchange(last_used_style_, std::nullopt));
}

void ImmersiveModeControllerCocoa::ObserveChildWindows(NSWindow* window) {
  // Watch the Widget for addition and removal of child Widgets.
  NativeWidgetMacNSWindow* widget_window =
      base::apple::ObjCCastStrict<NativeWidgetMacNSWindow>(window);
  widget_window.childWindowAddedHandler = ^(NSWindow* child) {
    OnChildWindowAdded(child);
  };
  widget_window.childWindowRemovedHandler = ^(NSWindow* child) {
    OnChildWindowRemoved(child);
  };
}

void ImmersiveModeControllerCocoa::StopObservingChildWindows(NSWindow* window) {
  NativeWidgetMacNSWindow* widget_window =
      base::apple::ObjCCastStrict<NativeWidgetMacNSWindow>(window);
  widget_window.childWindowAddedHandler = nil;
  widget_window.childWindowRemovedHandler = nil;
}

bool ImmersiveModeControllerCocoa::ShouldObserveChildWindow(NSWindow* child) {
  return true;
}

NSWindow* ImmersiveModeControllerCocoa::browser_window() {
  return browser_window_;
}
NativeWidgetMacOverlayNSWindow* ImmersiveModeControllerCocoa::overlay_window() {
  return overlay_window_;
}
BridgedContentView* ImmersiveModeControllerCocoa::overlay_content_view() {
  return overlay_content_view_;
}

void ImmersiveModeControllerCocoa::OnChildWindowAdded(NSWindow* child) {
  // Skip applying the reveal lock if the window is in the process of being
  // re-ordered, as this may inadvertently trigger a recursive re-ordering.
  // This is because changing the titlebar visibility (which reveal lock does)
  // can itself initiate another window re-ordering, causing this method to be
  // re-entered.
  if (((NativeWidgetMacNSWindow*)child).isShufflingForOrdering) {
    return;
  }
  // TODO(kerenzhu): the sole purpose of `window_lock_received_` is to
  // verify that we don't lock twice for a single window.
  // We can remove it once this is verified.
  CHECK(!base::Contains(window_lock_received_, child));
  window_lock_received_.insert(child);
  RevealLock();

  // TODO(crbug.com/40234209): Handle a detached find bar.
}

void ImmersiveModeControllerCocoa::OnChildWindowRemoved(NSWindow* child) {
  // Skip applying the reveal lock if the window is in the process of being
  // re-ordered, as this may inadvertently trigger a recursive re-ordering.
  // This is because changing the titlebar visibility (which reveal lock does)
  // can itself initiate another window re-ordering, causing this method to be
  // re-entered.
  if (((NativeWidgetMacNSWindow*)child).isShufflingForOrdering) {
    return;
  }
  CHECK(base::Contains(window_lock_received_, child));
  window_lock_received_.erase(child);
  RevealUnlock();
}

void ImmersiveModeControllerCocoa::RevealLock() {
  reveal_lock_count_++;
  if (ignore_reveal_locks_) {
    return;
  }
  ApplyRevealLockState();
}

void ImmersiveModeControllerCocoa::RevealUnlock() {
  reveal_lock_count_--;
  CHECK(reveal_lock_count_ >= 0);
  if (ignore_reveal_locks_) {
    return;
  }
  ApplyRevealLockState();
}

void ImmersiveModeControllerCocoa::ApplyRevealLockState() {
  if (reveal_lock_count_) {
    RevealLocked();
  } else {
    RevealUnlocked();
  }
}

void ImmersiveModeControllerCocoa::SetIgnoreRevealLocks(bool ignore) {
  // Set the new ignore value. If the value is same, return. If ignore is true
  // all we needed to do is update the ignore_reveal_locks_, which we just did
  // so also return.
  bool was_ignore = std::exchange(ignore_reveal_locks_, ignore);
  if (was_ignore == ignore_reveal_locks_ || ignore_reveal_locks_) {
    return;
  }

  // If we make it here, we are disabling ignore reveal locks. In other words we
  // are going to stop ignoring the reveal locks. We need to visually apply the
  // correct reveal state to make up for any missed calls to RevealLocked() /
  // RevealUnlocked() while the ignore was in place.
  ApplyRevealLockState();
}

void ImmersiveModeControllerCocoa::RevealLocked() {
  [immersive_mode_titlebar_view_controller_
      setVisibility:mojom::ToolbarVisibilityStyle::kAlways];
}

void ImmersiveModeControllerCocoa::RevealUnlocked() {
  ForceToolbarVisibilityUpdate();
}

bool ImmersiveModeControllerCocoa::IsToolbarRevealed() {
  // If `fullScreenMinHeight` is greater than 0, "Always Show Toolbar in Full
  // Screen" is enabled or there is an active reveal lock. If `revealAmount` >
  // 0, the toolbar is revealed because of mouse hovering. In each case, the
  // toolbar is visible.
  return immersive_mode_titlebar_view_controller_.fullScreenMinHeight > 0 ||
         immersive_mode_titlebar_view_controller_.revealAmount > 0;
}

void ImmersiveModeControllerCocoa::OnToolbarRevealMaybeChanged() {
  Reanchor();
  UpdateThinControllerVisibility();
  bool is_toolbar_revealed = IsToolbarRevealed();
  if (is_toolbar_revealed_ != is_toolbar_revealed) {
    is_toolbar_revealed_ = is_toolbar_revealed;
    NotifyBrowserWindowAboutToolbarRevealChanged();
  }
}

void ImmersiveModeControllerCocoa::OnMenuBarRevealChanged() {
  Reanchor();
  if (NativeWidgetNSWindowBridge* bridge =
          NativeWidgetNSWindowBridge::GetFromNativeWindow(browser_window_)) {
    bridge->OnImmersiveFullscreenMenuBarRevealChanged(
        immersive_mode_titlebar_view_controller_.revealAmount);
  }
}

void ImmersiveModeControllerCocoa::ImmersiveModeViewWillMoveToWindow(
    NSWindow* window) {
  // AppKit hands this view controller over to a fullscreen transition window
  // before we finally land at the NSToolbarFullScreenWindow. Add the frame
  // observer only once we reach the NSToolbarFullScreenWindow.
  if (remote_cocoa::IsNSToolbarFullScreenWindow(window)) {
    // This window is created by AppKit. Make sure it doesn't have a delegate
    // so we can use it for out own purposes.
    DCHECK(!window.delegate);
    window.delegate = immersive_mode_mapper_;

    // Attach overlay_widget to NSToolbarFullScreen so that children are placed
    // on top of the toolbar. When exiting fullscreen, we don't re-parent the
    // overlay window back to the browser window because it seems to trigger
    // re-entrancy in AppKit and cause crash. This is safe because sub-widgets
    // will be re-parented to the browser window and therefore the overlay
    // window won't have any observable effect.
    // Also, explicitly remove the overlay window from the browser window.
    // Leaving a dangling reference to the overlay window on the browser window
    // causes odd behavior.
    [browser_window_ removeChildWindow:overlay_window()];
    [window addChildWindow:overlay_window() ordered:NSWindowAbove];
  }
}

bool ImmersiveModeControllerCocoa::IsTabbed() {
  return false;
}

bool ImmersiveModeControllerCocoa::IsContentFullscreen() {
  return last_used_style_ == remote_cocoa::mojom::ToolbarVisibilityStyle::kNone;
}

double ImmersiveModeControllerCocoa::GetOffscreenYOrigin() {
  // Get the height of the screen plus the browser window's y origin. Use this
  // as the y origin for the overlay windows, it will move them offscreen. The
  // browser window's y origin is especially important for external displays
  // where the y origin is not 0, such as vertically aligned displays.
  double y =
      browser_window_.screen.frame.size.height + browser_window_.frame.origin.y;

  // Make sure to make it past the safe area insets, otherwise some portion
  // of the window may still be displayed.
  if (@available(macOS 12.0, *)) {
    y += browser_window_.screen.safeAreaInsets.top;
  }

  return y;
}

void ImmersiveModeControllerCocoa::
    NotifyBrowserWindowAboutToolbarRevealChanged() {
  if (NativeWidgetNSWindowBridge* bridge =
          NativeWidgetNSWindowBridge::GetFromNativeWindow(browser_window_)) {
    bridge->OnImmersiveFullscreenToolbarRevealChanged(IsToolbarRevealed());
  }
}

void ImmersiveModeControllerCocoa::UpdateThinControllerVisibility() {
  if (last_used_style_ == mojom::ToolbarVisibilityStyle::kNone &&
      immersive_mode_titlebar_view_controller_.revealAmount == 0) {
    // Needed when eventually exiting from content fullscreen and returning
    // to mojom::ToolbarVisibilityStyle::kAlways. This is a workaround for
    // https://crbug.com/1369643.
    //
    // We hit this situation when a window enters browser fullscreen, then
    // enters content fullscreen. Exiting content fullscreen will drop the
    // window back into browser fullscreen.
    //
    // We don't know what state we will be returning to when exiting content
    // fullscreen, but `kAlways` is one of the options. Because of this we
    // need to keep the thin controller visible during content fullscreen,
    // otherwise we will trip https://crbug.com/1369643.
    //
    // Exiting content fullscreen and returning to `kAutohide` does not
    // trigger https://crbug.com/1369643, but to keep things simple we keep
    // the mitigation in place for all transitions out of content fullscreen.

    // In short, when transitioning to `kNone` we need to take steps to
    // mitigate https://crbug.com/1369643 which is triggered when we
    // eventually transition out of `kNone`.
    thin_titlebar_view_controller_.hidden = NO;
  } else {
    // The extra -setHidden:YES call is to clear a visual artifact when
    // transitioning from `kNone`.
    thin_titlebar_view_controller_.hidden = YES;
    thin_titlebar_view_controller_.hidden = IsToolbarRevealed();
  }
}

void ImmersiveModeControllerCocoa::LayoutWindowWithAnchorView(
    NSWindow* window,
    NSView* anchor_view) {
  // Only move |window| when it is on the active space. If |window| has a child
  // with key an unwanted space switch will occur. See http://crbug.com/1519484
  // and http://crbug.com/1316543.
  if (!window.isOnActiveSpace) {
    return;
  }

  // Find the anchor view's point on screen (bottom left).
  NSPoint point_in_window = [anchor_view convertPoint:NSZeroPoint toView:nil];
  NSPoint point_on_screen =
      [anchor_view.window convertPointToScreen:point_in_window];

  // If the anchor view is clipped move the window off screen. A clipped
  // anchor view indicates the titlebar is hidden or is in transition AND the
  // browser content view takes up the whole window
  // ("Always Show Toolbar in Full Screen" is disabled). When we are in this
  // state we don't want the window on screen, otherwise it may mask input to
  // the browser view. In all other cases will not enter this branch and the
  // window will be placed at the same coordinates as the anchor view.
  if (anchor_view.visibleRect.size.height != anchor_view.frame.size.height) {
    point_on_screen.y = GetOffscreenYOrigin();
  }

  // If the toolbar is hidden (mojom::ToolbarVisibilityStyle::kNone) also move
  // the window offscreen. This applies to all versions of macOS where Chrome
  // can be run.
  if (last_used_style_ == mojom::ToolbarVisibilityStyle::kNone) {
    point_on_screen.y = GetOffscreenYOrigin();
  }

  [window setFrameOrigin:point_on_screen];
}

void ImmersiveModeControllerCocoa::Reanchor() {
  LayoutWindowWithAnchorView(overlay_window_, overlay_content_view_);
}

}  // namespace remote_cocoa
