// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_MENU_BUTTON_H_

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/controls/button/button.h"

class BrowserView;

// The 'app menu' button for a web app window.
class WebAppMenuButton : public AppMenuButton, public views::ButtonListener {
 public:
  explicit WebAppMenuButton(BrowserView* browser_view);
  ~WebAppMenuButton() override;

  // Sets the color of the menu button icon and highlight.
  void SetColor(SkColor color);

  // Fades the menu button highlight on and off.
  void StartHighlightAnimation();

  // views::ButtonListener:
  void ButtonPressed(views::Button* source, const ui::Event& event) override;

  // AppMenuButton:
  SkColor GetInkDropBaseColor() const override;

 protected:
  BrowserView* browser_view() { return browser_view_; }

 private:
  void FadeHighlightOff();

  // views::View:
  const char* GetClassName() const override;

  // The containing browser view.
  BrowserView* browser_view_;

  SkColor ink_drop_color_ = gfx::kPlaceholderColor;

  base::OneShotTimer highlight_off_timer_;

  DISALLOW_COPY_AND_ASSIGN(WebAppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_MENU_BUTTON_H_
