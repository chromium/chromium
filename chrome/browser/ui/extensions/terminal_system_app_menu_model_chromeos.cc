// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/terminal_system_app_menu_model_chromeos.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"

TerminalSystemAppMenuModel::TerminalSystemAppMenuModel(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : AppMenuModel(provider, browser) {}

TerminalSystemAppMenuModel::~TerminalSystemAppMenuModel() {}

void TerminalSystemAppMenuModel::Build() {
  AddItemWithStringId(IDC_OPTIONS, IDS_OPTIONS);
  if (base::FeatureList::IsEnabled(features::kTerminalSystemAppSplits)) {
    AddItemWithStringId(IDC_TERMINAL_SPLIT_VERTICAL,
                        IDS_APP_TERMINAL_SPLIT_VERTICAL);
    AddItemWithStringId(IDC_TERMINAL_SPLIT_HORIZONTAL,
                        IDS_APP_TERMINAL_SPLIT_HORIZONTAL);
  }
  AddItemWithStringId(IDC_FIND, IDS_FIND);
}

bool TerminalSystemAppMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void TerminalSystemAppMenuModel::ExecuteCommand(int command_id,
                                                int event_flags) {
  static const base::NoDestructor<base::flat_map<int, std::string>> kCommands({
      // Opens settings page.
      {IDC_OPTIONS, "options"},
      // Split the currently selected pane vertically.
      {IDC_TERMINAL_SPLIT_VERTICAL, "splitv"},
      // Split the currently selected pane horizontally.
      {IDC_TERMINAL_SPLIT_HORIZONTAL, "splith"},
      // Open the find dialog.
      {IDC_FIND, "find"},
  });

  auto it = kCommands->find(command_id);
  if (it == kCommands->end()) {
    NOTREACHED() << "Unknown command " << command_id;
    return;
  }
  const std::string& fragment = it->second;
  url::Replacements<char> replacements;
  replacements.SetRef(fragment.c_str(), url::Component(0, fragment.size()));
  NavigateParams params(
      browser(),
      browser()->app_controller()->GetAppLaunchURL().ReplaceComponents(
          replacements),
      ui::PAGE_TRANSITION_FROM_API);
  Navigate(&params);
}

void TerminalSystemAppMenuModel::LogMenuAction(AppMenuAction action_id) {
  UMA_HISTOGRAM_ENUMERATION("TerminalSystemAppFrame.WrenchMenu.MenuAction",
                            action_id, LIMIT_MENU_ACTION);
}
