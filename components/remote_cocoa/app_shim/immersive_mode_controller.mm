// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/immersive_mode_delegate_mac.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "ui/gfx/geometry/rect.h"

namespace {

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

@implementation ImmersiveModeTitlebarObserver

- (instancetype)initWithOverlayWindow:(NSWindow*)overlay_window
                          overlayView:(NSView*)overlay_view {
  self = [super init];
  if (self) {
    _overlay_window = overlay_window;
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

  // Find the overlay view's point on screen (bottom left).
  NSPoint point_in_window = [_overlay_view convertPoint:NSZeroPoint toView:nil];
  NSPoint point_on_screen =
      [_overlay_view.window convertPointToScreen:point_in_window];

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
  }

  [_overlay_window setFrameOrigin:point_on_screen];
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
}
@end

@implementation ImmersiveModeTitlebarViewController

- (instancetype)initWithViewWillAppearCallback:
    (base::OnceClosure)viewWillAppearCallback {
  if ((self = [super init])) {
    _view_will_appear_callback = std::move(viewWillAppearCallback);
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

  NSView* view = GetNSTitlebarContainerViewFromWindow(self.view.window);
  DCHECK(view);
  [view addObserver:_immersive_mode_titlebar_observer
         forKeyPath:@"frame"
            options:NSKeyValueObservingOptionInitial |
                    NSKeyValueObservingOptionNew
            context:NULL];
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
      _controller->RevealLock();
    }
    return;
  }

