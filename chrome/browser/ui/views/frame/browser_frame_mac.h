// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_MAC_H_

#include "chrome/browser/ui/views/frame/native_browser_frame.h"

#import "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "ui/views/widget/native_widget_mac.h"

class BrowserFrame;
class BrowserView;
@class BrowserWindowTouchBarController;
@class BrowserWindowTouchBarViewsDelegate;

////////////////////////////////////////////////////////////////////////////////
//  BrowserFrameMac is a NativeWidgetMac subclass that provides
//  the window frame for the Chrome browser window.
//
class BrowserFrameMac : public views::NativeWidgetMac,
                        public NativeBrowserFrame {
 public:
  BrowserFrameMac(BrowserFrame* browser_frame, BrowserView* browser_view);

  API_AVAILABLE(macos(10.12.2))
  BrowserWindowTouchBarController* GetTouchBarController() const;

  // Overridden from views::NativeWidgetMac:
  int32_t SheetOffsetY() override;
  void GetWindowFrameTitlebarHeight(bool* override_titlebar_height,
                                    float* titlebar_height) override;
  void OnFocusWindowToolbar() override;
  void OnWindowFullscreenStateChange() override;

  // Overridden from NativeBrowserFrame:
  views::Widget::InitParams GetWidgetParams() override;
  bool UseCustomFrame() const override;
  bool UsesNativeSystemMenu() const override;
  bool ShouldSaveWindowPlacement() const override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  bool HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;

 protected:
  ~BrowserFrameMac() override;

  // Overridden from views::NativeWidgetMac:
  void ValidateUserInterfaceItem(
      int32_t command,
      remote_cocoa::mojom::ValidateUserInterfaceItemResult* result) override;
  bool ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder) override;
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override;
  NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params) override;
  remote_cocoa::ApplicationHost* GetRemoteCocoaApplicationHost() override;
  void OnWindowInitialized() override;
  void OnWindowDestroying(gfx::NativeWindow window) override;

  // Overridden from NativeBrowserFrame:
  int GetMinimizeButtonOffset() const override;

 private:
  BrowserView* browser_view_;  // Weak. Our ClientView.
  base::scoped_nsobject<BrowserWindowTouchBarViewsDelegate> touch_bar_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_MAC_H_
