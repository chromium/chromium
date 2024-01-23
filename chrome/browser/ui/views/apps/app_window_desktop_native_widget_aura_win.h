// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_NATIVE_WIDGET_AURA_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_NATIVE_WIDGET_AURA_WIN_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"

class ChromeNativeAppWindowViewsWin;

namespace views {
class DesktopWindowTreeHost;
}

// AppWindowDesktopNativeWidgetAura is a DesktopNativeWidgetAura subclass that
// handles creating the right type of tree hosts for app windows on Windows.
class AppWindowDesktopNativeWidgetAuraWin
    : public views::DesktopNativeWidgetAura {
 public:
  explicit AppWindowDesktopNativeWidgetAuraWin(
      ChromeNativeAppWindowViewsWin* app_window);

  AppWindowDesktopNativeWidgetAuraWin(
      const AppWindowDesktopNativeWidgetAuraWin&) = delete;
  AppWindowDesktopNativeWidgetAuraWin& operator=(
      const AppWindowDesktopNativeWidgetAuraWin&) = delete;

 protected:
  ~AppWindowDesktopNativeWidgetAuraWin() override;

  // Overridden from views::DesktopNativeWidgetAura:
  void InitNativeWidget(views::Widget::InitParams params) override;
  void Maximize() override;
  void Minimize() override;
  void OnHostClosed() override;

 private:
  // Ownership managed by the views system.
  raw_ptr<ChromeNativeAppWindowViewsWin, DanglingUntriaged> app_window_;

  // Owned by superclass DesktopNativeWidgetAura.
  raw_ptr<views::DesktopWindowTreeHost> tree_host_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_WINDOW_DESKTOP_NATIVE_WIDGET_AURA_WIN_H_
