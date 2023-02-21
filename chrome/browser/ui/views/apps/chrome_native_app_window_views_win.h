// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_WIN_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"

namespace web_app {
struct ShortcutInfo;
}

class AppWindowFrameViewWin;

// Windows-specific parts of the views-backed native shell window implementation
// for packaged apps.
class ChromeNativeAppWindowViewsWin : public ChromeNativeAppWindowViewsAura {
 public:
  ChromeNativeAppWindowViewsWin();

  ChromeNativeAppWindowViewsWin(const ChromeNativeAppWindowViewsWin&) = delete;
  ChromeNativeAppWindowViewsWin& operator=(
      const ChromeNativeAppWindowViewsWin&) = delete;

  ~ChromeNativeAppWindowViewsWin() override;

  AppWindowFrameViewWin* frame_view() { return frame_view_; }

 private:
  void OnShortcutInfoLoaded(
      const web_app::ShortcutInfo& shortcut_info);

  HWND GetNativeAppWindowHWND() const;
  void EnsureCaptionStyleSet();

  // Overridden from ChromeNativeAppWindowViews:
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  void InitializeDefaultWindow(
      const extensions::AppWindow::CreateParams& create_params) override;
  std::unique_ptr<views::NonClientFrameView> CreateStandardDesktopAppFrame()
      override;

  // Overridden from views::WidgetDelegate:
  bool CanMinimize() const override;

  // Populated if there is a standard desktop app frame, which provides special
  // information to the native widget implementation. This will be NULL if the
  // frame is a non-standard app frame created by CreateNonStandardAppFrame.
  raw_ptr<AppWindowFrameViewWin> frame_view_ = nullptr;

  // The Windows Application User Model ID identifying the app.
  std::wstring app_model_id_;

  // Whether the InitParams indicated that this window should be translucent.
  bool is_translucent_ = false;

  base::WeakPtrFactory<ChromeNativeAppWindowViewsWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_WIN_H_
