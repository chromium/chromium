// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/terminal_system_app_menu_model_chromeos.h"

#include <string>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "url/gurl.h"

TerminalSystemAppMenuModel::TerminalSystemAppMenuModel(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : AppMenuModel(provider, browser) {}

TerminalSystemAppMenuModel::~TerminalSystemAppMenuModel() = default;

void TerminalSystemAppMenuModel::Build() {
  auto icon = [](const gfx::VectorIcon& icon) {
    return ui::ImageModel::FromVectorIcon(icon, ui::kColorMenuIcon,
                                          ash::kAppContextMenuIconSize);
  };
  AddItemWithStringIdAndIcon(IDC_TERMINAL_LINUX, IDS_APP_TERMINAL_LINUX,
                             icon(kCrostiniMascotIcon));
  AddItemWithStringIdAndIcon(IDC_TERMINAL_SSH, IDS_APP_TERMINAL_SSH,
                             icon(kTerminalSshIcon));
  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  AddItemWithStringIdAndIcon(IDC_OPTIONS, IDS_SETTINGS,
                             icon(vector_icons::kSettingsIcon));
}

bool TerminalSystemAppMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void TerminalSystemAppMenuModel::ExecuteCommand(int command_id,
                                                int event_flags) {
  if (command_id == IDC_TERMINAL_LINUX) {
    chrome::AddTabAt(browser(),
                     crostini::GenerateTerminalURL(browser()->profile()),
                     /*idx=*/-1,
                     /*foreground=*/true);
  } else if (command_id == IDC_TERMINAL_SSH) {
    chrome::AddTabAt(browser(),
                     GURL("chrome-untrusted://terminal/html/terminal_ssh.html"),
                     /*idx=*/-1,
                     /*foreground=*/true);
  } else if (command_id == IDC_OPTIONS) {
    crostini::LaunchTerminalSettings(browser()->profile());
  }
}

void TerminalSystemAppMenuModel::LogMenuAction(AppMenuAction action_id) {
  UMA_HISTOGRAM_ENUMERATION("TerminalSystemAppFrame.WrenchMenu.MenuAction",
                            action_id, LIMIT_MENU_ACTION);
}
