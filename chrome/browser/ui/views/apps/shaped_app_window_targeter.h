// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_SHAPED_APP_WINDOW_TARGETER_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_SHAPED_APP_WINDOW_TARGETER_H_

#include "base/macros.h"
#include "ui/aura/window_targeter.h"

class ChromeNativeAppWindowViews;

class ShapedAppWindowTargeter : public aura::WindowTargeter {
 public:
  explicit ShapedAppWindowTargeter(ChromeNativeAppWindowViews* app_window);
  ~ShapedAppWindowTargeter() override;

 private:
  // aura::WindowTargeter:
  std::unique_ptr<aura::WindowTargeter::HitTestRects> GetExtraHitTestShapeRects(
      aura::Window* target) const override;

  ChromeNativeAppWindowViews* app_window_;

  DISALLOW_COPY_AND_ASSIGN(ShapedAppWindowTargeter);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_SHAPED_APP_WINDOW_TARGETER_H_
