// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

// Aura-specific parts of ChromeNativeAppWindowViews. This is used directly on
// Linux and Windows, and is the base class for the Ash specific class used on
// ChromeOS.
class ChromeNativeAppWindowViewsAura : public ChromeNativeAppWindowViews {
 public:
  ChromeNativeAppWindowViewsAura();
  ~ChromeNativeAppWindowViewsAura() override;

 protected:
  ui::WindowShowState GetRestorableState(
      const ui::WindowShowState restore_state) const;

  // ChromeNativeAppWindowViews implementation.
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  views::NonClientFrameView* CreateNonStandardAppFrame() override;

  // ui::BaseWindow implementation.
  ui::WindowShowState GetRestoredState() const override;
  ui::ZOrderLevel GetZOrderLevel() const override;

  // NativeAppWindow implementation.
  void UpdateShape(std::unique_ptr<ShapeRects> rects) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ShapedAppWindowTargeterTest,
                           ResizeInsetsWithinBounds);

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_H_
