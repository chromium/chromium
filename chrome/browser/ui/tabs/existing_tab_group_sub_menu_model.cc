// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

ExistingTabGroupSubMenuModel::ExistingTabGroupSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabStripModel* model,
    int context_index)
    : ExistingBaseSubMenuModel(parent_delegate,
                               model,
                               context_index,
                               kMinExistingTabGroupCommandId) {
  const auto& tp = ThemeService::GetThemeProviderForProfile(model->profile());
  constexpr int kIconSize = 14;
  std::vector<MenuItemInfo> menu_item_infos;

  for (tab_groups::TabGroupId group : GetOrderedTabGroupsInSubMenu()) {
    const TabGroup* tab_group = model->group_model()->GetTabGroup(group);
    const std::u16string group_title = tab_group->visual_data()->title();
    const std::u16string displayed_title =
        group_title.empty() ? tab_group->GetContentString() : group_title;
    const int color_id =
        GetTabGroupContextMenuColorId(tab_group->visual_data()->color());
    // TODO (kylixrd): Investigate passing in color_id in order to color the
    // icon using the ColorProvider.
    ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
        kTabGroupIcon, tp.GetColor(color_id), kIconSize);
    menu_item_infos.emplace_back(MenuItemInfo{displayed_title, image_model});
    menu_item_infos.back().may_have_mnemonics = false;
  }
  Build(IDS_TAB_CXMENU_SUBMENU_NEW_GROUP, menu_item_infos);
}

std::vector<tab_groups::TabGroupId>
ExistingTabGroupSubMenuModel::GetOrderedTabGroupsInSubMenu() {
  std::vector<tab_groups::TabGroupId> ordered_groups;
  base::Optional<tab_groups::TabGroupId> current_group = base::nullopt;
  for (int i = 0; i < model()->count(); ++i) {
    base::Optional<tab_groups::TabGroupId> new_group =
        model()->GetTabGroupForTab(i);
    if (new_group.has_value() && new_group != current_group &&
        ShouldShowGroup(model(), GetContextIndex(), new_group.value())) {
      ordered_groups.push_back(new_group.value());
    }
    current_group = new_group;
  }
  return ordered_groups;
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowSubmenu(TabStripModel* model,
                                                     int context_index) {
  for (tab_groups::TabGroupId group : model->group_model()->ListTabGroups()) {
    if (ShouldShowGroup(model, context_index, group)) {
      return true;
    }
  }
  return false;
}

void ExistingTabGroupSubMenuModel::ExecuteNewCommand(int event_flags) {
  parent_delegate()->ExecuteCommand(TabStripModel::CommandAddToNewGroup,
                                    event_flags);
}

void ExistingTabGroupSubMenuModel::ExecuteExistingCommand(int command_index) {
  base::RecordAction(base::UserMetricsAction("TabContextMenu_NewTabInGroup"));

  if (size_t{command_index} >= model()->group_model()->ListTabGroups().size())
    return;
  if (!model()->ContainsIndex(GetContextIndex()))
    return;
  model()->ExecuteAddToExistingGroupCommand(
      GetContextIndex(), GetOrderedTabGroupsInSubMenu()[command_index]);
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowGroup(
    TabStripModel* model,
    int context_index,
    tab_groups::TabGroupId group) {
  if (!model->IsTabSelected(context_index)) {
    if (group != model->GetTabGroupForTab(context_index))
      return true;
  } else {
    for (int index : model->selection_model().selected_indices()) {
      if (group != model->GetTabGroupForTab(index)) {
        return true;
      }
    }
  }
  return false;
}
