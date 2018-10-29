// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/apps/chrome_native_app_window_views_mac.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#import "chrome/browser/ui/views/apps/app_window_native_widget_mac.h"
#import "chrome/browser/ui/views/apps/native_app_window_frame_view_mac.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// This observer is used to get NSWindow notifications. We need to monitor
// zoom and full screen events to store the correct bounds to Restore() to.
@interface ResizeNotificationObserver : NSObject {
 @private
  // Weak. Owns us.
  ChromeNativeAppWindowViewsMac* nativeAppWindow_;
}
- (id)initForNativeAppWindow:(ChromeNativeAppWindowViewsMac*)nativeAppWindow;
- (void)onWindowWillStartLiveResize:(NSNotification*)notification;
- (void)onWindowWillExitFullScreen:(NSNotification*)notification;
- (void)onWindowDidExitFullScreen:(NSNotification*)notification;
- (void)stopObserving;
@end

@implementation ResizeNotificationObserver

- (id)initForNativeAppWindow:(ChromeNativeAppWindowViewsMac*)nativeAppWindow {
  if ((self = [super init])) {
    nativeAppWindow_ = nativeAppWindow;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowWillStartLiveResize:)
               name:NSWindowWillStartLiveResizeNotification
             object:static_cast<ui::BaseWindow*>(nativeAppWindow)
                        ->GetNativeWindow()
                        .GetNativeNSWindow()];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowWillExitFullScreen:)
               name:NSWindowWillExitFullScreenNotification
             object:static_cast<ui::BaseWindow*>(nativeAppWindow)
                        ->GetNativeWindow()
                        .GetNativeNSWindow()];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onWindowDidExitFullScreen:)
               name:NSWindowDidExitFullScreenNotification
             object:static_cast<ui::BaseWindow*>(nativeAppWindow)
                        ->GetNativeWindow()
                        .GetNativeNSWindow()];
  }
  return self;
}

- (void)onWindowWillStartLiveResize:(NSNotification*)notification {
  nativeAppWindow_->OnWindowWillStartLiveResize();
}

- (void)onWindowWillExitFullScreen:(NSNotification*)notification {
  nativeAppWindow_->OnWindowWillExitFullScreen();
}

- (void)onWindowDidExitFullScreen:(NSNotification*)notification {
  nativeAppWindow_->OnWindowDidExitFullScreen();
}

- (void)stopObserving {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  nativeAppWindow_ = nullptr;
}

@end

namespace {

bool NSWindowIsMaximized(NSWindow* window) {
  // -[NSWindow isZoomed] only works if the zoom button is enabled.
  if ([[window standardWindowButton:NSWindowZoomButton] isEnabled])
    return [window isZoomed];

  // We don't attempt to distinguish between a window that has been explicitly
  // maximized versus one that has just been dragged by the user to fill the
  // screen. This is the same behavior as -[NSWindow isZoomed] above.
  return NSEqualRects([window frame], [[window screen] visibleFrame]);
}

}  // namespace

ChromeNativeAppWindowViewsMac::ChromeNativeAppWindowViewsMac() {}

ChromeNativeAppWindowViewsMac::~ChromeNativeAppWindowViewsMac() {
  [nswindow_observer_ stopObserving];
}

void ChromeNativeAppWindowViewsMac::OnWindowWillStartLiveResize() {
  if (!NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow()) &&
      !in_fullscreen_transition_) {
    bounds_before_maximize_ = [GetNativeWindow().GetNativeNSWindow() frame];
  }
}

void ChromeNativeAppWindowViewsMac::OnWindowWillExitFullScreen() {
  in_fullscreen_transition_ = true;
}

void ChromeNativeAppWindowViewsMac::OnWindowDidExitFullScreen() {
  in_fullscreen_transition_ = false;
}

void ChromeNativeAppWindowViewsMac::OnBeforeWidgetInit(
    const extensions::AppWindow::CreateParams& create_params,
    views::Widget::InitParams* init_params,
    views::Widget* widget) {
  DCHECK(!init_params->native_widget);
  init_params->remove_standard_frame = IsFrameless();
  init_params->native_widget = new AppWindowNativeWidgetMac(widget, this);
  ChromeNativeAppWindowViews::OnBeforeWidgetInit(create_params, init_params,
                                                 widget);
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsMac::CreateStandardDesktopAppFrame() {
  return new NativeAppWindowFrameViewMac(widget(), this);
}

views::NonClientFrameView*
ChromeNativeAppWindowViewsMac::CreateNonStandardAppFrame() {
  return new NativeAppWindowFrameViewMac(widget(), this);
}

bool ChromeNativeAppWindowViewsMac::IsMaximized() const {
  return !IsMinimized() && !IsFullscreen() &&
         NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow());
}

gfx::Rect ChromeNativeAppWindowViewsMac::GetRestoredBounds() const {
  if (NSWindowIsMaximized(GetNativeWindow().GetNativeNSWindow()))
    return gfx::ScreenRectFromNSRect(bounds_before_maximize_);

  return ChromeNativeAppWindowViews::GetRestoredBounds();
}

void ChromeNativeAppWindowViewsMac::Show() {
  UnhideWithoutActivation();
  ChromeNativeAppWindowViews::Show();
}

void ChromeNativeAppWindowViewsMac::ShowInactive() {
  if (is_hidden_with_app_)
    return;

  ChromeNativeAppWindowViews::ShowInactive();
}

void ChromeNativeAppWindowViewsMac::Activate() {
  UnhideWithoutActivation();
  ChromeNativeAppWindowViews::Activate();
}

void ChromeNativeAppWindowViewsMac::Maximize() {
  if (IsFullscreen())
    return;

  NSWindow* window = GetNativeWindow().GetNativeNSWindow();
  if (!NSWindowIsMaximized(window))
    [window setFrame:[[window screen] visibleFrame] display:YES animate:YES];

  if (IsMinimized())
    [window deminiaturize:nil];
}

void ChromeNativeAppWindowViewsMac::Restore() {
  NSWindow* window = GetNativeWindow().GetNativeNSWindow();
  if (NSWindowIsMaximized(window))
    [window setFrame:bounds_before_maximize_ display:YES animate:YES];

  ChromeNativeAppWindowViews::Restore();
}

void ChromeNativeAppWindowViewsMac::FlashFrame(bool flash) {
  apps::ExtensionAppShimHandler::Get()->RequestUserAttentionForWindow(
      app_window(), flash ? apps::APP_SHIM_ATTENTION_CRITICAL
                          : apps::APP_SHIM_ATTENTION_CANCEL);
}

void ChromeNativeAppWindowViewsMac::OnWidgetCreated(views::Widget* widget) {
  nswindow_observer_.reset(
      [[ResizeNotificationObserver alloc] initForNativeAppWindow:this]);
}

void ChromeNativeAppWindowViewsMac::ShowWithApp() {
  is_hidden_with_app_ = false;
  if (!app_window()->is_hidden())
    ShowInactive();
}

void ChromeNativeAppWindowViewsMac::HideWithApp() {
  is_hidden_with_app_ = true;
  ChromeNativeAppWindowViews::Hide();
}

void ChromeNativeAppWindowViewsMac::UnhideWithoutActivation() {
  if (is_hidden_with_app_) {
    apps::ExtensionAppShimHandler::Get()->UnhideWithoutActivationForWindow(
        app_window());
    is_hidden_with_app_ = false;
  }
}
