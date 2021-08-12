// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_window_sub_menu_model_chromeos.h"

#include "ash/public/cpp/desks_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

namespace chromeos {

namespace {

int GetDeskIndexForBrowser(Browser* browser, int num_desks) {
  int desk_index;
  CHECK(base::StringToInt(browser->window()->GetWorkspace(), &desk_index));
  CHECK_LT(desk_index, num_desks);
  return desk_index;
}

}  // namespace

ExistingWindowSubMenuModelChromeOS::ExistingWindowSubMenuModelChromeOS(
    base::PassKey<ExistingWindowSubMenuModel> passkey,
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabMenuModelDelegate* tab_menu_model_delegate,
    TabStripModel* model,
    int context_index)
    : ExistingWindowSubMenuModel(ExistingWindowSubMenuModel::GetPassKey(),
                                 parent_delegate,
                                 tab_menu_model_delegate,
                                 model,
                                 context_index),
      desks_helper_(ash::DesksHelper::Get()) {
  // If we shouldn't group by desk, ExistingWindowSubMenuModel's ctor has
  // already built the menu.
  if (!ShouldGroupByDesk())
    return;

  ClearMenu();
  BuildMenuGroupedByDesk(
      tab_menu_model_delegate->GetExistingWindowsForMoveMenu());
}

ExistingWindowSubMenuModelChromeOS::~ExistingWindowSubMenuModelChromeOS() =
    default;

void ExistingWindowSubMenuModelChromeOS::BuildMenuGroupedByDesk(
    const std::vector<Browser*>& existing_browsers) {
  // Get the vector of MenuItemInfo for |existing_browsers| and then group them
  // by desk.
  const int num_desks = desks_helper_->GetNumberOfDesks();
  std::vector<std::vector<ExistingBaseSubMenuModel::MenuItemInfo>>
      grouped_by_desk_menu_item_infos(num_desks);
  for (ExistingBaseSubMenuModel::MenuItemInfo item :
       BuildMenuItemInfoVectorForBrowsers(existing_browsers)) {
    const int desk = GetDeskIndexForBrowser(
        existing_browsers[item.target_index.value()], num_desks);
    item.accessible_name = l10n_util::GetStringFUTF16(
        IDS_TAB_CXMENU_GROUPED_BY_DESK_MENU_ITEM_A11Y, item.text,
        desks_helper_->GetDeskName(desk));
    grouped_by_desk_menu_item_infos[desk].push_back(std::move(item));
  }

  // Now flatten the 2D vector based on its grouping and insert the desk
  // headings before each group.
  std::vector<MenuItemInfo> sorted_menu_items_with_headings;
  for (size_t desk = 0; desk < grouped_by_desk_menu_item_infos.size(); ++desk) {
    const std::vector<MenuItemInfo>& desk_items =
        grouped_by_desk_menu_item_infos[desk];
    if (desk_items.empty())
      continue;

    // Create a MenuItemInfo for this desk.
    const std::u16string desk_name = desks_helper_->GetDeskName(desk);
    ExistingBaseSubMenuModel::MenuItemInfo desk_heading(desk_name);
    desk_heading.may_have_mnemonics = false;
    desk_heading.accessible_name = l10n_util::GetStringFUTF16(
        IDS_TAB_CXMENU_GROUPED_BY_DESK_DESK_HEADING_A11Y, desk_name,
        base::NumberToString16(desk_items.size()));

    sorted_menu_items_with_headings.push_back(std::move(desk_heading));
    sorted_menu_items_with_headings.insert(
        sorted_menu_items_with_headings.end(), desk_items.begin(),
        desk_items.end());
  }

  Build(IDS_TAB_CXMENU_MOVETOANOTHERNEWWINDOW, sorted_menu_items_with_headings);
}

bool ExistingWindowSubMenuModelChromeOS::ShouldGroupByDesk() {
  constexpr int kMinNumOfDesks = 2;
  return desks_helper_->GetNumberOfDesks() >= kMinNumOfDesks;
}

}  // namespace chromeos