  // Assume not-visible is a terminal state for an overlay child window. Also
  // assume child windows will become not-visible before self is destroyed.
  // These assumptions makes adding and removing the visible observer trival.
  [window removeObserver:self forKeyPath:@"visible"];
  if (_controller) {
    _controller->RevealUnlock();
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
    : browser_widget_(browser_widget),
      overlay_widget_(overlay_widget),
      weak_ptr_factory_(this) {
  immersive_mode_window_observer_.reset([[ImmersiveModeWindowObserver alloc]
      initWithController:weak_ptr_factory_.GetWeakPtr()]);

  // Create a new NSTitlebarAccessoryViewController that will host the
  // overlay_view_.
  immersive_mode_titlebar_view_controller_.reset(
      [[ImmersiveModeTitlebarViewController alloc]
          initWithViewWillAppearCallback:std::move(callback)]);

  // Create a NSWindow delegate that will be used to map the AppKit created
  // NSWindow to the overlay view widget's NSWindow.
  immersive_mode_mapper_.reset([[ImmersiveModeMapper alloc] init]);
  immersive_mode_mapper_.get().originalHostingWindow = overlay_widget_;
  immersive_mode_titlebar_view_controller_.get().view =
      [[ImmersiveModeView alloc]
          initWithImmersiveModeDelegate:immersive_mode_mapper_.get()];

  // Remove the content view from the overlay view widget's NSWindow. This
  // view will be re-parented into the AppKit created NSWindow.
  BridgedContentView* overlay_content_view =
      base::mac::ObjCCastStrict<BridgedContentView>(
          overlay_widget_.contentView);
  [overlay_content_view retain];
  [overlay_content_view removeFromSuperview];

  // Add the titlebar observer to the controller. Observing can only start
  // once the controller has been fully re-parented into the AppKit fullscreen
  // window.
  ImmersiveModeTitlebarObserver* titlebar_observer =
      [[[ImmersiveModeTitlebarObserver alloc]
          initWithOverlayWindow:overlay_widget_
                    overlayView:overlay_content_view] autorelease];
  [immersive_mode_titlebar_view_controller_
      setTitlebarObserver:titlebar_observer];

  // The original content view (top chrome) has been moved to the AppKit
  // created NSWindow. Create a new content view but reuse the original bridge
  // so that mouse drags are handled.
  overlay_widget_.contentView =
      [[[BridgedContentView alloc] initWithBridge:overlay_content_view.bridge
                                           bounds:gfx::Rect()] autorelease];

  // Add the overlay view to the accessory view controller getting ready to
  // hand everything over to AppKit.
  [immersive_mode_titlebar_view_controller_.get().view
      addSubview:overlay_content_view];
  [overlay_content_view release];
  immersive_mode_titlebar_view_controller_.get().layoutAttribute =
      NSLayoutAttributeBottom;

  ObserveOverlayChildWindows();
}

ImmersiveModeController::~ImmersiveModeController() {
  // Remove the titlebar observer before moving the view.
  [immersive_mode_titlebar_view_controller_ setTitlebarObserver:nil];

  // Rollback the view shuffling from enablement.
  NSView* overlay_content_view =
      immersive_mode_titlebar_view_controller_.get().view.subviews.firstObject;
  [overlay_content_view removeFromSuperview];
  overlay_widget_.contentView = overlay_content_view;
  [immersive_mode_titlebar_view_controller_ removeFromParentViewController];
  [immersive_mode_titlebar_view_controller_.get().view release];
  immersive_mode_titlebar_view_controller_.reset();
  browser_widget_.styleMask |= NSWindowStyleMaskFullSizeContentView;
}

void ImmersiveModeController::Enable() {
  DCHECK(!enabled_);
  enabled_ = true;
  [browser_widget_ addTitlebarAccessoryViewController:
                       immersive_mode_titlebar_view_controller_];
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
}

void ImmersiveModeController::UpdateToolbarVisibility(bool always_show) {
  // Remember the last used always_show for internal use of
  // UpdateToolbarVisibility.
  always_show_toolbar_ = always_show;

  // Only make changes if there are no outstanding reveal locks.
  if (revealed_lock_count_ > 0) {
    return;
  }

  if (always_show) {
    immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight =
        immersive_mode_titlebar_view_controller_.get().view.frame.size.height;
    browser_widget_.styleMask &= ~NSWindowStyleMaskFullSizeContentView;

    // Toggling the controller will allow the content view to resize below Top
    // Chrome.
    immersive_mode_titlebar_view_controller_.get().hidden = YES;
    immersive_mode_titlebar_view_controller_.get().hidden = NO;
  } else {
    immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight = 0;
    browser_widget_.styleMask |= NSWindowStyleMaskFullSizeContentView;
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
  // Remove the current, if any, clear controller from the window.
  [clear_titlebar_view_controller_.get() removeFromParentViewController];

  if (!pinned) {
    clear_titlebar_view_controller_.reset();
    return;
  }

  clear_titlebar_view_controller_.reset([[ClearTitlebarViewController alloc]
      initWithHeight:browser_widget_.contentView.frame.size.height]);
  clear_titlebar_view_controller_.get().view =
      [[[NSView alloc] init] autorelease];
  clear_titlebar_view_controller_.get().layoutAttribute =
      NSLayoutAttributeBottom;
  [browser_widget_
      addTitlebarAccessoryViewController:clear_titlebar_view_controller_];
}

void ImmersiveModeController::ObserveOverlayChildWindows() {
  // Watch the overlay Widget for new child Widgets.
  NativeWidgetMacNSWindow* overlay_window =
      base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(overlay_widget_);
  overlay_window.childWindowAddedHandler = ^(NSWindow* child) {
    // Ignore non-visible children.
    if (!child.visible) {
      return;
    }
    [child addObserver:immersive_mode_window_observer_
            forKeyPath:@"visible"
               options:NSKeyValueObservingOptionInitial |
                       NSKeyValueObservingOptionNew
               context:nullptr];
  };
}

void ImmersiveModeController::RevealLock() {
  revealed_lock_count_++;
  SetTitlebarPinned(true);
}

void ImmersiveModeController::RevealUnlock() {
  if (--revealed_lock_count_ < 1) {
    SetTitlebarPinned(false);
  }
}

}  // namespace remote_cocoa
