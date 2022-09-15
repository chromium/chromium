// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/app_shim/immersive_mode_controller.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "components/remote_cocoa/app_shim/bridged_content_view.h"
#include "components/remote_cocoa/app_shim/immersive_mode_delegate_mac.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

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
  base::OnceCallback<void()> _view_will_appear_callback;
}
@end

@implementation ImmersiveModeTitlebarViewController

- (instancetype)initWithViewWillAppearCallback:
    (base::OnceCallback<void()>)viewWillAppearCallback {
  if ((self = [super init])) {
    _view_will_appear_callback = std::move(viewWillAppearCallback);
  }
  return self;
}

- (void)viewWillAppear {
  [super viewWillAppear];

  // TODO(bur): Get the updated width from OnViewBoundsChanged
  NSView* tab_view = self.view;
  NSRect f = tab_view.frame;
  f.size.width = 2400;
  tab_view.frame = f;
  for (NSView* view in tab_view.subviews) {
    if ([view isKindOfClass:[BridgedContentView class]]) {
      view.frame = tab_view.frame;
    }
  }

  if (!_view_will_appear_callback.is_null()) {
    std::move(_view_will_appear_callback).Run();
  }
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
    // This window is created by AppKit. Make sure it doesn't have a delegate so
    // we can use it for out own purposes.
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

ImmersiveModeController::ImmersiveModeController(
    NSWindow* browser_widget,
    NSWindow* overlay_widget,
    base::OnceCallback<void()> callback)
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
  NSView* overlay_content_view = overlay_widget_.contentView;
  [overlay_content_view removeFromSuperview];

  // Add the overlay view to the accessory view controller and hand everything
  // over to AppKit.
  [immersive_mode_titlebar_view_controller_.get().view
      addSubview:overlay_content_view];
  immersive_mode_titlebar_view_controller_.get().layoutAttribute =
      NSLayoutAttributeBottom;

  ObserveOverlayChildWindows();
}

ImmersiveModeController::~ImmersiveModeController() {
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
  [immersive_mode_titlebar_view_controller_.get().view
      setFrameSize:bounds.size().ToCGSize()];
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
  } else {
    immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight = 0;
    browser_widget_.styleMask |= NSWindowStyleMaskFullSizeContentView;
  }
}

// This function will pin or unpin the Title Bar (holder of the traffic
// lights). When the Title Bar is pinned the Title Bar will stay present on
// screen even if the mouse leaves the Title Bar or Toolbar area. This is
// helpful when displaying sub-widgets. When the Title Bar is not pinned it will
// reveal and auto-hide itself based on mouse movement (controlled by AppKit).
void ImmersiveModeController::SetTitleBarPinned(bool pinned) {
  // TODO(bur): Provide a mechanism to acomplish Title Bar pinning that looks
  // and feels native. The use of a NSToolbar is an option but undesirable
  // because of the +10 pixel height it adds to the blank Title Bar.
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
  immersive_mode_titlebar_view_controller_.get().fullScreenMinHeight =
      immersive_mode_titlebar_view_controller_.get().view.frame.size.height;
  SetTitleBarPinned(true);
}

void ImmersiveModeController::RevealUnlock() {
  if (--revealed_lock_count_ < 1) {
    SetTitleBarPinned(false);
    UpdateToolbarVisibility(always_show_toolbar_);
  }
}

}  // namespace remote_cocoa
