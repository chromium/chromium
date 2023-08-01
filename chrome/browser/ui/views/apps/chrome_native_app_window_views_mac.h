// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_MAC_H_

#import <Foundation/Foundation.h>

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

@class ResizeNotificationObserver;

// Mac-specific parts of ChromeNativeAppWindowViews.
class ChromeNativeAppWindowViewsMac : public ChromeNativeAppWindowViews {
 public:
  ChromeNativeAppWindowViewsMac();

  ChromeNativeAppWindowViewsMac(const ChromeNativeAppWindowViewsMac&) = delete;
  ChromeNativeAppWindowViewsMac& operator=(
      const ChromeNativeAppWindowViewsMac&) = delete;

  ~ChromeNativeAppWindowViewsMac() override;

  // Called by |nswindow_observer_| for window resize events.
  void OnWindowWillStartLiveResize();
  void OnWindowWillExitFullScreen();
  void OnWindowDidExitFullScreen();

 protected:
  // ChromeNativeAppWindowViews implementation.
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  std::unique_ptr<views::NonClientFrameView> CreateStandardDesktopAppFrame()
      override;
  std::unique_ptr<views::NonClientFrameView> CreateNonStandardAppFrame()
      override;

  // ui::BaseWindow implementation.
  bool IsMaximized() const override;
  gfx::Rect GetRestoredBounds() const override;
  void Maximize() override;
  void Restore() override;
  void FlashFrame(bool flash) override;

  // WidgetObserver implementation.
  void OnWidgetCreated(views::Widget* widget) override;

 private:
  // Used to notify us about certain NSWindow events.
  ResizeNotificationObserver* __strong nswindow_observer_;

  // The bounds of the window just before it was last maximized.
  NSRect bounds_before_maximize_;

  // Set true during an exit fullscreen transition, so that the live resize
  // event AppKit sends can be distinguished from a zoom-triggered live resize.
  bool in_fullscreen_transition_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_MAC_H_
