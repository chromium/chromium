// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/mac/foundation_util.h"
#import "components/remote_cocoa/app_shim/immersive_mode_delegate_mac.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/gfx/geometry/rect.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const double kThinControllerHeight = 0.5;

NSView* GetNSTitlebarContainerViewFromWindow(NSWindow* window) {
  for (NSView* view in window.contentView.subviews) {
    if ([view isKindOfClass:NSClassFromString(@"NSTitlebarContainerView")]) {
      return view;
    }
  }
  return nil;
}

}  // namespace

@interface ImmersiveModeTitlebarObserver () {
  base::WeakPtr<remote_cocoa::ImmersiveModeController> _controller;
  NSView* __weak _titlebarContainerView;
}
@end

@implementation ImmersiveModeTitlebarObserver

- (instancetype)initWithController:
                    (base::WeakPtr<remote_cocoa::ImmersiveModeController>)
                        controller
             titlebarContainerView:(NSView*)titlebarContainerView {
  self = [super init];
  if (self) {
    _controller = std::move(controller);
    _titlebarContainerView = titlebarContainerView;
    [_titlebarContainerView addObserver:self
                             forKeyPath:@"frame"
                                options:NSKeyValueObservingOptionInitial |
                                        NSKeyValueObservingOptionNew
                                context:nullptr];
  }
  return self;
}

- (void)dealloc {
  [_titlebarContainerView removeObserver:self forKeyPath:@"frame"];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if (!_controller || ![keyPath isEqualToString:@"frame"]) {
    return;
  }

  NSRect frame = [change[@"new"] rectValue];
  _controller->OnTitlebarFrameDidChange(frame);
}

@end

// A stub NSWindowDelegate class that will be used to map the AppKit controlled
// NSWindow to the overlay view widget's NSWindow. The delegate will be used to
// help with input routing.
@interface ImmersiveModeMapper : NSObject <ImmersiveModeDelegate>
@property(weak) NSWindow* originalHostingWindow;
@end

@implementation ImmersiveModeMapper
@synthesize originalHostingWindow = _originalHostingWindow;
@end

// Host of the overlay view.
@interface ImmersiveModeTitlebarViewController
    : NSTitlebarAccessoryViewController {
  NSView* __strong _blank_separator_view;
}
@end

@implementation ImmersiveModeTitlebarViewController

