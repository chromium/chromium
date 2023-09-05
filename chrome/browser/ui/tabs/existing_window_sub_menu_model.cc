// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/accelerators/accelerator.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/tabs/existing_window_sub_menu_model_chromeos.h"
#endif

namespace {

// The max length for a window title.
static constexpr int kWindowTitleForMenuMaxWidth = 400;

}  // namespace

// static:
std::unique_ptr<ExistingWindowSubMenuModel> ExistingWindowSubMenuModel::Create(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabMenuModelDelegate* tab_menu_model_delegate,
    TabStripModel* model,
    int context_index) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<chromeos::ExistingWindowSubMenuModelChromeOS>(
      GetPassKey(), parent_delegate, tab_menu_model_delegate, model,
      context_index);
#else
  return std::make_unique<ExistingWindowSubMenuModel>(
      GetPassKey(), parent_delegate, tab_menu_model_delegate, model,
      context_index);
#endif
}

ExistingWindowSubMenuModel::ExistingWindowSubMenuModel(
    base::PassKey<ExistingWindowSubMenuModel> passkey,
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabMenuModelDelegate* tab_menu_model_delegate,
    TabStripModel* model,
    int context_index)
    : ExistingBaseSubMenuModel(parent_delegate,
                               model,
                               context_index,
                               kMinExistingWindowCommandId,
                               TabStripModel::CommandMoveTabsToNewWindow) {
  Build(IDS_TAB_CXMENU_MOVETOANOTHERNEWWINDOW,
        BuildMenuItemInfoVectorForBrowsers(
            tab_menu_model_delegate->GetOtherBrowserWindows(
                model->delegate()->IsForWebApp())));
}

ExistingWindowSubMenuModel::~ExistingWindowSubMenuModel() = default;

bool ExistingWindowSubMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  if (IsNewCommand(command_id)) {
    return parent_delegate()->GetAcceleratorForCommandId(
        TabStripModel::CommandMoveTabsToNewWindow, accelerator);
  }
  return false;
}

bool ExistingWindowSubMenuModel::IsCommandIdChecked(int command_id) const {
  if (IsNewCommand(command_id)) {
    return parent_delegate()->IsCommandIdChecked(
        TabStripModel::CommandMoveTabsToNewWindow);
  }
  return false;
}

bool ExistingWindowSubMenuModel::IsCommandIdEnabled(int command_id) const {
  if (IsNewCommand(command_id)) {
    return parent_delegate()->IsCommandIdEnabled(
        TabStripModel::CommandMoveTabsToNewWindow);
  }
  return true;
}

// static:
bool ExistingWindowSubMenuModel::ShouldShowSubmenu(Profile* profile) {
  return chrome::GetTabbedBrowserCount(profile) > 1;
}

bool ExistingWindowSubMenuModel::ShouldShowSubmenuForApp(
    TabMenuModelDelegate* tab_menu_model_delegate) {
  return tab_menu_model_delegate->GetOtherBrowserWindows(/*is_app=*/true)
             .size() >= 1;
}

// static:
base::PassKey<ExistingWindowSubMenuModel>
ExistingWindowSubMenuModel::GetPassKey() {
  return base::PassKey<ExistingWindowSubMenuModel>();
}

// static:
std::vector<ExistingBaseSubMenuModel::MenuItemInfo>
ExistingWindowSubMenuModel::BuildMenuItemInfoVectorForBrowsers(
    const std::vector<Browser*>& existing_browsers) {
  std::vector<MenuItemInfo> menu_item_infos;
  for (size_t i = 0; i < existing_browsers.size(); ++i) {
    Browser* browser = existing_browsers[i];
    auto window_title =
        browser->GetWindowTitleForMaxWidth(kWindowTitleForMenuMaxWidth);
    menu_item_infos.emplace_back(window_title);
    menu_item_infos.back().may_have_mnemonics = false;
    menu_item_infos.back().target_index = i;
  }
  return menu_item_infos;
}

void ExistingWindowSubMenuModel::ExecuteExistingCommand(size_t target_index) {
  model()->ExecuteAddToExistingWindowCommand(GetContextIndex(), target_index);
}
