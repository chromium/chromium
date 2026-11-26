// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "url/gurl.h"

namespace tab_groups {

namespace {

constexpr int kUIUpdateIconSize = 16;

struct TabMenuEntry {
  std::u16string title;
  GURL url;
};

struct ResolvedGroupData {
  void PopulateLocalState(const TabStripModel* tab_strip_model) {
    CHECK(local_group_id.has_value());
    const TabGroup* const tab_group =
        tab_strip_model->group_model()->GetTabGroup(*local_group_id);
    CHECK(tab_group);
    is_group_focused = tab_strip_model->GetFocusedGroup() == *local_group_id;
    // Disable moving when every tab in the window already belongs to this
    // group.
    should_enable_move = tab_strip_model->count() != tab_group->tab_count();
  }

  std::optional<base::Uuid> sync_id;
  std::optional<LocalTabGroupID> local_group_id;
  TabGroupColorId color = TabGroupColorId::kGrey;
  bool is_pinned = false;
  bool is_group_focused = false;
  bool should_enable_move = true;
  std::vector<TabMenuEntry> tabs;
};

ResolvedGroupData ResolveGroupData(
    const std::variant<SavedTabGroup, LocalTabGroupID>& group) {
  ResolvedGroupData data;
  if (const auto* saved_group = std::get_if<SavedTabGroup>(&group)) {
    data.sync_id = saved_group->saved_guid();
    data.local_group_id = saved_group->local_group_id();
    data.color = saved_group->color();
    data.is_pinned = saved_group->is_pinned();
    if (data.local_group_id.has_value()) {
      if (const BrowserWindowInterface* browser =
              SavedTabGroupUtils::GetBrowserWithTabGroupId(
                  *data.local_group_id)) {
        data.PopulateLocalState(browser->GetTabStripModel());
      }
    }
    data.tabs.reserve(saved_group->saved_tabs().size());
    for (const SavedTabGroupTab& tab : saved_group->saved_tabs()) {
      data.tabs.push_back(
          {TabGroupMenuUtils::GetMenuTextForTab(tab.title(), tab.url()),
           tab.url()});
    }
    return data;
  }

  const LocalTabGroupID local_id = std::get<LocalTabGroupID>(group);
  const BrowserWindowInterface* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_id);
  CHECK(browser);
  const TabStripModel* const tab_strip_model = browser->GetTabStripModel();
  const TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(local_id);
  CHECK(tab_group);
  data.local_group_id = local_id;
  data.color = tab_group->visual_data()->color();
  data.PopulateLocalState(tab_strip_model);
  const gfx::Range tab_range = tab_group->ListTabs();
  data.tabs.reserve(tab_range.length());
  for (tabs::TabInterface* tab :
       tab_strip_model->GetTabsAtIndices(tab_range.ToIntVector())) {
    TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab);
    CHECK(tab_ui_helper);
    const GURL url = tab_ui_helper->GetVisibleURL();
    data.tabs.push_back(
        {TabGroupMenuUtils::GetMenuTextForTab(tab_ui_helper->GetTitle(), url),
         url});
  }
  return data;
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kCloseGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kDeleteGroupMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kLeaveGroupMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel,
                                      kMoveGroupToNewWindowMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kOpenGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel,
                                      kToggleGroupFocusStateMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel,
                                      kToggleGroupPinStateMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kTabsTitleItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kTab);

STGTabsMenuModel::STGTabsMenuModel(BrowserWindowInterface* browser,
                                   TabGroupMenuContext context)
    : ui::SimpleMenuModel(this), browser_(browser), context_(context) {}

STGTabsMenuModel::STGTabsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                                   BrowserWindowInterface* browser,
                                   TabGroupMenuContext context)
    : ui::SimpleMenuModel(delegate), browser_(browser), context_(context) {}

STGTabsMenuModel::~STGTabsMenuModel() = default;

