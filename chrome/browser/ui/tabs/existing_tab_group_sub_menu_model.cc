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
#include "base/strings/string_util.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_on_close_helper.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_menu_utils.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"

namespace {
constexpr int kIconSize = 14;

// TODO(crbug.com/418774949) Move to TabGroupFeatures for desktop.
std::u16string GetContentString(const TabGroup& group) {
  constexpr size_t kContextMenuTabTitleMaxLength = 30;
  std::u16string format_string = l10n_util::GetPluralStringFUTF16(
      IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE, group.tab_count() - 1);
  std::u16string short_title;
  gfx::ElideString(
      group.GetFirstTab()->GetTabFeatures()->tab_ui_helper()->GetTitle(),
      kContextMenuTabTitleMaxLength, &short_title);
  return base::ReplaceStringPlaceholders(format_string, short_title, nullptr);
}

// Ungroups all of the tabs specified by |tab_indices_to_close| and then tries
// to close them and adds their URL to the end of the closed saved group
// denoted by |closed_saved_group_id|. Note that if there are any saved groups
// whose tabs are completely contained in |tab_indices_to_close|, we delete
// these groups from the tab group sync service before proceeding.
void MaybeDeleteTabsAndAddToSavedTabGroup(
    std::vector<int> tab_indices_to_close,
    base::Uuid closed_saved_group_id,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    TabStripModel* tab_strip_model) {
  if (tab_indices_to_close.empty()) {
    return;
  }

  if (!tab_group_sync_service->GetGroup(closed_saved_group_id)) {
    return;
  }

  // Get any open groups that are covered by |tab_indices_to_close| and delete
  // the saved groups they are in. Note that since we are in this function
  // the user already agreed to having them deleted.
  std::vector<tab_groups::TabGroupId> groups =
      tab_strip_model->GetGroupsDestroyedFromRemovingIndices(
          tab_indices_to_close);

  for (const tab_groups::TabGroupId& local_group_id : groups) {
    std::optional<tab_groups::SavedTabGroup> saved_group =
        tab_group_sync_service->GetGroup(local_group_id);

    if (saved_group) {
      tab_group_sync_service->RemoveGroup(saved_group->saved_guid());
    }
  }

  // Ungroup any tabs that are in open but not saved groups.
  // Keep a vector of tab pointers in case the indices change after the
  // ungrouping operation.
  std::vector<tabs::TabInterface*> tab_ptrs_to_close;

  for (int index : tab_indices_to_close) {
    tab_ptrs_to_close.push_back(tab_strip_model->GetTabAtIndex(index));
  }

  tab_strip_model->RemoveFromGroup(tab_indices_to_close);

  // Now that they are all ungrouped, close the tabs. But before doing that,
  // have the tabs listen for when they close, so that they will add
  // themselves to the saved group on closure.
  for (tabs::TabInterface* tab_to_close : tab_ptrs_to_close) {
    CHECK(tab_to_close);
    tab_to_close->GetTabFeatures()->saved_tab_group_on_close_helper()->SetGroup(
        closed_saved_group_id);
    tab_to_close->Close();
  }
}

// Callback function for when we use the tab group deletion dialog, if we see
// it is possible for saved tab groups to be deleted from executing commands in
// using the commands in ExistingTabGroupSubMenuModel.
void OnTabGroupDeletionDialogOK(
    std::vector<int> tab_indices_to_close,
    base::Uuid closed_saved_group_id,
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    TabStripModel* tab_strip_model,
    tab_groups::DeletionDialogController::DeletionDialogTiming timing) {
  MaybeDeleteTabsAndAddToSavedTabGroup(tab_indices_to_close,
                                       closed_saved_group_id,
                                       tab_group_sync_service, tab_strip_model);
}

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
  CHECK_EQ(menu_item_infos.size(), groups.size());
  for (const auto& group : groups) {
    size_t index = target_index_to_group_mapping_.size();
    menu_item_infos[index].target_index = index;
    target_index_to_group_mapping_.emplace(index, group);
  }

