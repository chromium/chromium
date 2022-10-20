// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_NATIVE_WIDGET_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_NATIVE_WIDGET_MAC_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/widget/native_widget_mac.h"

namespace extensions {
class NativeAppWindow;
}

// This implements features specific to app windows, e.g. frameless windows that
// behave like normal windows.
class AppWindowNativeWidgetMac : public views::NativeWidgetMac {
 public:
  AppWindowNativeWidgetMac(views::Widget* widget,
                           extensions::NativeAppWindow* native_app_window);

  AppWindowNativeWidgetMac(const AppWindowNativeWidgetMac&) = delete;
  AppWindowNativeWidgetMac& operator=(const AppWindowNativeWidgetMac&) = delete;

  ~AppWindowNativeWidgetMac() override;

 protected:
  // NativeWidgetMac:
  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override;
  NativeWidgetMacNSWindow* CreateNSWindow(
      const remote_cocoa::mojom::CreateWindowParams* params) override;

 private:
  // Weak. Owned by extensions::AppWindow (which manages our Widget via its
  // WebContents).
  raw_ptr<extensions::NativeAppWindow, DanglingUntriaged> native_app_window_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_NATIVE_WIDGET_MAC_H_
