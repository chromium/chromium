// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_

#include <string>

#include "base/timer/timer.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/metadata/metadata_header_macros.h"

class BrowserView;

// The 'app menu' button for a web app window.
class WebAppMenuButton : public AppMenuButton {
 public:
  METADATA_HEADER(WebAppMenuButton);
  static int GetMenuButtonSizeForBrowser(Browser* browser);
  explicit WebAppMenuButton(BrowserView* browser_view,
                            std::u16string accessible_name = std::u16string());
  WebAppMenuButton(const WebAppMenuButton&) = delete;
  WebAppMenuButton& operator=(const WebAppMenuButton&) = delete;
  ~WebAppMenuButton() override;

  // Sets the color of the menu button icon and highlight.
  void SetColor(SkColor color);
  SkColor GetColor() const;

  // Fades the menu button highlight on and off.
  void StartHighlightAnimation();

  void ButtonPressed(const ui::Event& event);

  // AppMenuButton:
  SkColor GetInkDropBaseColor() const override;

 protected:
  BrowserView* browser_view() { return browser_view_; }

 private:
  void FadeHighlightOff();

  // The containing browser view.
  BrowserView* browser_view_;

  SkColor color_ = gfx::kPlaceholderColor;

  base::OneShotTimer highlight_off_timer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_WEB_APP_MENU_BUTTON_H_
