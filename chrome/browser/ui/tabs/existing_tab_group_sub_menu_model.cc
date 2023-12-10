// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include <optional>
#include <vector>

#include "base/containers/contains.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {
constexpr int kIconSize = 14;
}  // anonymous namespace

ExistingTabGroupSubMenuModel::ExistingTabGroupSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabMenuModelDelegate* tab_menu_model_delegate,
    TabStripModel* model,
    int context_index)
    : ExistingBaseSubMenuModel(parent_delegate,
                               model,
                               context_index,
                               kMinExistingTabGroupCommandId,
                               TabStripModel::CommandAddToNewGroup),
      tab_menu_model_delegate_(tab_menu_model_delegate) {
  DCHECK(model->SupportsTabGroups());
  std::vector<MenuItemInfo> menu_item_infos;

  menu_item_infos = GetMenuItemsFromModel(model);
  std::vector<tab_groups::TabGroupId> groups = GetGroupsFromModel(model);
  // TODO(dljames): Consider moving CHECK + loop into a separate private
  // function that updates both `menu_item_infos` and
  // `target_index_to_group_mapping_`.
  CHECK_EQ(menu_item_infos.size(), groups.size());
  for (const auto& group : groups) {
    size_t index = target_index_to_group_mapping_.size();
    menu_item_infos[index].target_index = index;
    target_index_to_group_mapping_.emplace(index, group);
  }

  // For each window, append the tab groups to the end of the menu items.
  if (tab_menu_model_delegate_) {
    for (Browser* browser :
         tab_menu_model_delegate_->GetOtherBrowserWindows(/*is_app=*/false)) {
      if (browser->tab_strip_model() == model)
        continue;
      const std::vector<MenuItemInfo> retrieved_menu_item_infos =
          GetMenuItemsFromModel(browser->tab_strip_model());
      menu_item_infos.insert(menu_item_infos.end(),
                             retrieved_menu_item_infos.begin(),
                             retrieved_menu_item_infos.end());
      groups = GetGroupsFromModel(browser->tab_strip_model());
      CHECK_EQ(menu_item_infos.size(),
               groups.size() + target_index_to_group_mapping_.size());
      for (const auto& group : groups) {
        size_t index = target_index_to_group_mapping_.size();
        menu_item_infos[index].target_index = index;
        target_index_to_group_mapping_.emplace(index, group);
      }
    }
  }

  Build(IDS_TAB_CXMENU_SUBMENU_NEW_GROUP, menu_item_infos);
}

ExistingTabGroupSubMenuModel::~ExistingTabGroupSubMenuModel() = default;

const std::vector<tab_groups::TabGroupId>
ExistingTabGroupSubMenuModel::GetGroupsFromModel(TabStripModel* current_model) {
  // No model, no group model, no service.
  if (!current_model || !current_model->group_model())
    return {};

  // Add tab groups to `groups` if they differ from our indexes current group.
  std::vector<tab_groups::TabGroupId> groups;
  for (auto& group : current_model->group_model()->ListTabGroups()) {
    if (model() == current_model &&
        model()->GetTabGroupForTab(GetContextIndex()).has_value() &&
        model()->GetTabGroupForTab(GetContextIndex()).value() == group) {
      continue;
    }

    groups.push_back(group);
  }

  return groups;
}

