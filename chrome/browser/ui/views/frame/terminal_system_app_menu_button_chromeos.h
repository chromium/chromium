// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TERMINAL_SYSTEM_APP_MENU_BUTTON_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TERMINAL_SYSTEM_APP_MENU_BUTTON_CHROMEOS_H_

#include "chrome/browser/ui/views/web_apps/web_app_menu_button.h"

class BrowserView;

class TerminalSystemAppMenuButton : public WebAppMenuButton {
 public:
  explicit TerminalSystemAppMenuButton(BrowserView* browser_view);
  ~TerminalSystemAppMenuButton() override;

  // WebAppMenuButton:
  void ButtonPressed(views::Button* source, const ui::Event& event) override;

 private:
  // WebAppMenuButton:
  const char* GetClassName() const override;

  DISALLOW_COPY_AND_ASSIGN(TerminalSystemAppMenuButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TERMINAL_SYSTEM_APP_MENU_BUTTON_CHROMEOS_H_
