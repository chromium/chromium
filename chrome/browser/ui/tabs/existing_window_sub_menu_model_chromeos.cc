// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_window_sub_menu_model_chromeos.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"

namespace chromeos {

namespace {

int GetDeskIndexForBrowser(Browser* browser, int num_desks) {
  const std::string& workspace = browser->window()->GetWorkspace();
  int desk_index;
  // If the window is visible on all workspaces or unassigned
  // (aura::client::kWindowWorkspaceUnassignedWorkspace),
  // we should get the active desk index.
  if (workspace.empty() || browser->window()->IsVisibleOnAllWorkspaces()) {
    desk_index = DesksHelper::Get(browser->window()->GetNativeWindow())
                     ->GetActiveDeskIndex();
  } else {
    CHECK(base::StringToInt(workspace, &desk_index));
  }
  CHECK_LT(desk_index, num_desks);
  return desk_index;
}

// Returns true if there are at least 2 desks.
bool ShouldGroupByDesk(const DesksHelper* desks_helper) {
  constexpr int kMinNumOfDesks = 2;
  return desks_helper->GetNumberOfDesks() >= kMinNumOfDesks;
}

DesksHelper* GetDesksHelper(const std::vector<Browser*>& existing_browsers) {
  DCHECK_GT(existing_browsers.size(), 0UL);
  // It is OK to get DesksHelper from the window of the first existing browser
  // since the APIs (GetNumberOfDesks, GetDeskName(index)) used by this class
  // doesn't depend on the specific aura::Window.
  return DesksHelper::Get(
      (*existing_browsers.begin())->window()->GetNativeWindow());
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
                                 context_index) {
  // If we shouldn't group by desk, ExistingWindowSubMenuModel's ctor has
  // already built the menu.
  std::vector<Browser*> tabbed_browser_windows =
      tab_menu_model_delegate->GetOtherBrowserWindows(
          model->delegate()->IsForWebApp());
  if (!ShouldGroupByDesk(GetDesksHelper(tabbed_browser_windows))) {
    return;
  }

  ClearMenu();
  BuildMenuGroupedByDesk(tabbed_browser_windows);
}

ExistingWindowSubMenuModelChromeOS::~ExistingWindowSubMenuModelChromeOS() =
    default;

void ExistingWindowSubMenuModelChromeOS::BuildMenuGroupedByDesk(
    const std::vector<Browser*>& existing_browsers) {
  // Get the vector of MenuItemInfo for |existing_browsers| and then group them
  // by desk.
  const DesksHelper* desks_helper = GetDesksHelper(existing_browsers);
  const int num_desks = desks_helper->GetNumberOfDesks();
  std::vector<std::vector<ExistingBaseSubMenuModel::MenuItemInfo>>
      grouped_by_desk_menu_item_infos(num_desks);
  for (ExistingBaseSubMenuModel::MenuItemInfo item :
       BuildMenuItemInfoVectorForBrowsers(existing_browsers)) {
    const int desk = GetDeskIndexForBrowser(
        existing_browsers[item.target_index.value()], num_desks);
    item.accessible_name = l10n_util::GetStringFUTF16(
        IDS_TAB_CXMENU_GROUPED_BY_DESK_MENU_ITEM_A11Y, item.text,
        desks_helper->GetDeskName(desk));
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

    // Create a MenuItemInfo for this desk for `desk_name`.
    std::u16string desk_name;
    if (desk == static_cast<size_t>(desks_helper->GetActiveDeskIndex())) {
      desk_name = l10n_util::GetStringFUTF16(
          IDS_TAB_CXMENU_GROUPED_BY_DESK_CURRENT_DESK_LABEL,
          desks_helper->GetDeskName(desk));
    } else {
      desk_name = desks_helper->GetDeskName(desk);
    }
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

}  // namespace chromeos