  // For each window, append the tab groups to the end of the menu items.
  if (tab_menu_model_delegate_) {
    for (BrowserWindowInterface* browser :
         tab_menu_model_delegate_->GetOtherBrowserWindows(/*is_app=*/false)) {
      if (browser->GetFeatures().tab_strip_model() == model) {
        continue;
      }
      const std::vector<MenuItemInfo> retrieved_menu_item_infos =
          GetMenuItemsFromModel(browser->GetFeatures().tab_strip_model());
      menu_item_infos.insert(menu_item_infos.end(),
                             retrieved_menu_item_infos.begin(),
                             retrieved_menu_item_infos.end());
      groups = GetGroupsFromModel(browser->GetFeatures().tab_strip_model());
      CHECK_EQ(menu_item_infos.size(),
               groups.size() + target_index_to_group_mapping_.size());
      for (const auto& group : groups) {
        size_t index = target_index_to_group_mapping_.size();
        menu_item_infos[index].target_index = index;
        target_index_to_group_mapping_.emplace(index, group);
      }
    }
  }

  if (features::IsTabGroupMenuMoreEntryPointsEnabled()) {
    AppendMenuItemInfosFromSavedTabGroups(menu_item_infos);
  }

  Build(IDS_TAB_CXMENU_SUBMENU_NEW_GROUP, menu_item_infos);
}

ExistingTabGroupSubMenuModel::~ExistingTabGroupSubMenuModel() = default;

const std::vector<tab_groups::TabGroupId>
ExistingTabGroupSubMenuModel::GetGroupsFromModel(TabStripModel* current_model) {
  // No model, no group model, no service.
  if (!current_model || !current_model->group_model()) {
    return {};
  }

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
        group_title.empty() ? GetContentString(*tab_group) : group_title;

    menu_item_infos.emplace_back(
        CreateMenuItemInfo(displayed_title, tab_group->visual_data()->color()));
    menu_item_infos.back().may_have_mnemonics = false;
  }

  return menu_item_infos;
}

// static
std::vector<base::Uuid> ExistingTabGroupSubMenuModel::GetClosedSavedTabGroups(
    TabMenuModelDelegate* tab_menu_model_delegate) {
  if (!tab_menu_model_delegate) {
    return {};
  }

  tab_groups::TabGroupSyncService* tgss =
      tab_menu_model_delegate->GetTabGroupSyncService();

  if (!tgss) {
    return {};
  }

  std::vector<base::Uuid> saved_tab_groups;

  for (const tab_groups::SavedTabGroup& group : tgss->GetAllGroups()) {
    if (group.saved_tabs().empty()) {
      continue;
    }

    // Having a local group ID implies it is open.
    if (group.local_group_id()) {
      continue;
    }

    saved_tab_groups.push_back(group.saved_guid());
  }

  return saved_tab_groups;
}

ExistingTabGroupSubMenuModel::MenuItemInfo
ExistingTabGroupSubMenuModel::CreateMenuItemInfo(
    const std::u16string& displayed_title,
    const tab_groups::TabGroupColorId& color_id) {
  const int context_menu_color_id = GetTabGroupContextMenuColorId(color_id);
  const ui::ColorProvider& color_provider =
      model()->GetWebContentsAt(GetContextIndex())->GetColorProvider();
  ui::ImageModel image_model = ui::ImageModel::FromVectorIcon(
      kTabGroupIcon, color_provider.GetColor(context_menu_color_id), kIconSize);

  return {displayed_title, image_model};
}