- (instancetype)init {
  if ((self = [super init])) {
    _blank_separator_view = [[NSView alloc] init];
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
      self.fullScreenMinHeight != 0 && !self.hidden) {
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

@end

// An NSView that will set the ImmersiveModeDelegate on the AppKit created
// window that ends up hosting this view via the
// NSTitlebarAccessoryViewController API.
@interface ImmersiveModeView : NSView
- (instancetype)initWithController:
    (base::WeakPtr<remote_cocoa::ImmersiveModeController>)controller;
@end

@implementation ImmersiveModeView {
  base::WeakPtr<remote_cocoa::ImmersiveModeController> _controller;
}

- (instancetype)initWithController:
    (base::WeakPtr<remote_cocoa::ImmersiveModeController>)controller {
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

namespace remote_cocoa {

bool IsNSToolbarFullScreenWindow(NSWindow* window) {
  // TODO(bur): Investigate other approaches to detecting
  // NSToolbarFullScreenWindow. This is a private class and the name could
  // change.
  return [window isKindOfClass:NSClassFromString(@"NSToolbarFullScreenWindow")];
}

struct ImmersiveModeController::ObjCStorage {
  NSWindow* __weak browser_window;
  NSWindow* __weak overlay_window;
  BridgedContentView* __weak overlay_content_view;

  // A controller for top chrome.
  ImmersiveModeTitlebarViewController* __strong
      immersive_mode_titlebar_view_controller;

  // A controller that keeps a small portion (0.5px) of the fullscreen AppKit
  // NSWindow on screen.
  // This controller is used as a workaround for an AppKit bug that displays a
  // black bar when changing a NSTitlebarAccessoryViewController's
  // fullScreenMinHeight from zero to non-zero.
  // TODO(https://crbug.com/1369643): Remove when fixed by Apple.
  NSTitlebarAccessoryViewController* __strong thin_titlebar_view_controller;

  ImmersiveModeMapper* __strong immersive_mode_mapper;
  ImmersiveModeTitlebarObserver* __strong immersive_mode_titlebar_observer;

  // Keeps track of which windows have received titlebar and reveal locks.
  std::set<NSWindow*> window_lock_received;
};

ImmersiveModeController::ImmersiveModeController(NSWindow* browser_window,
                                                 NSWindow* overlay_window)
    : objc_storage_(std::make_unique<ObjCStorage>()), weak_ptr_factory_(this) {
  objc_storage_->browser_window = browser_window;
  objc_storage_->overlay_window = overlay_window;

  // A style of NSTitlebarSeparatorStyleAutomatic (default) will show a black
  // line separator when removing the NSWindowStyleMaskFullSizeContentView style
  // bit. We do not want a separator. Pre-macOS 11 there is no titlebar
  // separator.
  if (@available(macOS 11.0, *)) {
    objc_storage_->browser_window.titlebarSeparatorStyle =
        NSTitlebarSeparatorStyleNone;
  }

  // Create a new NSTitlebarAccessoryViewController that will host the
  // overlay_view_.
  objc_storage_->immersive_mode_titlebar_view_controller =
      [[ImmersiveModeTitlebarViewController alloc] init];

  // Create a NSWindow delegate that will be used to map the AppKit created
  // NSWindow to the overlay view widget's NSWindow.
  objc_storage_->immersive_mode_mapper = [[ImmersiveModeMapper alloc] init];
  objc_storage_->immersive_mode_mapper.originalHostingWindow =
      objc_storage_->overlay_window;
  objc_storage_->immersive_mode_titlebar_view_controller.view =
      [[ImmersiveModeView alloc]
          initWithController:weak_ptr_factory_.GetWeakPtr()];

  // Remove the content view from the overlay view widget's NSWindow, and hold a
  // local strong reference. This view will be re-parented into the AppKit
  // created NSWindow.
  BridgedContentView* overlay_content_view =
      base::mac::ObjCCastStrict<BridgedContentView>(
          objc_storage_->overlay_window.contentView);
  objc_storage_->overlay_content_view = overlay_content_view;
  [overlay_content_view removeFromSuperview];

  // The original content view (top chrome) has been moved to the AppKit
  // created NSWindow. Create a new content view but reuse the original bridge
  // so that mouse drags are handled.
  objc_storage_->overlay_window.contentView =
      [[BridgedContentView alloc] initWithBridge:overlay_content_view.bridge
                                          bounds:gfx::Rect()];

  // The overlay window will become a child of NSToolbarFullScreenWindow and sit
  // above it in the z-order. Allow mouse events that are not handled by the
  // BridgedContentView to passthrough the overlay window to the
  // NSToolbarFullScreenWindow. This will allow the NSToolbarFullScreenWindow to
  // become key when interacting with "top chrome".
  objc_storage_->overlay_window.ignoresMouseEvents = YES;

  // Add the overlay view to the accessory view controller getting ready to
  // hand everything over to AppKit.
  [objc_storage_->immersive_mode_titlebar_view_controller.view
      addSubview:overlay_content_view];
  objc_storage_->immersive_mode_titlebar_view_controller.layoutAttribute =
      NSLayoutAttributeBottom;

  objc_storage_->thin_titlebar_view_controller =
      [[NSTitlebarAccessoryViewController alloc] init];
  objc_storage_->thin_titlebar_view_controller.view = [[NSView alloc] init];
  objc_storage_->thin_titlebar_view_controller.view.wantsLayer = YES;
  objc_storage_->thin_titlebar_view_controller.view.layer.backgroundColor =
      NSColor.blackColor.CGColor;
  objc_storage_->thin_titlebar_view_controller.layoutAttribute =
      NSLayoutAttributeBottom;
  objc_storage_->thin_titlebar_view_controller.fullScreenMinHeight =
      kThinControllerHeight;
}

ImmersiveModeController::~ImmersiveModeController() {
  // Remove the titlebar observer before moving the view.
  objc_storage_->immersive_mode_titlebar_observer = nil;

  StopObservingChildWindows(objc_storage_->overlay_window);

  // Rollback the view shuffling from enablement.
  [objc_storage_->thin_titlebar_view_controller removeFromParentViewController];
  [objc_storage_->overlay_content_view removeFromSuperview];
  objc_storage_->overlay_window.contentView =
      objc_storage_->overlay_content_view;
  [objc_storage_->immersive_mode_titlebar_view_controller
          removeFromParentViewController];
  objc_storage_->browser_window.styleMask |=
      NSWindowStyleMaskFullSizeContentView;
  if (@available(macOS 11.0, *)) {
    objc_storage_->browser_window.titlebarSeparatorStyle =
        NSTitlebarSeparatorStyleAutomatic;
  }
}

void ImmersiveModeController::Enable() {
  DCHECK(!enabled_);
  enabled_ = true;
  [objc_storage_->browser_window
      addTitlebarAccessoryViewController:
          objc_storage_->immersive_mode_titlebar_view_controller];

  // Keep the overlay content view's size in sync with its parent view.
  objc_storage_->overlay_content_view
      .translatesAutoresizingMaskIntoConstraints = NO;
  [objc_storage_->overlay_content_view.heightAnchor
      constraintEqualToAnchor:objc_storage_->overlay_content_view.superview
                                  .heightAnchor]
      .active = YES;
  [objc_storage_->overlay_content_view.widthAnchor
      constraintEqualToAnchor:objc_storage_->overlay_content_view.superview
                                  .widthAnchor]
      .active = YES;
  [objc_storage_->overlay_content_view.centerXAnchor
      constraintEqualToAnchor:objc_storage_->overlay_content_view.superview
                                  .centerXAnchor]
      .active = YES;
  [objc_storage_->overlay_content_view.centerYAnchor
      constraintEqualToAnchor:objc_storage_->overlay_content_view.superview
                                  .centerYAnchor]
      .active = YES;

  objc_storage_->thin_titlebar_view_controller.hidden = YES;
  [objc_storage_->browser_window
      addTitlebarAccessoryViewController:objc_storage_->
                                         thin_titlebar_view_controller];

  NSRect frame = objc_storage_->thin_titlebar_view_controller.view.frame;
  frame.size.height = kThinControllerHeight;
  objc_storage_->thin_titlebar_view_controller.view.frame = frame;
}

void ImmersiveModeController::FullscreenTransitionCompleted() {
  fullscreen_transition_complete_ = true;
  UpdateToolbarVisibility(last_used_style_);

  //  Establish reveal locks for windows that exist before entering fullscreen,
  //  such as permission popups and the find bar. Do this after the fullscreen
  //  transition has ended to avoid graphical flashes during the animation.
  for (NSWindow* child in objc_storage_->overlay_window.childWindows) {
    if (!ShouldObserveChildWindow(child)) {
      continue;
    }
    OnChildWindowAdded(child);
  }

  // Watch for child windows. When they are added the overlay view will be
  // revealed as appropriate.
  ObserveChildWindows(objc_storage_->overlay_window);
}

void ImmersiveModeController::OnTopViewBoundsChanged(const gfx::Rect& bounds) {
  // Set the height of the AppKit fullscreen view. The width will be
  // automatically handled by AppKit.
  NSRect frame = NSRectFromCGRect(bounds.ToCGRect());
  NSView* overlay_view =
      objc_storage_->immersive_mode_titlebar_view_controller.view;
  NSSize size = overlay_view.window.frame.size;
  if (frame.size.height != size.height) {
    size.height = frame.size.height;
    [overlay_view setFrameSize:size];
  }

  UpdateToolbarVisibility(last_used_style_);

  // If the toolbar is always visible, update the fullscreen min height.
  // Also update the fullscreen min height if the toolbar auto hides, but only
  // if the toolbar is currently revealed.
  if (last_used_style_ == mojom::ToolbarVisibilityStyle::kAlways ||
      (last_used_style_ == mojom::ToolbarVisibilityStyle::kAutohide &&
       reveal_lock_count_ > 0)) {
    objc_storage_->immersive_mode_titlebar_view_controller.fullScreenMinHeight =
        frame.size.height;
  }
}

void ImmersiveModeController::UpdateToolbarVisibility(
    mojom::ToolbarVisibilityStyle style) {
  // Remember the last used style for internal use of UpdateToolbarVisibility.
  last_used_style_ = style;

  // Only make changes if there are no outstanding reveal locks.
  if (reveal_lock_count_ > 0) {
    return;
  }

  switch (style) {
    case mojom::ToolbarVisibilityStyle::kAlways:
      objc_storage_->immersive_mode_titlebar_view_controller
          .fullScreenMinHeight =
          objc_storage_->immersive_mode_titlebar_view_controller.view.frame.size
              .height;
      objc_storage_->thin_titlebar_view_controller.hidden = YES;

      // Top chrome is removed from the content view when the browser window
      // starts the fullscreen transition, however the request is asynchronous
      // and sometimes top chrome is still present in the content view when the
      // animation starts. This results in top chrome being painted twice during
      // the fullscreen animation, once in the content view and once in
      // `objc_storage_->immersive_mode_titlebar_view_controller`. Keep
      // NSWindowStyleMaskFullSizeContentView active during the fullscreen
      // transition to allow
      // `objc_storage_->immersive_mode_titlebar_view_controller` to be
      // displayed z-order on top of the content view. This will cover up any
      // perceived jank.
      // TODO(https://crbug.com/1375995): Handle fullscreen exit.
      if (fullscreen_transition_complete_) {
        objc_storage_->browser_window.styleMask &=
            ~NSWindowStyleMaskFullSizeContentView;
      } else {
        objc_storage_->browser_window.styleMask |=
            NSWindowStyleMaskFullSizeContentView;
      }

      // Toggling the controller will allow the content view to resize below Top
      // Chrome.
      objc_storage_->immersive_mode_titlebar_view_controller.hidden = YES;
      objc_storage_->immersive_mode_titlebar_view_controller.hidden = NO;
      break;
    case mojom::ToolbarVisibilityStyle::kAutohide:
      objc_storage_->immersive_mode_titlebar_view_controller.hidden = NO;

      // TODO(https://crbug.com/1369643): Remove the thin controller.
      // The thin titlebar controller keeps a tiny portion of the AppKit
      // fullscreen NSWindow on screen as a workaround for
      // https://crbug.com/1369643. Toggle to clear any artifacts from a
      // previous state.
      objc_storage_->thin_titlebar_view_controller.hidden = YES;
      objc_storage_->thin_titlebar_view_controller.hidden = NO;

      objc_storage_->immersive_mode_titlebar_view_controller
          .fullScreenMinHeight = 0;
      objc_storage_->browser_window.styleMask |=
          NSWindowStyleMaskFullSizeContentView;
      break;
    case mojom::ToolbarVisibilityStyle::kNone:
      // TODO(https://crbug.com/1369643): Remove the thin controller.
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
      objc_storage_->thin_titlebar_view_controller.hidden = NO;

      objc_storage_->immersive_mode_titlebar_view_controller.hidden = YES;
      break;
  }
}

void ImmersiveModeController::ObserveChildWindows(NSWindow* window) {
  // Watch the Widget for addition and removal of child Widgets.
  NativeWidgetMacNSWindow* widget_window =
      base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(window);
  widget_window.childWindowAddedHandler = ^(NSWindow* child) {
    OnChildWindowAdded(child);
  };
  widget_window.childWindowRemovedHandler = ^(NSWindow* child) {
    OnChildWindowRemoved(child);
  };
}

void ImmersiveModeController::StopObservingChildWindows(NSWindow* window) {
  NativeWidgetMacNSWindow* widget_window =
      base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(window);
  widget_window.childWindowAddedHandler = nil;
  widget_window.childWindowRemovedHandler = nil;
}

bool ImmersiveModeController::ShouldObserveChildWindow(NSWindow* child) {
  return true;
}

NSWindow* ImmersiveModeController::browser_window() {
  return objc_storage_->browser_window;
}
NSWindow* ImmersiveModeController::overlay_window() {
  return objc_storage_->overlay_window;
}
BridgedContentView* ImmersiveModeController::overlay_content_view() {
  return objc_storage_->overlay_content_view;
}

void ImmersiveModeController::OnChildWindowAdded(NSWindow* child) {
  // When windows are re-ordered they get removed and re-added triggering
  // OnChildWindowRemoved and OnChildWindowAdded calls.
  // Prevent any given window from obtaining more than one lock.
  if (!base::Contains(objc_storage_->window_lock_received, child)) {
    objc_storage_->window_lock_received.insert(child);
    RevealLock();
  }

  // TODO(https://crbug.com/1350595): Handle a detached find bar.
}

void ImmersiveModeController::OnChildWindowRemoved(NSWindow* child) {
  if (base::Contains(objc_storage_->window_lock_received, child)) {
    objc_storage_->window_lock_received.erase(child);
    RevealUnlock();
  }
}

void ImmersiveModeController::RevealLock() {
  reveal_lock_count_++;
  objc_storage_->immersive_mode_titlebar_view_controller.fullScreenMinHeight =
      objc_storage_->immersive_mode_titlebar_view_controller.view.frame.size
          .height;
}

void ImmersiveModeController::RevealUnlock() {
  // Re-hide the toolbar if appropriate.
  if (--reveal_lock_count_ < 1 &&
      objc_storage_->immersive_mode_titlebar_view_controller
              .fullScreenMinHeight > 0 &&
      last_used_style_ == mojom::ToolbarVisibilityStyle::kAutohide) {
    objc_storage_->immersive_mode_titlebar_view_controller.fullScreenMinHeight =
        0;
  }

  // Account for last_used_style_ changing while a reveal lock was active.
  if (reveal_lock_count_ < 1) {
    UpdateToolbarVisibility(last_used_style_);
  }
  DCHECK(reveal_lock_count_ >= 0);
}

void ImmersiveModeController::ImmersiveModeViewWillMoveToWindow(
    NSWindow* window) {
  // AppKit hands this view controller over to a fullscreen transition window
  // before we finally land at the NSToolbarFullScreenWindow. Add the frame
  // observer only once we reach the NSToolbarFullScreenWindow.
  if (remote_cocoa::IsNSToolbarFullScreenWindow(window)) {
    // This window is created by AppKit. Make sure it doesn't have a delegate
    // so we can use it for out own purposes.
    DCHECK(!window.delegate);
    window.delegate = objc_storage_->immersive_mode_mapper;

    // Attach overlay_widget to NSToolbarFullScreen so that children are placed
    // on top of the toolbar. When exiting fullscreen, we don't re-parent the
    // overlay window back to the browser window because it seems to trigger
    // re-entrancy in AppKit and cause crash. This is safe because sub-widgets
    // will be re-parented to the browser window and therefore the overlay
    // window won't have any observable effect.
    // Also, explicitly remove the overlay window from the browser window.
    // Leaving a dangling reference to the overlay window on the browser window
    // causes odd behavior.
    [objc_storage_->browser_window removeChildWindow:overlay_window()];
    [window addChildWindow:overlay_window() ordered:NSWindowAbove];

    NSView* view = GetNSTitlebarContainerViewFromWindow(window);
    DCHECK(view);
    // Create the titlebar observer. Observing can only start once the view has
    // been fully re-parented into the AppKit fullscreen window.
    objc_storage_->immersive_mode_titlebar_observer =
        [[ImmersiveModeTitlebarObserver alloc]
               initWithController:weak_ptr_factory_.GetWeakPtr()
            titlebarContainerView:view];
  }
}

void ImmersiveModeController::OnTitlebarFrameDidChange(NSRect frame) {
  LayoutWindowWithAnchorView(objc_storage_->overlay_window,
                             objc_storage_->overlay_content_view);
}

bool ImmersiveModeController::IsTabbed() {
  return false;
}

double ImmersiveModeController::GetOffscreenYOrigin() {
  // Get the height of the screen. Using this as the y origin will move a window
  // offscreen.
  double y = objc_storage_->browser_window.screen.frame.size.height;

  // Make sure to make it past the safe area insets, otherwise some portion
  // of the window may still be displayed.
  if (@available(macOS 12.0, *)) {
    y += objc_storage_->browser_window.screen.safeAreaInsets.top;
  }

  return y;
}

void ImmersiveModeController::LayoutWindowWithAnchorView(NSWindow* window,
                                                         NSView* anchor_view) {
  // Find the anchor view's point on screen (bottom left).
  NSPoint point_in_window = [anchor_view convertPoint:NSZeroPoint toView:nil];
  NSPoint point_on_screen =
      [anchor_view.window convertPointToScreen:point_in_window];

  // This branch is only useful on macOS 11 and greater. macOS 10.15 and
  // earlier move the window instead of clipping the view within the window.
  // This allows the overlay window to appropriately track the overlay view.
  if (@available(macOS 11.0, *)) {
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
  }

  // If the toolbar is hidden (mojom::ToolbarVisibilityStyle::kNone) also move
  // the window offscreen. This applies to all versions of macOS where Chrome
  // can be run.
  if (last_used_style_ == mojom::ToolbarVisibilityStyle::kNone) {
    point_on_screen.y = GetOffscreenYOrigin();
  }

  [window setFrameOrigin:point_on_screen];
}

}  // namespace remote_cocoa
