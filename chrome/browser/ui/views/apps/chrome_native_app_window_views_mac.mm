// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/apps/chrome_native_app_window_views_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/views/apps/app_window_native_widget_mac.h"
#import "chrome/browser/ui/views/apps/native_app_window_frame_view_mac.h"
#import "ui/gfx/mac/coordinate_conversion.h"

// This observer is used to get NSWindow notifications. We need to monitor
// zoom and full screen events to store the correct bounds to Restore() to.
@interface ResizeNotificationObserver : NSObject {
 @private
  // Weak. Owns us.
  raw_ptr<ChromeNativeAppWindowViewsMac> _nativeAppWindow;
}
- (instancetype)initForNativeAppWindow:
    (ChromeNativeAppWindowViewsMac*)nativeAppWindow;
- (void)onWindowWillStartLiveResize:(NSNotification*)notification;
- (void)onWindowWillExitFullScreen:(NSNotification*)notification;
- (void)onWindowDidExitFullScreen:(NSNotification*)notification;
- (void)stopObserving;
@end

@implementation ResizeNotificationObserver

- (instancetype)initForNativeAppWindow:
    (ChromeNativeAppWindowViewsMac*)nativeAppWindow {
  if ((self = [super init])) {
    _nativeAppWindow = nativeAppWindow;
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(onWindowWillStartLiveResize:)
               name:NSWindowWillStartLiveResizeNotification
             object:static_cast<ui::BaseWindow*>(nativeAppWindow)
                        ->GetNativeWindow()
                        .GetNativeNSWindow()];
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(onWindowWillExitFullScreen:)
               name:NSWindowWillExitFullScreenNotification
             object:static_cast<ui::BaseWindow*>(nativeAppWindow)
                        ->GetNativeWindow()
                        .GetNativeNSWindow()];
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(onWindowDidExitFullScreen:)
               name:NSWindowDidExitFullScreenNotification
             object:static_cast<ui::BaseWindow*>(nativeAppWindow)
                        ->GetNativeWindow()
                        .GetNativeNSWindow()];
  }
  return self;
}

- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)onWindowWillStartLiveResize:(NSNotification*)notification {
  _nativeAppWindow->OnWindowWillStartLiveResize();
}

- (void)onWindowWillExitFullScreen:(NSNotification*)notification {
  _nativeAppWindow->OnWindowWillExitFullScreen();
}

- (void)onWindowDidExitFullScreen:(NSNotification*)notification {
  _nativeAppWindow->OnWindowDidExitFullScreen();
}

- (void)stopObserving {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  _nativeAppWindow = nullptr;
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

std::unique_ptr<views::NonClientFrameView>
ChromeNativeAppWindowViewsMac::CreateStandardDesktopAppFrame() {
  return std::make_unique<NativeAppWindowFrameViewMac>(widget(), this);
}

std::unique_ptr<views::NonClientFrameView>
ChromeNativeAppWindowViewsMac::CreateNonStandardAppFrame() {
  return std::make_unique<NativeAppWindowFrameViewMac>(widget(), this);
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
  Profile* profile =
      Profile::FromBrowserContext(app_window()->browser_context());
  AppShimHost* shim_host = apps::AppShimManager::Get()->FindHost(
      profile, app_window()->extension_id());
  if (!shim_host)
    return;
  shim_host->GetAppShim()->SetUserAttention(
      flash ? chrome::mojom::AppShimAttentionType::kCritical
            : chrome::mojom::AppShimAttentionType::kCancel);
}

void ChromeNativeAppWindowViewsMac::OnWidgetCreated(views::Widget* widget) {
  nswindow_observer_ =
      [[ResizeNotificationObserver alloc] initForNativeAppWindow:this];
}
