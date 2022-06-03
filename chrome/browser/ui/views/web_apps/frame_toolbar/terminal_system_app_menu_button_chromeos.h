// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_TERMINAL_SYSTEM_APP_MENU_BUTTON_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_TERMINAL_SYSTEM_APP_MENU_BUTTON_CHROMEOS_H_

#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"

class BrowserView;

class TerminalSystemAppMenuButton : public WebAppMenuButton {
 public:
  explicit TerminalSystemAppMenuButton(BrowserView* browser_view);
  TerminalSystemAppMenuButton(const TerminalSystemAppMenuButton&) = delete;
  TerminalSystemAppMenuButton& operator=(const TerminalSystemAppMenuButton&) =
      delete;
  ~TerminalSystemAppMenuButton() override;

 private:
  // WebAppMenuButton:
  void ButtonPressed(const ui::Event& event) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_FRAME_TOOLBAR_TERMINAL_SYSTEM_APP_MENU_BUTTON_CHROMEOS_H_