const std::vector<ExistingTabGroupSubMenuModel::MenuItemInfo>
ExistingTabGroupSubMenuModel::GetMenuItemsFromModel(
    TabStripModel* current_model) {
  const std::vector<tab_groups::TabGroupId> groups =
      GetGroupsFromModel(current_model);
  std::vector<MenuItemInfo> menu_item_infos;

  for (const auto& group : groups) {
    const TabGroup* tab_group =
        current_model->group_model()->GetTabGroup(group);
    const std::u16string group_title = tab_group->visual_data()->title();
    // TODO(dljames): Add method to tab_group.cc to return displayed_title.
    // TODO(dljames): Add unit tests for all of tab_group.h
    const std::u16string displayed_title =
        group_title.empty() ? tab_group->GetContentString() : group_title;
    const int color_id =
        GetTabGroupContextMenuColorId(tab_group->visual_data()->color());
    const ui::ColorProvider& color_provider =
        model()->GetWebContentsAt(GetContextIndex())->GetColorProvider();
    ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
        kTabGroupIcon, color_provider.GetColor(color_id), kIconSize);

    menu_item_infos.emplace_back(displayed_title, image_model);
    menu_item_infos.back().may_have_mnemonics = false;
  }

  return menu_item_infos;
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
    TabStripModel* model,
    int context_index,
    TabMenuModelDelegate* tab_menu_model_delegate) {
  TabGroupModel* group_model = model->group_model();
  if (!group_model)
    return false;

  // Look at tab groups in current window
  for (tab_groups::TabGroupId group : group_model->ListTabGroups()) {
    if (ShouldShowGroup(model, context_index, group)) {
      return true;
    }
  }

  // Look at tab groups in all other windows
  if (tab_menu_model_delegate) {
    for (Browser* browser :
         tab_menu_model_delegate->GetOtherBrowserWindows(/*is_app=*/false)) {
      TabGroupModel* browser_group_model =
          browser->tab_strip_model()->group_model();
      if (!browser_group_model)
        continue;
      for (tab_groups::TabGroupId group :
           browser_group_model->ListTabGroups()) {
        if (ShouldShowGroup(model, context_index, group))
          return true;
      }
    }
  }

  return false;
}

void ExistingTabGroupSubMenuModel::ExecuteExistingCommand(size_t target_index) {
  // The tab strip may have been modified while the context menu was open,
  // including closing the tab originally at |context_index|.
  if (!model()->ContainsIndex(GetContextIndex())) {
    return;
  }

  DCHECK_LE(size_t(target_index), target_index_to_group_mapping_.size());
  TabGroupModel* group_model = model()->group_model();
  if (!group_model)
    return;

  base::RecordAction(base::UserMetricsAction("TabContextMenu_NewTabInGroup"));

  tab_groups::TabGroupId group =
      target_index_to_group_mapping_.at(target_index);

  // If the group exists in this model, move the tab into it.
  if (group_model->ContainsTabGroup(group)) {
    model()->ExecuteAddToExistingGroupCommand(GetContextIndex(), group);
    return;
  }

  // Find the index of the browser with the group we are looking for.
  std::optional<size_t> browser_index;
  std::vector<Browser*> browsers =
      tab_menu_model_delegate_->GetOtherBrowserWindows(/*is_app=*/false);
  for (size_t i = 0; i < browsers.size(); ++i) {
    TabStripModel* potential_model = browsers[i]->tab_strip_model();
    if (potential_model && potential_model != model() &&
        potential_model->group_model()->ContainsTabGroup(group)) {
      browser_index = i;
      break;
    }
  }

  // Do nothing if the browser does not exist.
  if (!browser_index.has_value())
    return;

  std::vector<int> selected_indices;
  if (!model()->IsTabSelected(GetContextIndex())) {
    // If the context index is not selected, set it as the selected index.
    selected_indices = {GetContextIndex()};
  } else {
    // Use the currently selected indices as the selected_indices we will move.
    const ui::ListSelectionModel::SelectedIndices selection_indices =
        model()->selection_model().selected_indices();
    selected_indices =
        std::vector<int>(selection_indices.begin(), selection_indices.end());
  }

  // At the time this was written, all tabs moved to a new window via
  // MoveToExistingWindow() are placed at the end of the tabstrip where any
  // previously selected tabs in the new window are unselected.
  model()->delegate()->MoveToExistingWindow(selected_indices,
                                            browser_index.value());

  TabStripModel* found_model =
      browsers[browser_index.value()]->tab_strip_model();

  // Ensure that the selected_indices maintain selection in the new window.
  // Our indices to consider are guaranteed to be at the end of the tabstrip.
  for (size_t count = 0; count < selected_indices.size(); ++count) {
    const int tab_index = found_model->count() - 1 - count;
    if (!found_model->IsTabSelected(tab_index)) {
      found_model->ToggleSelectionAt(tab_index);
    }
  }

  // Move all selected tabs into `group`. Note, we can choose any tab that is
  // currently selected. For consistency we choose the last tab since we know
  // where it is.
  found_model->ExecuteAddToExistingGroupCommand(found_model->count() - 1,
                                                group);
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

void ExistingTabGroupSubMenuModel::ExecuteExistingCommandForTesting(
    size_t target_index) {
  ExecuteExistingCommand(target_index);
}
