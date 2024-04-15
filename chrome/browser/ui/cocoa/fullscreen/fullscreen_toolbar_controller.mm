// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_controller.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_menubar_tracker.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_animation_controller.h"
#import "chrome/browser/ui/cocoa/fullscreen/fullscreen_toolbar_mouse_tracker.h"
#import "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"

@implementation FullscreenToolbarController {
  // Whether or not we are in fullscreen mode.
  BOOL _inFullscreenMode;

  // Updates the fullscreen toolbar layout for changes in the menubar. This
  // object is only set when the browser is in fullscreen mode.
  FullscreenMenubarTracker* __strong _menubarTracker;

  // Manages the toolbar animations for the TOOLBAR_HIDDEN style.
  std::unique_ptr<FullscreenToolbarAnimationController> _animationController;

  // When the menu bar and toolbar are visible, creates a tracking area which
  // is used to keep them visible until the mouse moves far enough away from
  // them. Only set when the browser is in fullscreen mode.
  FullscreenToolbarMouseTracker* __strong _mouseTracker;

  // The style of the fullscreen toolbar.
  FullscreenToolbarStyle _toolbarStyle;

  raw_ptr<BrowserView> _browserView;  // weak
}

- (instancetype)initWithBrowserView:(BrowserView*)browserView {
  if ((self = [super init])) {
    _browserView = browserView;
    _animationController =
        std::make_unique<FullscreenToolbarAnimationController>(self);
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_inFullscreenMode);
}

- (void)enterFullscreenMode {
  if (_inFullscreenMode)
    return;
  _inFullscreenMode = YES;

  _menubarTracker = [[FullscreenMenubarTracker alloc]
      initWithFullscreenToolbarController:self];
  _mouseTracker = [[FullscreenToolbarMouseTracker alloc]
      initWithFullscreenToolbarController:self];
}

- (void)exitFullscreenMode {
  if (!_inFullscreenMode)
    return;
  _inFullscreenMode = NO;

  _animationController->StopAnimationAndTimer();
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  _menubarTracker = nil;
  _mouseTracker = nil;
}

- (void)revealToolbarForWebContents:(content::WebContents*)contents
                       inForeground:(BOOL)inForeground {
  _animationController->AnimateToolbarForTabstripChanges(contents,
                                                         inForeground);
}

- (CGFloat)toolbarFraction {
  // Visibility fractions for the menubar and toolbar.
  constexpr CGFloat kHideFraction = 0.0;
  constexpr CGFloat kShowFraction = 1.0;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode))
    return kHideFraction;

  switch (_toolbarStyle) {
    case FullscreenToolbarStyle::TOOLBAR_PRESENT:
      return kShowFraction;
    case FullscreenToolbarStyle::TOOLBAR_NONE:
      return kHideFraction;
    case FullscreenToolbarStyle::TOOLBAR_HIDDEN:
      if (_animationController->IsAnimationRunning())
        return _animationController->GetToolbarFractionFromProgress();

      if ([self mustShowFullscreenToolbar])
        return kShowFraction;

      return [_menubarTracker menubarFraction];
  }
}

- (FullscreenToolbarStyle)toolbarStyle {
  return _toolbarStyle;
}

- (BOOL)mustShowFullscreenToolbar {
  if (!_inFullscreenMode)
    return NO;

  if (_toolbarStyle == FullscreenToolbarStyle::TOOLBAR_PRESENT)
    return YES;

  if (_toolbarStyle == FullscreenToolbarStyle::TOOLBAR_NONE)
    return NO;

  return [_menubarTracker state] == FullscreenMenubarState::SHOWN;
}

- (void)updateToolbarFrame:(NSRect)frame {
  if (_mouseTracker) {
    [_mouseTracker updateToolbarFrame:frame];
  }
}

- (void)layoutToolbar {
  _browserView->DeprecatedLayoutImmediately();
  _animationController->ToolbarDidUpdate();
  [_mouseTracker updateTrackingArea];
}

- (BOOL)isInFullscreen {
  return _inFullscreenMode;
}

- (FullscreenMenubarTracker*)menubarTracker {
  return _menubarTracker;
}

- (void)setToolbarStyle:(FullscreenToolbarStyle)style {
  _toolbarStyle = style;
}

- (BOOL)isInAnyFullscreenMode {
  return _browserView->IsFullscreen();
}

- (BOOL)isFullscreenTransitionInProgress {
  auto* host =
      views::NativeWidgetMacNSWindowHost::GetFromNativeWindow([self window]);
  if (auto* bridge = host->GetInProcessNSWindowBridge())
    return bridge->in_fullscreen_transition();
  DLOG(ERROR) << "TODO(crbug.com/41431787): Support fullscreen "
                 "transitions for RemoteMacViews PWA windows.";
  return false;
}

- (NSWindow*)window {
  return _browserView->GetNativeWindow().GetNativeNSWindow();
}

@end
