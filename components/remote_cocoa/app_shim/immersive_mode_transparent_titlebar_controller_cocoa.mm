// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/immersive_mode_transparent_titlebar_controller_cocoa.h"

#include <algorithm>
#include <utility>

#import "components/remote_cocoa/app_shim/appkit_titlebar_private.h"
#import "components/remote_cocoa/app_shim/immersive_mode_controller_cocoa.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"

namespace {

// Walks up the superview chain of `view` until a view of class `class_name` is
// found.
NSView* FindAncestorOfClass(NSView* view, NSString* class_name) {
  Class cls = NSClassFromString(class_name);
  if (!cls) {
    return nil;
  }
  while (view && ![view isKindOfClass:cls]) {
    view = view.superview;
  }
  return view;
}

NSView* GetTitlebarContainerForWindow(NSWindow* window) {
  return FindAncestorOfClass([window standardWindowButton:NSWindowCloseButton],
                             @"NSTitlebarContainerView");
}

// Sets whether the titlebar hosted in `container` (an NSTitlebarContainerView)
// may be transparent in fullscreen, and makes the titlebar pick up the change.
void SetTransparencyAllowedInFullScreen(NSView* container, bool allowed) {
  if (![container
          respondsToSelector:@selector(setTransparencyAllowedInFullScreen:)]) {
    return;
  }
  if ([container
          respondsToSelector:@selector(transparencyAllowedInFullScreen)] &&
      container.transparencyAllowedInFullScreen == allowed) {
    return;
  }
  container.transparencyAllowedInFullScreen = allowed;
  for (NSView* subview in container.subviews) {
    if ([subview respondsToSelector:@selector(_updateSectionTransparency)]) {
      [subview _updateSectionTransparency];
    }
  }
}

}  // namespace

// The view of the titlebar accessory. It is empty and transparent, and it
// tells the controller when AppKit moves it to a different window.
@interface ImmersiveModeTransparentTitlebarAccessoryView : NSView
- (instancetype)initWithController:
    (base::WeakPtr<
        remote_cocoa::ImmersiveModeTransparentTitlebarControllerCocoa>)
        controller;
@end

@implementation ImmersiveModeTransparentTitlebarAccessoryView {
  base::WeakPtr<remote_cocoa::ImmersiveModeTransparentTitlebarControllerCocoa>
      _controller;
}

- (instancetype)initWithController:
    (base::WeakPtr<
        remote_cocoa::ImmersiveModeTransparentTitlebarControllerCocoa>)
        controller {
  if ((self = [super initWithFrame:NSZeroRect])) {
    _controller = std::move(controller);
  }
  return self;
}

- (BOOL)isOpaque {
  return NO;
}

// Never handle events. Top chrome, which is drawn in the browser window
// underneath this view, handles them.
- (NSView*)hitTest:(NSPoint)point {
  return nil;
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  if (_controller) {
    _controller->OnAccessoryViewMovedToWindow(self.window);
  }
}

@end

// An empty titlebar accessory that is placed below the titlebar. It makes the
// NSToolbarFullScreenWindow as tall as top chrome, and reports the steps of
// AppKit's reveal animation.
@interface ImmersiveModeTransparentTitlebarViewController
    : NSTitlebarAccessoryViewController
- (instancetype)initWithController:
    (base::WeakPtr<
        remote_cocoa::ImmersiveModeTransparentTitlebarControllerCocoa>)
        controller;
@end

@implementation ImmersiveModeTransparentTitlebarViewController {
  base::WeakPtr<remote_cocoa::ImmersiveModeTransparentTitlebarControllerCocoa>
      _controller;
  NSView* __strong _blankSeparatorView;
}

- (instancetype)initWithController:
    (base::WeakPtr<
        remote_cocoa::ImmersiveModeTransparentTitlebarControllerCocoa>)
        controller {
  if ((self = [super init])) {
    _controller = controller;
    _blankSeparatorView = [[NSView alloc] init];
    self.view = [[ImmersiveModeTransparentTitlebarAccessoryView alloc]
        initWithController:std::move(controller)];
    self.layoutAttribute = NSLayoutAttributeBottom;
    self.automaticallyAdjustsSize = NO;
    // Top chrome visibility (e.g. "Always Show Toolbar in Full Screen" and
    // reveal locks) is handled by the host. AppKit only needs to slide the
    // titlebar in and out.
    self.fullScreenMinHeight = 0;
  }
  return self;
}

// A private API method. Avoid a separator line between the titlebar and the
// accessory by returning a blank NSView.
- (NSView*)separatorView {
  return _blankSeparatorView;
}

