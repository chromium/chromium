// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include "base/containers/contains.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

ExistingTabGroupSubMenuModel::ExistingTabGroupSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabStripModel* model,
    int context_index)
    : ExistingBaseSubMenuModel(parent_delegate,
                               model,
                               context_index,
                               kMinExistingTabGroupCommandId,
                               TabStripModel::CommandAddToNewGroup) {
  DCHECK(model->SupportsTabGroups());
  const ui::ColorProvider& color_provider =
      model->GetWebContentsAt(context_index)->GetColorProvider();
  constexpr int kIconSize = 14;
  std::vector<MenuItemInfo> menu_item_infos;

  std::vector<tab_groups::TabGroupId> ordered_tab_groups =
      GetOrderedTabGroupsInSubMenu();
  for (size_t i = 0; i < ordered_tab_groups.size(); ++i) {
    tab_groups::TabGroupId group = ordered_tab_groups[i];
    const TabGroup* tab_group = model->group_model()->GetTabGroup(group);
    const std::u16string group_title = tab_group->visual_data()->title();
    const std::u16string displayed_title =
        group_title.empty() ? tab_group->GetContentString() : group_title;
    const int color_id =
        GetTabGroupContextMenuColorId(tab_group->visual_data()->color());
    ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
        kTabGroupIcon, color_provider.GetColor(color_id), kIconSize);
    menu_item_infos.emplace_back(MenuItemInfo{displayed_title, image_model});
    menu_item_infos.back().may_have_mnemonics = false;

    menu_item_infos.back().target_index = static_cast<int>(i);
    target_index_to_group_mapping_.emplace(i, group);
  }
  Build(IDS_TAB_CXMENU_SUBMENU_NEW_GROUP, menu_item_infos);
}

ExistingTabGroupSubMenuModel::~ExistingTabGroupSubMenuModel() = default;

std::vector<tab_groups::TabGroupId>
ExistingTabGroupSubMenuModel::GetOrderedTabGroupsInSubMenu() {
  std::vector<tab_groups::TabGroupId> ordered_groups;
  absl::optional<tab_groups::TabGroupId> current_group = absl::nullopt;
  for (int i = 0; i < model()->count(); ++i) {
    absl::optional<tab_groups::TabGroupId> new_group =
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
  TabGroupModel* group_model = model->group_model();
  if (!group_model)
    return false;

  for (tab_groups::TabGroupId group : group_model->ListTabGroups()) {
    if (ShouldShowGroup(model, context_index, group)) {
      return true;
    }
  }
  return false;
}

void ExistingTabGroupSubMenuModel::ExecuteExistingCommand(int target_index) {
  TabGroupModel* group_model = model()->group_model();
  if (!group_model)
    return;

  base::RecordAction(base::UserMetricsAction("TabContextMenu_NewTabInGroup"));

  if (static_cast<size_t>(target_index) >= group_model->ListTabGroups().size())
    return;

  if (!model()->ContainsIndex(GetContextIndex()))
    return;

  if (!base::Contains(target_index_to_group_mapping_, target_index) ||
      !model()->group_model()->ContainsTabGroup(
          target_index_to_group_mapping_.at(target_index)))
    return;

  model()->ExecuteAddToExistingGroupCommand(
      GetContextIndex(), target_index_to_group_mapping_.at(target_index));
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
