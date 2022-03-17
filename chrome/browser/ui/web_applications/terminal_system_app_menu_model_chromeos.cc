// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/terminal_system_app_menu_model_chromeos.h"

#include <string>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
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
  int next_command = ash::LAUNCH_APP_SHORTCUT_FIRST;
  AddItemWithStringIdAndIcon(IDC_TERMINAL_HOME, IDS_ACCNAME_HOME,
                             icon(kNavigateHomeIcon));

  // All SSH connections.
  std::vector<std::pair<std::string, std::string>> connections =
      crostini::GetSSHConnections(browser()->profile());
  for (const auto& connection : connections) {
    urls_.push_back(GURL(base::StrCat(
        {chrome::kChromeUIUntrustedTerminalURL,
         "html/terminal_ssh.html#profile-id:", connection.first})));
    AddItemWithIcon(next_command++, base::UTF8ToUTF16(connection.second),
                    icon(kTerminalSshIcon));
  }

  // All Linux containers.
  std::vector<crostini::ContainerId> containers =
      crostini::GetLinuxContainers(browser()->profile());
  for (const auto& container : containers) {
    urls_.push_back(
        crostini::GenerateTerminalURL(browser()->profile(), container));
    // Use label 'Linux' if we have only the default container, else use
    // <vm_name>:<container_name>.
    std::u16string label = l10n_util::GetStringUTF16(IDS_APP_TERMINAL_LINUX);
    if (containers.size() > 1 ||
        container != crostini::ContainerId::GetDefault()) {
      label = base::UTF8ToUTF16(
          base::StrCat({container.vm_name, ":", container.container_name}));
    }
    AddItemWithIcon(next_command++, label, icon(kCrostiniMascotIcon));
  }

  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  AddItemWithStringIdAndIcon(IDC_OPTIONS, IDS_SETTINGS,
                             icon(vector_icons::kSettingsIcon));
}

bool TerminalSystemAppMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

GURL TerminalSystemAppMenuModel::GetURLForCommand(int command_id) const {
  if (command_id == IDC_TERMINAL_HOME) {
    return GURL(base::StrCat(
        {chrome::kChromeUIUntrustedTerminalURL, "html/terminal_home.html"}));
  } else if (command_id >= ash::LAUNCH_APP_SHORTCUT_FIRST &&
             command_id <= ash::LAUNCH_APP_SHORTCUT_LAST) {
    int i = command_id - ash::LAUNCH_APP_SHORTCUT_FIRST;
    if (i < urls_.size()) {
      return urls_[i];
    }
  }
  return GURL();
}

void TerminalSystemAppMenuModel::ExecuteCommand(int command_id,
                                                int event_flags) {
  if (command_id == IDC_OPTIONS) {
    crostini::LaunchTerminalSettings(browser()->profile());
  } else {
    GURL url = GetURLForCommand(command_id);
    if (url.is_valid()) {
      chrome::AddTabAt(browser(), url, /*index=*/-1, /*foreground=*/true);
    }
  }
}

void TerminalSystemAppMenuModel::LogMenuAction(AppMenuAction action_id) {
  UMA_HISTOGRAM_ENUMERATION("TerminalSystemAppFrame.WrenchMenu.MenuAction",
                            action_id, LIMIT_MENU_ACTION);
}