- (void)setRevealAmount:(double)revealAmount {
  [super setRevealAmount:revealAmount];
  if (_controller) {
    _controller->OnTitlebarGeometryChanged();
  }
}

@end

namespace remote_cocoa {

ImmersiveModeTransparentTitlebarControllerCocoa::
    ImmersiveModeTransparentTitlebarControllerCocoa(
        NSWindow* browser_window,
        TitlebarFrameChangedCallback titlebar_frame_changed_callback)
    : browser_window_(browser_window),
      titlebar_frame_changed_callback_(
          std::move(titlebar_frame_changed_callback)),
      notification_observers_([[NSMutableArray alloc] init]) {
  accessory_view_controller_ =
      [[ImmersiveModeTransparentTitlebarViewController alloc]
          initWithController:weak_ptr_factory_.GetWeakPtr()];
}

ImmersiveModeTransparentTitlebarControllerCocoa::
    ~ImmersiveModeTransparentTitlebarControllerCocoa() {
  StopTrackingToolbarWindow();
  [accessory_view_controller_ removeFromParentViewController];
  if (initialized_) {
    browser_window_.titlebarSeparatorStyle = original_titlebar_separator_style_;
  }
}

void ImmersiveModeTransparentTitlebarControllerCocoa::Init() {
  DCHECK(!initialized_);
  initialized_ = true;

  // The flag may have been left on by a previous fullscreen session, which
  // would make AppKit use the opaque surrogate titlebar. See the class
  // comment.
  ResetTitlebarTransparency(browser_window_);

  // Avoid a separator line below the titlebar.
  original_titlebar_separator_style_ = browser_window_.titlebarSeparatorStyle;
  browser_window_.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;

  [browser_window_
      addTitlebarAccessoryViewController:accessory_view_controller_];
  UpdateAccessoryHeight();

  // The accessory may already be hosted in the NSToolbarFullScreenWindow if
  // the window is already in fullscreen.
  OnAccessoryViewMovedToWindow(accessory_view_controller_.view.window);
}

void ImmersiveModeTransparentTitlebarControllerCocoa::
    FullscreenTransitionCompleted() {
  fullscreen_transition_complete_ = true;
  OnAccessoryViewMovedToWindow(accessory_view_controller_.view.window);

  // This must happen after -[_NSEnterFullScreenTransitionController
  // doAfterEnterFullScreen] ran, see the class comment.
  UpdateTitlebarTransparency();

  // AppKit may have resized the accessory during the transition.
  UpdateAccessoryHeight();
  ReportTitlebarFrame();
}

void ImmersiveModeTransparentTitlebarControllerCocoa::OnTopViewBoundsChanged(
    const gfx::Rect& bounds) {
  if (bounds.IsEmpty()) {
    return;
  }
  top_container_height_ = bounds.height();
  UpdateAccessoryHeight();
}

void ImmersiveModeTransparentTitlebarControllerCocoa::UpdateToolbarVisibility(
    mojom::ToolbarVisibilityStyle style) {
  if (style_ == style) {
    return;
  }
  style_ = style;

  // In content fullscreen there is no top chrome. Only show the titlebar.
  accessory_view_controller_.hidden =
      style == mojom::ToolbarVisibilityStyle::kNone;
  UpdateTitlebarTransparency();
  ReportTitlebarFrame();
}

bool ImmersiveModeTransparentTitlebarControllerCocoa::IsContentFullscreen()
    const {
  return style_ == mojom::ToolbarVisibilityStyle::kNone;
}

void ImmersiveModeTransparentTitlebarControllerCocoa::
    OnAccessoryViewMovedToWindow(NSWindow* window) {
  if (!IsNSToolbarFullScreenWindow(window)) {
    return;
  }
  StartTrackingToolbarWindow(window);
  UpdateTitlebarTransparency();
}

void ImmersiveModeTransparentTitlebarControllerCocoa::
    OnTitlebarGeometryChanged() {
  ReportTitlebarFrame();
}

// static
void ImmersiveModeTransparentTitlebarControllerCocoa::ResetTitlebarTransparency(
    NSWindow* window) {
  SetTransparencyAllowedInFullScreen(GetTitlebarContainerForWindow(window),
                                     false);
}

void ImmersiveModeTransparentTitlebarControllerCocoa::
    StartTrackingToolbarWindow(NSWindow* toolbar_window) {
  NSView* titlebar_container = GetTitlebarContainer();
  if (toolbar_window == toolbar_window_ &&
      titlebar_container == titlebar_container_) {
    return;
  }
  StopTrackingToolbarWindow();
  toolbar_window_ = toolbar_window;
  titlebar_container_ = titlebar_container;

  // The NSToolbarFullScreenWindow only draws the window controls. Everything
  // else is transparent and lets top chrome in the browser window show
  // through.
  toolbar_window.opaque = NO;
  toolbar_window.backgroundColor = NSColor.clearColor;

  base::WeakPtr<ImmersiveModeTransparentTitlebarControllerCocoa> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  void (^geometry_changed)(NSNotification*) = ^(NSNotification*) {
    if (weak_this) {
      weak_this->OnTitlebarGeometryChanged();
    }
  };
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;

  // On screens without a camera housing (notch), the whole
  // NSToolbarFullScreenWindow moves down when the menu bar is revealed.
  for (NSNotificationName name in
       @[ NSWindowDidMoveNotification, NSWindowDidResizeNotification ]) {
    [notification_observers_
        addObject:[center addObserverForName:name
                                      object:toolbar_window
                                       queue:nil
                                  usingBlock:geometry_changed]];
  }

  // On screens with a notch the NSToolbarFullScreenWindow doesn't move.
  // Instead, AppKit slides the titlebar container inside of it.
  if (titlebar_container) {
    titlebar_container.postsFrameChangedNotifications = YES;
    [notification_observers_
        addObject:[center addObserverForName:NSViewFrameDidChangeNotification
                                      object:titlebar_container
                                       queue:nil
                                  usingBlock:geometry_changed]];
  }
}

void ImmersiveModeTransparentTitlebarControllerCocoa::
    StopTrackingToolbarWindow() {
  for (id<NSObject> observer in notification_observers_) {
    [NSNotificationCenter.defaultCenter removeObserver:observer];
  }
  [notification_observers_ removeAllObjects];
  toolbar_window_ = nil;
  titlebar_container_ = nil;
}

NSView* ImmersiveModeTransparentTitlebarControllerCocoa::GetTitlebarContainer()
    const {
  return GetTitlebarContainerForWindow(browser_window_);
}

CGFloat ImmersiveModeTransparentTitlebarControllerCocoa::GetTitlebarHeight()
    const {
  NSView* close_button =
      [browser_window_ standardWindowButton:NSWindowCloseButton];
  NSView* titlebar_view = FindAncestorOfClass(close_button, @"NSTitlebarView")
                              ?: close_button.superview;
  return titlebar_view ? NSHeight(titlebar_view.frame) : 0;
}

void ImmersiveModeTransparentTitlebarControllerCocoa::
    UpdateTitlebarTransparency() {
  // Never allow transparency before the fullscreen transition completed. See
  // the class comment.
  if (!fullscreen_transition_complete_ || !toolbar_window_) {
    return;
  }
  const bool transparent = !IsContentFullscreen();
  SetTransparencyAllowedInFullScreen(GetTitlebarContainer(), transparent);
}

void ImmersiveModeTransparentTitlebarControllerCocoa::UpdateAccessoryHeight() {
  NSView* accessory_view = accessory_view_controller_.view;
  const CGFloat height =
      std::max<CGFloat>(0, top_container_height_ - GetTitlebarHeight());
  if (NSHeight(accessory_view.frame) != height) {
    [accessory_view
        setFrameSize:NSMakeSize(NSWidth(accessory_view.frame), height)];
  }
}

void ImmersiveModeTransparentTitlebarControllerCocoa::ReportTitlebarFrame() {
  // Until the fullscreen transition completes, the titlebar container moves
  // between AppKit's transition windows. Top chrome stays fully visible in the
  // meantime.
  if (!fullscreen_transition_complete_) {
    return;
  }
  NSWindow* browser_window = browser_window_;
  NSView* titlebar_container = titlebar_container_;
  NSWindow* toolbar_window = titlebar_container.window;
  if (!browser_window || !toolbar_window ||
      !IsNSToolbarFullScreenWindow(toolbar_window)) {
    return;
  }

  // Convert the titlebar container frame to the browser window's content view
  // coordinates, with the origin at the top-left.
  NSRect frame = [titlebar_container convertRect:titlebar_container.bounds
                                          toView:nil];
  frame = [toolbar_window convertRectToScreen:frame];
  frame = [browser_window convertRectFromScreen:frame];
  NSView* content_view = browser_window.contentView;
  frame = [content_view convertRect:frame fromView:nil];
  if (!content_view.flipped) {
    frame.origin.y = NSHeight(content_view.bounds) - NSMaxY(frame);
  }
  const gfx::Rect titlebar_frame =
      gfx::ToRoundedRect(gfx::RectF(NSRectToCGRect(frame)));

  if (last_reported_titlebar_frame_ == titlebar_frame) {
    return;
  }
  last_reported_titlebar_frame_ = titlebar_frame;
  titlebar_frame_changed_callback_.Run(titlebar_frame);
}

}  // namespace remote_cocoa
