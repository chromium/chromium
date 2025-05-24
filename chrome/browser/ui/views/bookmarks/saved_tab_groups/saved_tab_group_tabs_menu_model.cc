// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"

#include <memory>
#include <optional>

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/widget/widget.h"

namespace tab_groups {

static constexpr int kUIUpdateIconSize = 16;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kDeleteGroupMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kLeaveGroupMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel,
                                      kMoveGroupToNewWindowMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kOpenGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel,
                                      kToggleGroupPinStateMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kTabsTitleItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGTabsMenuModel, kTab);

STGTabsMenuModel::Action::Action(Type type,
                                 std::variant<base::Uuid, GURL> element)
    : type(type), element(element) {}
STGTabsMenuModel::Action::Action(const Action& action) = default;
STGTabsMenuModel::Action::~Action() = default;

STGTabsMenuModel::STGTabsMenuModel(Browser* browser)
    : ui::SimpleMenuModel(this), browser_(browser) {}

STGTabsMenuModel::STGTabsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                                   Browser* browser)
    : ui::SimpleMenuModel(delegate), browser_(browser) {}

STGTabsMenuModel::~STGTabsMenuModel() = default;

void STGTabsMenuModel::Build(
    const SavedTabGroup& saved_group,
    base::RepeatingCallback<int()> get_next_command_id) {
  command_id_to_action_.clear();
  should_enable_move_menu_item_ = true;
  sync_id_ = saved_group.saved_guid();

  // Add item: open in browser.
  int latest_command_id = get_next_command_id.Run();
  AddItemWithStringIdAndIcon(
      latest_command_id, IDS_OPEN_GROUP_IN_BROWSER_MENU,
      ui::ImageModel::FromVectorIcon(kOpenInBrowserIcon, ui::kColorMenuIcon,
                                     kUIUpdateIconSize));
  SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                         kOpenGroup);
  command_id_to_action_.emplace(
      latest_command_id,
      Action{Action::Type::OPEN_IN_BROWSER, sync_id_.value()});

  // Add item: open or move to new window.
  const std::u16string move_or_open_group_text =
      saved_group.local_group_id().has_value()
          ? l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW)
          : l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW);
  latest_command_id = get_next_command_id.Run();
  AddItemWithIcon(
      latest_command_id, move_or_open_group_text,
      ui::ImageModel::FromVectorIcon(kMoveGroupToNewWindowRefreshIcon,
                                     ui::kColorMenuIcon, kUIUpdateIconSize));
  SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                         kMoveGroupToNewWindowMenuItem);
  command_id_to_action_.emplace(
      latest_command_id,
      Action{Action::Type::OPEN_OR_MOVE_TO_NEW_WINDOW, sync_id_.value()});

  // Add item: pin or unpin.
  latest_command_id = get_next_command_id.Run();
  bool group_pinned = saved_group.is_pinned();
  AddItemWithStringIdAndIcon(
      latest_command_id,
      group_pinned ? IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP
                   : IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP,
      ui::ImageModel::FromVectorIcon(group_pinned ? kKeepOffIcon : kKeepIcon,
                                     ui::kColorMenuIcon, kUIUpdateIconSize));
  SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                         kToggleGroupPinStateMenuItem);
  command_id_to_action_.emplace(
      latest_command_id,
      Action{Action::Type::PIN_OR_UNPIN_GROUP, sync_id_.value()});

  latest_command_id = get_next_command_id.Run();
  if (SavedTabGroupUtils::IsOwnerOfSharedTabGroup(browser_->profile(),
                                                  sync_id_.value())) {
    // Add item: delete group.
    AddItemWithStringIdAndIcon(
        latest_command_id, IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP,
        ui::ImageModel::FromVectorIcon(kCloseGroupRefreshIcon,
                                       ui::kColorMenuIcon, kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kDeleteGroupMenuItem);
    command_id_to_action_.emplace(
        latest_command_id,
        Action{Action::Type::DELETE_GROUP, sync_id_.value()});
  } else {
    // Add item: leave group.
    AddItemWithStringIdAndIcon(
        latest_command_id, IDS_DATA_SHARING_LEAVE_GROUP,
        ui::ImageModel::FromVectorIcon(kCloseGroupRefreshIcon,
                                       ui::kColorMenuIcon, kUIUpdateIconSize));
    SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                           kLeaveGroupMenuItem);
    command_id_to_action_.emplace(
        latest_command_id, Action{Action::Type::LEAVE_GROUP, sync_id_.value()});
  }

  // Add a separator and title.
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddTitleWithStringId(IDS_TABS_TITLE_CXMENU);
  SetElementIdentifierAt(GetIndexOfCommandId(ui::MenuModel::kTitleId).value(),
                         kTabsTitleItem);

  // Perform an async request for the favicon from the favicon service
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(browser_->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);

  // Append open urls.
  const auto& tabs = saved_group.saved_tabs();
  for (size_t i = 0; i < tabs.size(); ++i) {
    const auto& tab = tabs.at(i);
    const std::u16string title =
        tab.title().empty() ? base::UTF8ToUTF16(tab.url().spec()) : tab.title();

    latest_command_id = get_next_command_id.Run();
    const ui::ImageModel image = favicon::GetDefaultFaviconModel(
        GetTabGroupBookmarkColorId(saved_group.color()));
    AddItemWithIcon(latest_command_id, title, image);

    // Can be null for tests
    if (favicon_service) {
      favicon_service->GetFaviconImageForPageURL(
          tab.url(),
          base::BindOnce(&STGTabsMenuModel::OnFaviconDataAvailable,
                         weak_ptr_factory_.GetWeakPtr(), latest_command_id),
          &cancelable_task_tracker_);
    }

    command_id_to_action_.emplace(latest_command_id,
                                  Action{Action::Type::OPEN_URL, tab.url()});
    // Assign an element identifier to the first tab.
    if (i == 0) {
      SetElementIdentifierAt(GetIndexOfCommandId(latest_command_id).value(),
                             kTab);
    }
  }

  if (saved_group.local_group_id().has_value()) {
    const Browser* const browser_with_local_group_id =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(
            saved_group.local_group_id().value());
    const TabStripModel* const tab_strip_model =
        browser_with_local_group_id->tab_strip_model();

    // Show the menu item if there are tabs outside of the saved group.
    should_enable_move_menu_item_ =
        tab_strip_model->count() !=
        tab_strip_model->group_model()
            ->GetTabGroup(saved_group.local_group_id().value())
            ->tab_count();
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
  if (it->second.type == Action::Type::OPEN_OR_MOVE_TO_NEW_WINDOW) {
    return should_enable_move_menu_item_;
  }
  return true;
}

