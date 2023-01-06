// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/immersive_mode_delegate_mac.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/gfx/geometry/rect.h"

namespace {

const double kThinControllerHeight = 0.5;

// TODO(https://crbug.com/1373552): use constraints / autoresizingmask instead
// of manually setting the frame size.
void PropagateFrameSizeToViewsSubviews(NSView* view) {
  for (NSView* sub_view in view.subviews) {
    if ([sub_view isKindOfClass:[BridgedContentView class]]) {
      [sub_view setFrameSize:view.frame.size];
    }
  }
}

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
  NSView* _overlay_view;
  BOOL _barrier;
  BOOL _titlebarFullyVisible;
}
@end

@implementation ImmersiveModeTitlebarObserver

- (instancetype)initWithController:
                    (base::WeakPtr<remote_cocoa::ImmersiveModeController>)
                        controller
                       overlayView:(NSView*)overlay_view {
  self = [super init];
  if (self) {
    _controller = std::move(controller);
    _overlay_view = overlay_view;
  }
  return self;
}

- (void)dealloc {
  NSView* view = GetNSTitlebarContainerViewFromWindow(_overlay_view.window);
  [view removeObserver:self forKeyPath:@"frame"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if (![keyPath isEqualToString:@"frame"]) {
    return;
  }

  NSRect frame = [change[@"new"] rectValue];
  _titlebarFullyVisible = frame.origin.y == 0;

  // Find the overlay view's point on screen (bottom left).
  NSPoint point_in_window = [_overlay_view convertPoint:NSZeroPoint toView:nil];
  NSPoint point_on_screen =
      [_overlay_view.window convertPointToScreen:point_in_window];

  BOOL overlay_view_is_clipped = NO;
  // This branch is only useful on macOS 11 and greater. macOS 10.15 and
  // earlier move the window instead of clipping the view within the window.
  // This allows the overlay window to appropriately track the overlay view.
  if (@available(macOS 11.0, *)) {
    // If the overlay view is clipped move the overlay window off screen. A
    // clipped overlay view indicates the titlebar is hidden or is in transition
    // AND the browser content view takes up the whole window ("Always Show
    // Toolbar in Full Screen" is disabled). When we are in this state we don't
    // want the overlay window on screen, otherwise it may mask input to the
    // browser view.
    // In all other cases will not enter this branch and the overlay
    // window will be placed at the same coordinates as the overlay view.
    if (_overlay_view.visibleRect.size.height !=
        _overlay_view.frame.size.height) {
      point_on_screen.y = -_overlay_view.frame.size.height;
      overlay_view_is_clipped = YES;
    }
  }

  if (!overlay_view_is_clipped) {
    // If there are sub-windows and the titlebar is fully visible (a y origin of
    // 0), pin the titlebar. This will prevent the titlebar from autohiding and
    // causing the sub-windows from moving up when the mouse leaves top chrome.
    if (!_barrier && _titlebarFullyVisible &&
        _controller->titlebar_lock_count() > 0) {
      // Add a barrier to prevent re-entry, which is a byproduct of
      // TitlebarLock() and TitlebarUnlock().
      base::AutoReset<BOOL> set_barrier(&_barrier, YES);
      // This lock / unlock scheme is to force the titlebar to be pinned in
      // place, which can only be done when the titlebar is fully visible.
      // Existing sub-windows hold a lock, however since the titlebar isn't
      // fully revealed until this point the existing locks don't actually pin
      // the titlebar. The existing locks are still important for knowing when
      // to unpin the titlebar. When all outstanding locks are released the
      // titlebar be unpinned.
      _controller->TitlebarLock();
      _controller->TitlebarUnlock();
    }
  }

  [_controller->overlay_window() setFrameOrigin:point_on_screen];
}

- (BOOL)titlebarFullyVisible {
  return _titlebarFullyVisible;
}

@end

// A stub NSWindowDelegate class that will be used to map the AppKit controlled
// NSWindow to the overlay view widget's NSWindow. The delegate will be used to
// help with input routing.
@interface ImmersiveModeMapper : NSObject <ImmersiveModeDelegate>
@property(assign) NSWindow* originalHostingWindow;
@end

@implementation ImmersiveModeMapper
@synthesize originalHostingWindow = _originalHostingWindow;
@end

// Host of the overlay view.
@interface ImmersiveModeTitlebarViewController
    : NSTitlebarAccessoryViewController {
  base::OnceClosure _view_will_appear_callback;
  base::scoped_nsobject<ImmersiveModeTitlebarObserver>
      _immersive_mode_titlebar_observer;
  NSWindow* _overlay_window;
}
@end

@implementation ImmersiveModeTitlebarViewController

- (instancetype)initWithOverlayWindow:(NSWindow*)overlay_window
               viewWillAppearCallback:
                   (base::OnceClosure)view_will_appear_callback {
  if ((self = [super init])) {
    _overlay_window = overlay_window;
    _view_will_appear_callback = std::move(view_will_appear_callback);
  }
  return self;
}

- (void)setTitlebarObserver:(ImmersiveModeTitlebarObserver*)observer {
  _immersive_mode_titlebar_observer.reset([observer retain]);
}

- (void)viewWillAppear {
  [super viewWillAppear];

  // Resize the views and run the callback on the first call to this method. We
  // will most likely be in the fullscreen transition window and we want our
  // views to be displayed.
  PropagateFrameSizeToViewsSubviews(self.view);
  if (!_view_will_appear_callback.is_null()) {
    // Triggers Views to display top chrome.
    std::move(_view_will_appear_callback).Run();
  }

  // AppKit hands this view controller over to a fullscreen transition window
  // before we finally land at the NSToolbarFullScreenWindow. Add the frame
  // observer only once we reach the NSToolbarFullScreenWindow.
  if (!remote_cocoa::IsNSToolbarFullScreenWindow(self.view.window)) {
    return;
  }

  // Attach overlay_widget to NSToolbarFullScreen so that children are placed on
  // top of the toolbar.
  // When exitting fullscreeen, we don't re-parent the overlay window back to
  // the browser window because it seems to trigger re-entrancy in AppKit and
  // cause crash.  This is safe because sub-widgets will be re-parented to the
  // browser window and therefore the overlay window won't have any observable
  // effect.
  [self.view.window addChildWindow:_overlay_window ordered:NSWindowAbove];

  NSView* view = GetNSTitlebarContainerViewFromWindow(self.view.window);
  DCHECK(view);
  [view addObserver:_immersive_mode_titlebar_observer
         forKeyPath:@"frame"
            options:NSKeyValueObservingOptionInitial |
                    NSKeyValueObservingOptionNew
            context:NULL];
}

- (BOOL)titlebarFullyVisible {
  return [_immersive_mode_titlebar_observer titlebarFullyVisible];
}

@end

@interface ClearTitlebarViewController : NSTitlebarAccessoryViewController {
  CGFloat _height;
}
@end

@implementation ClearTitlebarViewController

- (instancetype)initWithHeight:(CGFloat)height {
  self = [super init];
  if (self) {
    _height = height;
  }
  return self;
}

- (void)viewWillAppear {
  [super viewWillAppear];

  NSSize size = self.view.frame.size;
  size.height = _height;
  [self.view setFrameSize:size];

  // Hide the controller before it is appears but after the view's frame is
  // set. This will extend the NSTitlebarAccessoryViewController mouse
  // tracking area over the entirety of the window stopping the titlebar from
  // auto hiding.
  self.hidden = YES;
}

@end

// An NSView that will set the ImmersiveModeDelegate on the AppKit created
// window that ends up hosting this view via the
// NSTitlebarAccessoryViewController API.
@interface ImmersiveModeView : NSView
- (instancetype)initWithImmersiveModeDelegate:
    (id<ImmersiveModeDelegate>)delegate;
@end

@implementation ImmersiveModeView {
  ImmersiveModeMapper* _fullscreenDelegate;
}

- (instancetype)initWithImmersiveModeDelegate:
    (id<ImmersiveModeDelegate>)delegate {
  self = [super init];
  if (self) {
    _fullscreenDelegate = delegate;
  }
  return self;
}

- (void)viewWillMoveToWindow:(NSWindow*)window {
  if (remote_cocoa::IsNSToolbarFullScreenWindow(window)) {
    // This window is created by AppKit. Make sure it doesn't have a delegate
    // so we can use it for out own purposes.
    DCHECK(!window.delegate);
    window.delegate = _fullscreenDelegate;
  }
}

@end

@interface ImmersiveModeWindowObserver : NSObject {
  base::WeakPtr<remote_cocoa::ImmersiveModeController> _controller;
}
- (instancetype)initWithController:
    (base::WeakPtr<remote_cocoa::ImmersiveModeController>)controller;
@end

@implementation ImmersiveModeWindowObserver

- (instancetype)initWithController:
    (base::WeakPtr<remote_cocoa::ImmersiveModeController>)controller {
  self = [super init];
  if (self) {
    _controller = std::move(controller);
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if (![keyPath isEqualToString:@"visible"]) {
    return;
  }

  BOOL visible = [change[NSKeyValueChangeNewKey] boolValue];
  NSWindow* window = base::mac::ObjCCastStrict<NSWindow>(object);
  if (visible) {
    if (_controller) {
      _controller->TitlebarLock();
    }
    return;
  }

  // Assume not-visible is a terminal state for an overlay child window. Also
  // assume child windows will become not-visible before self is destroyed.
  // These assumptions makes adding and removing the visible observer trival.
  [window removeObserver:self forKeyPath:@"visible"];
  if (_controller) {
    _controller->TitlebarUnlock();
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

ImmersiveModeController::ImmersiveModeController(NSWindow* browser_widget,
                                                 NSWindow* overlay_widget,
                                                 base::OnceClosure callback)
    : browser_window_(browser_widget),
      overlay_window_(overlay_widget),
      weak_ptr_factory_(this) {
  immersive_mode_window_observer_.reset([[ImmersiveModeWindowObserver alloc]
      initWithController:weak_ptr_factory_.GetWeakPtr()]);

  // A style of NSTitlebarSeparatorStyleAutomatic (default) will show a black
  // line separator when removing the NSWindowStyleMaskFullSizeContentView style
  // bit. We do not want a separator. Pre-macOS 11 there is no titlebar
  // separator.
  if (@available(macOS 11.0, *)) {
    browser_window_.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
  }

  // Create a new NSTitlebarAccessoryViewController that will host the
  // overlay_view_.
  immersive_mode_titlebar_view_controller_.reset(
      [[ImmersiveModeTitlebarViewController alloc]
           initWithOverlayWindow:overlay_window_
          viewWillAppearCallback:std::move(callback)]);

  // Create a NSWindow delegate that will be used to map the AppKit created
  // NSWindow to the overlay view widget's NSWindow.
  immersive_mode_mapper_.reset([[ImmersiveModeMapper alloc] init]);
  immersive_mode_mapper_.get().originalHostingWindow = overlay_window_;
  immersive_mode_titlebar_view_controller_.get().view =
      [[ImmersiveModeView alloc]
          initWithImmersiveModeDelegate:immersive_mode_mapper_.get()];

  // Remove the content view from the overlay view widget's NSWindow. This
  // view will be re-parented into the AppKit created NSWindow.
  BridgedContentView* overlay_content_view =
      base::mac::ObjCCastStrict<BridgedContentView>(
          overlay_window_.contentView);
  [overlay_content_view retain];
  [overlay_content_view removeFromSuperview];

  // Add the titlebar observer to the controller. Observing can only start
  // once the controller has been fully re-parented into the AppKit fullscreen
  // window.
  ImmersiveModeTitlebarObserver* titlebar_observer =
      [[[ImmersiveModeTitlebarObserver alloc]
          initWithController:weak_ptr_factory_.GetWeakPtr()
                 overlayView:overlay_content_view] autorelease];
  [immersive_mode_titlebar_view_controller_
      setTitlebarObserver:titlebar_observer];

  // The original content view (top chrome) has been moved to the AppKit
  // created NSWindow. Create a new content view but reuse the original bridge
  // so that mouse drags are handled.
  overlay_window_.contentView =
      [[[BridgedContentView alloc] initWithBridge:overlay_content_view.bridge
                                           bounds:gfx::Rect()] autorelease];

  // Add the overlay view to the accessory view controller getting ready to
  // hand everything over to AppKit.
  [immersive_mode_titlebar_view_controller_.get().view
      addSubview:overlay_content_view];
  [overlay_content_view release];
  immersive_mode_titlebar_view_controller_.get().layoutAttribute =
      NSLayoutAttributeBottom;

  thin_titlebar_view_controller_.reset(
      [[NSTitlebarAccessoryViewController alloc] init]);
  thin_titlebar_view_controller_.get().view =
      [[[NSView alloc] init] autorelease];
  thin_titlebar_view_controller_.get().view.wantsLayer = YES;
  thin_titlebar_view_controller_.get().view.layer.backgroundColor =
      NSColor.blackColor.CGColor;
  thin_titlebar_view_controller_.get().layoutAttribute =
      NSLayoutAttributeBottom;
  thin_titlebar_view_controller_.get().fullScreenMinHeight =
      kThinControllerHeight;

  // Move sub-widgets from the browser widget to the overlay widget so that
  // they are rendered above the toolbar.
  ObserveOverlayChildWindows();
  ReparentChildWindows(browser_window_, overlay_window_);
}

ImmersiveModeController::~ImmersiveModeController() {
  // Remove the titlebar observer before moving the view.
  [immersive_mode_titlebar_view_controller_ setTitlebarObserver:nil];

  // Rollback the view shuffling from enablement.
  [thin_titlebar_view_controller_ removeFromParentViewController];
  NSView* overlay_content_view =
      immersive_mode_titlebar_view_controller_.get().view.subviews.firstObject;
  [overlay_content_view removeFromSuperview];
  overlay_window_.contentView = overlay_content_view;
  [immersive_mode_titlebar_view_controller_ removeFromParentViewController];
  [immersive_mode_titlebar_view_controller_.get().view release];
  immersive_mode_titlebar_view_controller_.reset();
  browser_window_.styleMask |= NSWindowStyleMaskFullSizeContentView;
  if (@available(macOS 11.0, *)) {
    browser_window_.titlebarSeparatorStyle = NSTitlebarSeparatorStyleAutomatic;
  }

  // Move sub-widgets back to the browser widget.
  ReparentChildWindows(overlay_window_, browser_window_);
}

void ImmersiveModeController::Enable() {
  DCHECK(!enabled_);
  enabled_ = true;
  [browser_window_ addTitlebarAccessoryViewController:
                       immersive_mode_titlebar_view_controller_];
  [browser_window_
      addTitlebarAccessoryViewController:thin_titlebar_view_controller_];
  NSRect frame = thin_titlebar_view_controller_.get().view.frame;
  frame.size.height = kThinControllerHeight;
  thin_titlebar_view_controller_.get().view.frame = frame;
}

void ImmersiveModeController::OnTopViewBoundsChanged(const gfx::Rect& bounds) {
  // Set the height of the AppKit fullscreen view. The width will be
  // automatically handled by AppKit.
  NSRect frame = NSRectFromCGRect(bounds.ToCGRect());
  NSView* overlay_view = immersive_mode_titlebar_view_controller_.get().view;
  NSSize size = overlay_view.frame.size;
  size.height = frame.size.height;
  [overlay_view setFrameSize:size];
  PropagateFrameSizeToViewsSubviews(overlay_view);
  UpdateToolbarVisibility(last_used_style_);

  // If the toolbar is always visible, update the fullscreen min height.
  // Also update the fullscreen min height if the toolbar auto hides, but only
  // if the toolbar is currently revealed.
  if (last_used_style_ == mojom::ToolbarVisibilityStyle::kAlways ||
      (last_used_style_ == mojom::ToolbarVisibilityStyle::kAutohide &&
       reveal_lock_count_ > 0)) {
    immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight =
        immersive_mode_titlebar_view_controller_.get().view.frame.size.height;
  }
}

void ImmersiveModeController::UpdateToolbarVisibility(
    mojom::ToolbarVisibilityStyle style) {
  // Remember the last used style for internal use of UpdateToolbarVisibility.
  last_used_style_ = style;

  // Only make changes if there are no outstanding reveal locks.
  if (titlebar_lock_count_ > 0 || reveal_lock_count_ > 0) {
    return;
  }

  switch (style) {
    case mojom::ToolbarVisibilityStyle::kAlways:
      immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight =
          immersive_mode_titlebar_view_controller_.get().view.frame.size.height;
      thin_titlebar_view_controller_.get().hidden = YES;
      browser_window_.styleMask &= ~NSWindowStyleMaskFullSizeContentView;

      // Toggling the controller will allow the content view to resize below Top
      // Chrome.
      immersive_mode_titlebar_view_controller_.get().hidden = YES;
      immersive_mode_titlebar_view_controller_.get().hidden = NO;
      break;
    case mojom::ToolbarVisibilityStyle::kAutohide:
      immersive_mode_titlebar_view_controller_.get().hidden = NO;

      // The thin titlebar controller keeps a tiny portion of the AppKit
      // fullscreen NSWindow on screen as a workaround for
      // https://crbug.com/1369643.
      thin_titlebar_view_controller_.get().hidden = NO;

      immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight = 0;
      browser_window_.styleMask |= NSWindowStyleMaskFullSizeContentView;
      break;
    case mojom::ToolbarVisibilityStyle::kNone:
      thin_titlebar_view_controller_.get().hidden = YES;
      immersive_mode_titlebar_view_controller_.get().hidden = YES;
      break;
  }

  // Unpin the titlebar.
  SetTitlebarPinned(false);
}

// This function will pin or unpin the titlebar (holder of the traffic
// lights). When the titlebar is pinned the titlebar will stay present on
// screen even if the mouse leaves the titlebar or Toolbar area. This is
// helpful when displaying sub-widgets. When the titlebar is not pinned it
// will reveal and auto-hide itself based on mouse movement (controlled by
// AppKit).
void ImmersiveModeController::SetTitlebarPinned(bool pinned) {
  // Remove current, if any, clear controllers from the window. For some reason
  // -removeFromParentViewController does not always remove the controller.
  // Attempt to remove the current and any stale controllers.
  for (NSTitlebarAccessoryViewController* c in browser_window_
           .titlebarAccessoryViewControllers) {
    if ([c isKindOfClass:[ClearTitlebarViewController class]]) {
      [c removeFromParentViewController];
    }
  }

  if (!pinned) {
    clear_titlebar_view_controller_.reset();
    return;
  }

  clear_titlebar_view_controller_.reset([[ClearTitlebarViewController alloc]
      initWithHeight:browser_window_.contentView.frame.size.height -
                     kThinControllerHeight]);
  clear_titlebar_view_controller_.get().view =
      [[[NSView alloc] init] autorelease];
  clear_titlebar_view_controller_.get().layoutAttribute =
      NSLayoutAttributeBottom;
  [browser_window_
      addTitlebarAccessoryViewController:clear_titlebar_view_controller_];
}

void ImmersiveModeController::ObserveOverlayChildWindows() {
  // Watch the overlay Widget for new child Widgets.
  auto observe_window = [this](NSWindow* window) {
    [window addObserver:immersive_mode_window_observer_
             forKeyPath:@"visible"
                options:NSKeyValueObservingOptionInitial |
                        NSKeyValueObservingOptionNew
                context:nullptr];
  };
  NativeWidgetMacNSWindow* overlay_window =
      base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(overlay_window_);
  overlay_window.childWindowAddedHandler = ^(NSWindow* child) {
    // Ignore non-visible children.
    if (!child.visible) {
      return;
    }
    observe_window(child);
  };
}

void ImmersiveModeController::ReparentChildWindows(NSWindow* source,
                                                   NSWindow* target) {
  NativeWidgetNSWindowBridge* source_bridge =
      NativeWidgetNSWindowBridge::GetFromNativeWindow(source);
  NativeWidgetNSWindowBridge* target_bridge =
      NativeWidgetNSWindowBridge::GetFromNativeWindow(target);

  // TODO(kerenzhu): DCHECK(source_bridge && target_bridge)
  // Only in unittests the associated bridges might not exist.
  if (source_bridge && target_bridge) {
    source_bridge->MoveChildrenTo(target_bridge);
  }
}

void ImmersiveModeController::TitlebarLock() {
  titlebar_lock_count_++;
  if (titlebar_fully_visible_for_testing_ ||
      [immersive_mode_titlebar_view_controller_ titlebarFullyVisible]) {
    SetTitlebarPinned(true);
  }
}

void ImmersiveModeController::TitlebarUnlock() {
  if (--titlebar_lock_count_ < 1) {
    SetTitlebarPinned(false);
  }
  DCHECK(titlebar_lock_count_ >= 0);
}

void ImmersiveModeController::RevealLock() {
  reveal_lock_count_++;
  immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight =
      immersive_mode_titlebar_view_controller_.get().view.frame.size.height;
}

void ImmersiveModeController::RevealUnlock() {
  // Re-hide the toolbar if appropriate.
  if (--reveal_lock_count_ < 1 &&
      immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight > 0 &&
      last_used_style_ == mojom::ToolbarVisibilityStyle::kAutohide) {
    immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight = 0;
  }

  // Account for last_used_style_ changing to kAlways while a reveal lock was
  // active.
  if (reveal_lock_count_ < 1 &&
      last_used_style_ == mojom::ToolbarVisibilityStyle::kAlways) {
    UpdateToolbarVisibility(last_used_style_);
  }
  DCHECK(reveal_lock_count_ >= 0);
}

}  // namespace remote_cocoa