void STGTabsMenuModel::AddSavedGroupItems(
    bool is_pinned,
    const base::RepeatingCallback<int()>& get_next_command_id) {
  CHECK(sync_id_.has_value());
  int latest_command_id = 0;

  // Add item: pin or unpin.
  if (context_ != TabGroupMenuContext::ORGANIZER_PANEL) {
    latest_command_id = get_next_command_id.Run();
    AddItemWithStringIdAndIcon(
        latest_command_id,
        is_pinned ? IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP
                  : IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP,
        ui::ImageModel::FromVectorIcon(
            is_pinned ? features::IsRoundedIconsEnabled() ? kKeepOffIcon
                                                          : kKeepOffOldIcon
            : features::IsRoundedIconsEnabled() ? kKeepIcon
                                                : kKeepOldIcon,
            ui::kColorMenuIcon, kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kToggleGroupPinStateMenuItem);
    command_id_to_action_.emplace(
        latest_command_id,
        TabGroupMenuAction{TabGroupMenuAction::Type::PIN_OR_UNPIN_GROUP,
                           sync_id_.value()});
  }

  latest_command_id = get_next_command_id.Run();
  if (SavedTabGroupUtils::IsOwnerOfSharedTabGroup(browser_->GetProfile(),
                                                  sync_id_.value())) {
    // Add item: delete group.
    AddItemWithStringIdAndIcon(
        latest_command_id, IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP,
        ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                           ? kDeleteIcon
                                           : kTrashCanRefreshOldIcon,
                                       ui::kColorMenuIcon, kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kDeleteGroupMenuItem);
    command_id_to_action_.emplace(
        latest_command_id,
        TabGroupMenuAction{TabGroupMenuAction::Type::DELETE_GROUP,
                           sync_id_.value()});
  } else {
    // Add item: leave group.
    AddItemWithStringIdAndIcon(
        latest_command_id, IDS_DATA_SHARING_LEAVE_GROUP,
        ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                           ? kTabCloseIcon
                                           : kCloseGroupRefreshOldIcon,
                                       ui::kColorMenuIcon, kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kLeaveGroupMenuItem);
    command_id_to_action_.emplace(
        latest_command_id,
        TabGroupMenuAction{TabGroupMenuAction::Type::LEAVE_GROUP,
                           sync_id_.value()});
  }
}

void STGTabsMenuModel::Build(
    std::variant<SavedTabGroup, LocalTabGroupID> group,
    base::RepeatingCallback<int()> get_next_command_id,
    OnExecuteCommandCallback on_execute_command_callback) {
  command_id_to_action_.clear();
  on_execute_command_callback_ = std::move(on_execute_command_callback);
  should_enable_move_menu_item_ = true;

  ResolvedGroupData data = ResolveGroupData(group);
  sync_id_ = data.sync_id;
  should_enable_move_menu_item_ = data.should_enable_move;

  const EitherGroupID action_target =
      sync_id_.has_value() ? EitherGroupID(sync_id_.value())
                           : EitherGroupID(data.local_group_id.value());

  const bool is_group_open = data.local_group_id.has_value();
  int latest_command_id = get_next_command_id.Run();
  if (is_group_open) {
    AddItemWithStringIdAndIcon(
        latest_command_id, IDS_TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP,
        ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                           ? kTabCloseIcon
                                           : kCloseGroupRefreshOldIcon,
                                       ui::kColorMenuIcon, kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kCloseGroup);
    command_id_to_action_.emplace(
        latest_command_id,
        TabGroupMenuAction{TabGroupMenuAction::Type::CLOSE_GROUP,
                           action_target});
  } else {
    CHECK(sync_id_.has_value());
    AddItemWithStringIdAndIcon(
        latest_command_id, IDS_OPEN_GROUP_IN_BROWSER_MENU,
        ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                           ? kOpenInBrowserIcon
                                           : kOpenInBrowserOldIcon,
                                       ui::kColorMenuIcon, kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kOpenGroup);
    command_id_to_action_.emplace(
        latest_command_id,
        TabGroupMenuAction{TabGroupMenuAction::Type::OPEN_IN_BROWSER,
                           sync_id_.value()});
  }

  // Add item: open or move to new window.
  const std::u16string move_or_open_group_text =
      is_group_open ? l10n_util::GetStringUTF16(
                          IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW)
                    : l10n_util::GetStringUTF16(
                          IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW);
  latest_command_id = get_next_command_id.Run();
  AddItemWithIcon(
      latest_command_id, move_or_open_group_text,
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kMoveGroupIcon
                                         : kMoveGroupToNewWindowRefreshOldIcon,
                                     ui::kColorMenuIcon, kUIUpdateIconSize));
  SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                         kMoveGroupToNewWindowMenuItem);
  command_id_to_action_.emplace(
      latest_command_id,
      TabGroupMenuAction{TabGroupMenuAction::Type::OPEN_OR_MOVE_TO_NEW_WINDOW,
                         action_target});

  // Add item: focus or unfocus group.
  if (base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
    latest_command_id = get_next_command_id.Run();
    AddItemWithStringIdAndIcon(
        latest_command_id,
        data.is_group_focused ? IDS_TAB_GROUP_HEADER_CXMENU_UNFOCUS_GROUP
                              : IDS_TAB_GROUP_HEADER_CXMENU_FOCUS_GROUP,
        ui::ImageModel::FromVectorIcon(kFilterTiltShiftIcon, ui::kColorMenuIcon,
                                       kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kToggleGroupFocusStateMenuItem);
    command_id_to_action_.emplace(
        latest_command_id,
        TabGroupMenuAction{TabGroupMenuAction::Type::FOCUS_OR_UNFOCUS_GROUP,
                           action_target});
  }

  if (sync_id_.has_value()) {
    AddSavedGroupItems(data.is_pinned, get_next_command_id);
  }

  // Add a separator and title.
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddTitleWithStringId(IDS_TABS_TITLE_CXMENU);
  SetElementIdentifierAt(GetIndexOfCommandId(ui::MenuModel::kTitleId).value(),
                         kTabsTitleItem);

  // Perform an async request for the favicon from the favicon service.
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(browser_->GetProfile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);

  // Append open urls.
  for (size_t i = 0; i < data.tabs.size(); ++i) {
    const TabMenuEntry& tab = data.tabs.at(i);
    latest_command_id = get_next_command_id.Run();
    const ui::ImageModel image =
        favicon::GetDefaultFaviconModel(GetTabGroupBookmarkColorId(data.color));
    AddItemWithIcon(latest_command_id, tab.title, image);

    // Can be null for tests.
    if (favicon_service) {
      favicon_service->GetFaviconImageForPageURL(
          tab.url,
          base::BindOnce(&STGTabsMenuModel::OnFaviconDataAvailable,
                         weak_ptr_factory_.GetWeakPtr(), latest_command_id),
          &cancelable_task_tracker_);
    }

    command_id_to_action_.emplace(
        latest_command_id,
        TabGroupMenuAction{TabGroupMenuAction::Type::OPEN_URL, tab.url});
    // Assign an element identifier to the first tab.
    if (i == 0) {
      SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                             kTab);
    }
  }
}