void STGTabsMenuModel::ExecuteCommand(int command_id, int event_flags) {
  auto it = command_id_to_action_.find(command_id);
  CHECK(it != command_id_to_action_.end());

  auto type = it->second.type;
  if (type == Action::Type::OPEN_URL) {
    SavedTabGroupUtils::OpenUrlInNewUngroupedTab(
        browser_, std::get<GURL>(it->second.element));
    return;
  }

  auto uuid = std::get<base::Uuid>(it->second.element);
  switch (type) {
    case Action::Type::OPEN_IN_BROWSER: {
      base::RecordAction(base::UserMetricsAction(
          "TabGroups_SavedTabGroups_OpenedFromTabGroupsAppMenu"));
      TabGroupSyncService* tab_group_service =
          tab_groups::SavedTabGroupUtils::GetServiceForProfile(
              browser_->profile());

      bool will_open_shared_group = false;
      if (std::optional<tab_groups::SavedTabGroup> saved_group =
              tab_group_service->GetGroup(uuid)) {
        will_open_shared_group = !saved_group->local_group_id().has_value() &&
                                 saved_group->is_shared_tab_group();
      }

      tab_group_service->OpenTabGroup(
          uuid, std::make_unique<TabGroupActionContextDesktop>(
                    browser_, OpeningSource::kOpenedFromRevisitUi));

      if (will_open_shared_group) {
        saved_tab_groups::metrics::RecordSharedTabGroupRecallType(
            saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::
                kOpenedFromSubmenu);
      }
      break;
    }
    case Action::Type::OPEN_OR_MOVE_TO_NEW_WINDOW:
      SavedTabGroupUtils::OpenOrMoveSavedGroupToNewWindow(browser_, uuid);
      break;
    case Action::Type::PIN_OR_UNPIN_GROUP:
      SavedTabGroupUtils::ToggleGroupPinState(browser_, uuid);
      break;
    case Action::Type::DELETE_GROUP:
      SavedTabGroupUtils::DeleteSavedGroup(browser_, uuid);
      break;
    case Action::Type::LEAVE_GROUP:
      SavedTabGroupUtils::LeaveSharedGroup(browser_, uuid);
      break;
    case Action::Type::OPEN_URL:
    case Action::Type::DEFAULT:
      break;
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
  DCHECK(index_in_menu.has_value());
  SetIcon(index_in_menu.value(), ui::ImageModel::FromImage(image_result.image));

  ui::MenuModelDelegate* delegate = menu_model_delegate();
  if (delegate) {
    delegate->OnIconChanged(command_id);
  }
}

}  // namespace tab_groups
