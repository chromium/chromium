// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/terminal_system_app_menu_model_chromeos.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/crostini/crostini_terminal.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"

TerminalSystemAppMenuModel::TerminalSystemAppMenuModel(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : AppMenuModel(provider, browser) {}

TerminalSystemAppMenuModel::~TerminalSystemAppMenuModel() = default;

void TerminalSystemAppMenuModel::Build() {
  AddItemWithStringId(IDC_TERMINAL_LINUX, IDS_APP_TERMINAL_LINUX);
  AddItemWithStringId(IDC_TERMINAL_SSH, IDS_APP_TERMINAL_SSH);
  AddItemWithStringId(IDC_OPTIONS, IDS_SETTINGS);
}

bool TerminalSystemAppMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void TerminalSystemAppMenuModel::ExecuteCommand(int command_id,
                                                int event_flags) {
  if (command_id == IDC_TERMINAL_LINUX) {
    chrome::NewTab(browser());
  } else if (command_id == IDC_TERMINAL_SSH) {
    // TODO(crbug.com/1241262): Include nassh in cros rootfs and host it within
    // Terminal SWA rather than loading the nassh extension.
    GURL nassh_extension(
        "chrome-extension://iodihamcpbpeioajjeobimgagajmlibd/html/nassh.html");
    content::WebContents::CreateParams params(
        browser()->profile(), tab_util::GetSiteInstanceForNewTab(
                                  browser()->profile(), nassh_extension));
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(params);
    contents->GetController().LoadURL(nassh_extension, content::Referrer(),
                                      ui::PAGE_TRANSITION_FROM_API,
                                      std::string());
    browser()->tab_strip_model()->AddWebContents(
        std::move(contents),
        /*idx=*/-1, ui::PAGE_TRANSITION_FROM_API, TabStripModel::ADD_ACTIVE);
  } else if (command_id == IDC_OPTIONS) {
    crostini::LaunchTerminalSettings(browser()->profile());
  }
}

void TerminalSystemAppMenuModel::LogMenuAction(AppMenuAction action_id) {
  UMA_HISTOGRAM_ENUMERATION("TerminalSystemAppFrame.WrenchMenu.MenuAction",
                            action_id, LIMIT_MENU_ACTION);
}
