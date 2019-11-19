// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/terminal_system_app_menu_button_chromeos.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/extensions/terminal_system_app_menu_model_chromeos.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/views/controls/menu/menu_runner.h"

TerminalSystemAppMenuButton::TerminalSystemAppMenuButton(
    BrowserView* browser_view)
    : WebAppMenuButton(browser_view) {}

TerminalSystemAppMenuButton::~TerminalSystemAppMenuButton() {}

void TerminalSystemAppMenuButton::ButtonPressed(views::Button* source,
                                                const ui::Event& event) {
  Browser* browser = browser_view()->browser();
  RunMenu(std::make_unique<TerminalSystemAppMenuModel>(browser_view(), browser),
          browser,
          event.IsKeyEvent() ? views::MenuRunner::SHOULD_SHOW_MNEMONICS
                             : views::MenuRunner::NO_FLAGS,
          /*alert_reopen_tab_items=*/false);

  base::RecordAction(
      base::UserMetricsAction("TerminalSystemAppMenuButtonButton_Clicked"));
}

const char* TerminalSystemAppMenuButton::GetClassName() const {
  return "TerminalSystemAppMenuButton";
}