void ExistingTabGroupSubMenuModel::AppendMenuItemInfosFromSavedTabGroups(
    std::vector<MenuItemInfo>& existing_menu_item_infos) {
  std::vector<base::Uuid> saved_tab_groups =
      GetClosedSavedTabGroups(tab_menu_model_delegate_);

  if (saved_tab_groups.empty()) {
    return;
  }

  CHECK(tab_menu_model_delegate_);
  tab_groups::TabGroupSyncService* tgss =
      tab_menu_model_delegate_->GetTabGroupSyncService();
  CHECK(tgss);

  int menu_item_info_count_old = existing_menu_item_infos.size();

  // Keep track of whether we added a separator for the closed groups.
  // The first closed saved tab group has a separator before it, unless
  // there are no open groups.
  bool separated_first_closed_group = false;

  // Create the corresponding MenuItemInfos and add them
  // to the end of the list.
  for (const base::Uuid& saved_guid : saved_tab_groups) {
    std::optional<tab_groups::SavedTabGroup> group_opt =
        tgss->GetGroup(saved_guid);
    CHECK(group_opt.has_value());
    const tab_groups::SavedTabGroup& group = *group_opt;

    std::u16string displayed_title = group.title();

    if (displayed_title.empty()) {
      displayed_title =
          tab_groups::TabGroupMenuUtils::GetMenuTextForGroup(group);
    }

    existing_menu_item_infos.emplace_back(
        CreateMenuItemInfo(displayed_title, group.color()));

    size_t index = target_index_to_group_mapping_.size();
    existing_menu_item_infos[index].target_index = index;
    existing_menu_item_infos.back().may_have_mnemonics = false;

    if (!separated_first_closed_group) {
      if (menu_item_info_count_old > 0) {
        existing_menu_item_infos.back().has_separator_before = true;
      }
      separated_first_closed_group = true;
    }

    target_index_to_group_mapping_.emplace(index, saved_guid);
  }

  CHECK(target_index_to_group_mapping_.size() ==
        menu_item_info_count_old + saved_tab_groups.size());

  if (existing_menu_item_infos.size() > 0) {
    existing_menu_item_infos.front().has_separator_before = true;
  }
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowSubmenu(
    TabStripModel* model,
    int context_index,
    TabMenuModelDelegate* tab_menu_model_delegate) {
  TabGroupModel* group_model = model->group_model();
  if (!group_model) {
    return false;
  }

  // Look at tab groups in current window
  for (tab_groups::TabGroupId group : group_model->ListTabGroups()) {
    if (ShouldShowGroup(model, context_index, group)) {
      return true;
    }
  }

  // Look at tab groups in all other windows
  if (tab_menu_model_delegate) {
    for (BrowserWindowInterface* browser :
         tab_menu_model_delegate->GetOtherBrowserWindows(/*is_app=*/false)) {
      TabGroupModel* browser_group_model =
          browser->GetFeatures().tab_strip_model()->group_model();
      if (!browser_group_model) {
        continue;
      }
      for (tab_groups::TabGroupId group :
           browser_group_model->ListTabGroups()) {
        if (ShouldShowGroup(model, context_index, group)) {
          return true;
        }
      }
    }
  }

  // Look if there are any saved tab groups that are not open.
  if (features::IsTabGroupMenuMoreEntryPointsEnabled()) {
    return GetClosedSavedTabGroups(tab_menu_model_delegate).size() > 0;
  }

  return false;
}

void ExistingTabGroupSubMenuModel::ExecuteExistingCommandForTesting(
    size_t target_index) {
  ExecuteExistingCommand(target_index);
}

void ExistingTabGroupSubMenuModel::ExecuteExistingCommand(size_t target_index) {
  // The tab strip may have been modified while the context menu was open,
  // including closing the tab originally at |context_index|.
  if (!model()->ContainsIndex(GetContextIndex())) {
    return;
  }

  DCHECK_LE(size_t(target_index), target_index_to_group_mapping_.size());
  TabGroupModel* group_model = model()->group_model();
  if (!group_model) {
    return;
  }

  tab_groups::EitherGroupID group_v =
      target_index_to_group_mapping_.at(target_index);

  if (std::holds_alternative<base::Uuid>(group_v)) {
    const base::Uuid& group_id = get<base::Uuid>(group_v);
    AddSelectedTabsToSavedGroup(group_id);
    base::RecordAction(
        base::UserMetricsAction("TabContextMenu_AddToClosedSavedGroup"));
  } else if (std::holds_alternative<tab_groups::LocalTabGroupID>(group_v)) {
    base::RecordAction(base::UserMetricsAction("TabContextMenu_NewTabInGroup"));
    tab_groups::TabGroupId group = get<tab_groups::TabGroupId>(group_v);
    AddSelectedTabsToOpenGroup(group);
  } else {
    NOTREACHED();
  }
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowGroup(
    TabStripModel* model,
    int context_index,
    tab_groups::TabGroupId group) {
  if (!model->IsTabSelected(context_index)) {
    if (group != model->GetTabGroupForTab(context_index)) {
      return true;
    }
  } else {
    for (int index : model->selection_model().selected_indices()) {
      if (group != model->GetTabGroupForTab(index)) {
        return true;
      }
    }
  }
  return false;
}

std::vector<int> ExistingTabGroupSubMenuModel::GetSelectedIndices() {
  if (!model()->IsTabSelected(GetContextIndex())) {
    // If the context index is not selected, set it as the selected index.
    return {GetContextIndex()};
  } else {
    // Use the currently selected indices.
    const ui::ListSelectionModel::SelectedIndices selection_indices =
        model()->selection_model().selected_indices();
    return std::vector<int>(selection_indices.begin(), selection_indices.end());
  }
}

void ExistingTabGroupSubMenuModel::AddSelectedTabsToSavedGroup(
    const base::Uuid& group_id) {
  if (!tab_menu_model_delegate_) {
    return;
  }

  tab_groups::TabGroupSyncService* tgss =
      tab_menu_model_delegate_->GetTabGroupSyncService();

  if (!tgss) {
    return;
  }

  std::optional<tab_groups::SavedTabGroup> group_opt = tgss->GetGroup(group_id);

  CHECK(group_opt.has_value());

  std::vector<int> selected_indices = GetSelectedIndices();

  if (selected_indices.empty()) {
    return;
  }

  std::vector<tab_groups::TabGroupId> groups_destroyed =
      model()->GetGroupsDestroyedFromRemovingIndices(selected_indices);

  if (groups_destroyed.empty()) {
    MaybeDeleteTabsAndAddToSavedTabGroup(
        selected_indices, group_opt->saved_guid(), tgss, model());
  } else {
    // We have saved tab groups that may be deleted.
    // Show the tab group deletion dialog and delete the groups using
    // a callback function.
    tabs::TabInterface* tab_0 = model()->GetTabAtIndex(selected_indices[0]);
    tab_groups::DeletionDialogController* deletion_dialog_controller =
        tab_0->GetBrowserWindowInterface()
            ->GetFeatures()
            .tab_group_deletion_dialog_controller();

    base::OnceCallback<void(
        tab_groups::DeletionDialogController::DeletionDialogTiming)>
        callback = base::BindOnce(&OnTabGroupDeletionDialogOK, selected_indices,
                                  group_opt->saved_guid(), tgss, model());

    tab_groups::DeletionDialogController::DialogMetadata saved_dialog_metadata(
        tab_groups::DeletionDialogController::DialogType::DeleteSingle,
        /*closing_group_count=*/groups_destroyed.size(),
        /*closing_multiple_tabs=*/selected_indices.size() > 1);

    deletion_dialog_controller->MaybeShowDialog(
        saved_dialog_metadata, std::move(callback), std::nullopt);
  }
}

void ExistingTabGroupSubMenuModel::AddSelectedTabsToOpenGroup(
    const tab_groups::TabGroupId& group) {
  // If the group exists in this model, move the tab into it.
  TabGroupModel* group_model = model()->group_model();
  if (group_model->ContainsTabGroup(group)) {
    model()->ExecuteAddToExistingGroupCommand(GetContextIndex(), group);
    return;
  }

  // Find the index of the browser with the group we are looking for.
  std::optional<size_t> browser_index;
  std::vector<BrowserWindowInterface*> browsers =
      tab_menu_model_delegate_->GetOtherBrowserWindows(/*is_app=*/false);
  for (size_t i = 0; i < browsers.size(); ++i) {
    TabStripModel* potential_model =
        browsers[i]->GetFeatures().tab_strip_model();
    if (potential_model && potential_model != model() &&
        potential_model->group_model()->ContainsTabGroup(group)) {
      browser_index = i;
      break;
    }
  }

  // Do nothing if the browser does not exist.
  if (!browser_index.has_value()) {
    return;
  }

  // Collect the selected tab indices from the source model into a list.
  std::vector<int> selected_indices = GetSelectedIndices();
  std::vector<tabs::TabInterface*> tabs =
      model()->GetTabsAtIndices(selected_indices);

  // Unpin the tabs before moving from end
  for (int i = selected_indices.size() - 1; i >= 0; --i) {
    int tab_index = selected_indices[i];
    if (model()->IsTabPinned(tab_index)) {
      model()->SetTabPinned(tab_index, false);
    }
  }
  // Unpinning can move tabs; repopulate `selected_indices`.
  selected_indices.clear();
  for (tabs::TabInterface* tab : tabs) {
    selected_indices.push_back(model()->GetIndexOfTab(tab));
  }
  model()->delegate()->MoveToExistingWindow(selected_indices,
                                            browser_index.value());

  TabStripModel* const found_model =
      browsers[browser_index.value()]->GetFeatures().tab_strip_model();
  // Find the tabs in the new window.
  selected_indices.clear();
  for (tabs::TabInterface* tab : tabs) {
    selected_indices.push_back(found_model->GetIndexOfTab(tab));
  }
  // Ensure that the selected_indices maintain selection in the new window.
  for (int tab_index : selected_indices) {
    if (!found_model->IsTabSelected(tab_index)) {
      found_model->SelectTabAt(tab_index);
    }
  }

  found_model->ExecuteAddToExistingGroupCommand(selected_indices.back(), group);
}