bool STGTabsMenuModel::HasCommandId(int command_id) const {
  auto it = command_id_to_action_.find(command_id);
  return it != command_id_to_action_.end();
}

// ui::SimpleMenuModel::Delegate:
bool STGTabsMenuModel::IsCommandIdEnabled(int command_id) const {
  auto it = command_id_to_action_.find(command_id);
  CHECK(it != command_id_to_action_.end());
  if (it->second.type == TabGroupMenuAction::Type::OPEN_OR_MOVE_TO_NEW_WINDOW) {
    return should_enable_move_menu_item_;
  }
  return true;
}

void STGTabsMenuModel::ExecuteCommand(int command_id, int event_flags) {
  auto it = command_id_to_action_.find(command_id);
  CHECK(it != command_id_to_action_.end());

  TabGroupMenuAction action = it->second;
  // Copied locally since performing the action may destroy `this`.
  OnExecuteCommandCallback on_execute_command_callback =
      on_execute_command_callback_;
  TabGroupSyncService* tab_group_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser_->GetProfile());
  SavedTabGroupUtils::PerformTabGroupMenuAction(action, context_, browser_,
                                                tab_group_service);
  if (on_execute_command_callback) {
    on_execute_command_callback.Run(action);
  }
}

void STGTabsMenuModel::OnFaviconDataAvailable(
    int command_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty()) {
    // Default icon has already been set.
    return;
  }

  const std::optional<size_t> index_in_menu = GetIndexOfCommandId(command_id);
  CHECK(index_in_menu.has_value(), base::NotFatalUntil::M162);
  SetIcon(index_in_menu.value(), ui::ImageModel::FromImage(image_result.image));

  ui::MenuModelDelegate* delegate = menu_model_delegate();
  if (delegate) {
    delegate->OnIconChanged(command_id);
  }
}

}  // namespace tab_groups
