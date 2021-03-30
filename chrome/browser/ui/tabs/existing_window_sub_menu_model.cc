// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_window_sub_menu_model.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/vector_icons.h"

ExistingWindowSubMenuModel::ExistingWindowSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabStripModel* model,
    int context_index)
    : ExistingBaseSubMenuModel(parent_delegate,
                               model,
                               context_index,
                               kMinExistingWindowCommandId) {
  std::vector<MenuItemInfo> menu_item_infos;
  auto window_titles = model->GetExistingWindowsForMoveMenu();

  for (auto& window_title : window_titles) {
    menu_item_infos.emplace_back(MenuItemInfo{window_title});
    menu_item_infos.back().may_have_mnemonics = false;
  }
  Build(IDS_TAB_CXMENU_MOVETOANOTHERNEWWINDOW, menu_item_infos);
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

// static
bool ExistingWindowSubMenuModel::ShouldShowSubmenu(Profile* profile) {
  return chrome::GetTabbedBrowserCount(profile) > 1;
}

void ExistingWindowSubMenuModel::ExecuteNewCommand(int event_flags) {
  parent_delegate()->ExecuteCommand(TabStripModel::CommandMoveTabsToNewWindow,
                                    event_flags);
}

void ExistingWindowSubMenuModel::ExecuteExistingCommand(int command_index) {
  model()->ExecuteAddToExistingWindowCommand(GetContextIndex(), command_index);
}
