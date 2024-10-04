// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "base/uuid.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/widget/widget.h"

namespace {
static constexpr int kUIUpdateIconSize = 16;

// TODO(pengchaocai): Explore a generic command id solution if there are more
// use cases than app menu.
static constexpr int kMinCommandId = AppMenuModel::kMinTabGroupsCommandId;
static constexpr int kGap = AppMenuModel::kNumUnboundedMenuTypes;

void AddModelToParent(ui::MenuModel* model, views::MenuItemView* parent) {
  for (size_t i = 0, max = model->GetItemCount(); i < max; ++i) {
    views::MenuModelAdapter::AppendMenuItemFromModel(model, i, parent,
                                                     model->GetCommandIdAt(i));
  }
}
}  // namespace

namespace tab_groups {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGEverythingMenu, kCreateNewTabGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGEverythingMenu, kTabGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGEverythingMenu, kOpenGroup);

STGEverythingMenu::Action::Action(Type type,
                                  std::variant<base::Uuid, GURL> element)
    : type(type), element(element) {}
STGEverythingMenu::Action::Action(const Action& action) = default;
STGEverythingMenu::Action::~Action() = default;

STGEverythingMenu::STGEverythingMenu(views::MenuButtonController* controller,
                                     Browser* browser)
    : menu_button_controller_(controller),
      browser_(browser),
      widget_(views::Widget::GetWidgetForNativeWindow(
          browser->window()->GetNativeWindow())) {}

int STGEverythingMenu::GenerateTabGroupCommandID(int idx_in_sorted_tab_groups) {
  latest_tab_group_command_id_ =
      kMinCommandId + kGap * idx_in_sorted_tab_groups;
  return latest_tab_group_command_id_;
}

base::Uuid STGEverythingMenu::GetTabGroupIdFromCommandId(int command_id) {
  const int idx_in_sorted_tab_group = (command_id - kMinCommandId) / kGap;
  return sorted_non_empty_tab_groups_.at(idx_in_sorted_tab_group);
}

std::vector<base::Uuid>
STGEverythingMenu::GetGroupsForDisplaySortedByCreationTime(
    TabGroupSyncService* tab_group_service) {
  CHECK(tab_group_service);
  std::vector<base::Uuid> sorted_tab_groups;
  for (const SavedTabGroup& group : tab_group_service->GetAllGroups()) {
    if (group.saved_tabs().empty()) {
      continue;
    }

    sorted_tab_groups.push_back(group.saved_guid());
  }
  auto compare_by_creation_time = [=](const base::Uuid& a,
                                      const base::Uuid& b) {
    const std::optional<SavedTabGroup> saved_tab_group_a =
        tab_group_service->GetGroup(a);
    const std::optional<SavedTabGroup> saved_tab_group_b =
        tab_group_service->GetGroup(b);

    // If either gets deleted while creating the model, ignore the order.
    if (!saved_tab_group_a.has_value() || !saved_tab_group_b.has_value()) {
      return false;
    }

    return saved_tab_group_a->creation_time_windows_epoch_micros() >
           saved_tab_group_b->creation_time_windows_epoch_micros();
  };
  std::sort(sorted_tab_groups.begin(), sorted_tab_groups.end(),
            compare_by_creation_time);
  return sorted_tab_groups;
}

std::unique_ptr<ui::SimpleMenuModel> STGEverythingMenu::CreateMenuModel() {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddItemWithIcon(
      IDC_CREATE_NEW_TAB_GROUP,
      l10n_util::GetStringUTF16(IDS_CREATE_NEW_TAB_GROUP),
      ui::ImageModel::FromVectorIcon(kCreateNewTabGroupIcon, ui::kColorMenuIcon,
                                     kUIUpdateIconSize));
  menu_model->SetElementIdentifierAt(
      menu_model->GetIndexOfCommandId(IDC_CREATE_NEW_TAB_GROUP).value(),
      kCreateNewTabGroup);

  TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());
  if (!tab_group_service->GetAllGroups().empty()) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  sorted_non_empty_tab_groups_ =
      GetGroupsForDisplaySortedByCreationTime(tab_group_service);

  const auto* const color_provider = browser_->window()->GetColorProvider();
  for (size_t i = 0; i < sorted_non_empty_tab_groups_.size(); ++i) {
    const std::optional<SavedTabGroup> tab_group =
        tab_group_service->GetGroup(sorted_non_empty_tab_groups_[i]);
    // In case any tab group gets deleted while creating the model.
    if (!tab_group) {
      continue;
    }
    const auto color_id = GetTabGroupDialogColorId(tab_group->color());
    const auto group_icon = ui::ImageModel::FromVectorIcon(
        kTabGroupIcon, color_provider->GetColor(color_id), gfx::kFaviconSize);
    const auto title = tab_group->title();
    // For saved tab group items, use the indice in `sorted_tab_groups_` as the
    // command ids.
    const int command_id = GenerateTabGroupCommandID(i);

    // Tab group items in the app menu have submenus but the Everything button
    // in bookmarks bar has normal tab groups items which show context menus
    // with right click.
    if (show_submenu_) {
      menu_model->AddSubMenuWithIcon(
          command_id,
          title.empty() ? l10n_util::GetPluralStringFUTF16(
                              IDS_SAVED_TAB_GROUP_TABS_COUNT,
                              static_cast<int>(tab_group->saved_tabs().size()))
                        : title,
          nullptr, group_icon);
    } else {
      menu_model->AddItemWithIcon(
          command_id,
          title.empty() ? l10n_util::GetPluralStringFUTF16(
                              IDS_SAVED_TAB_GROUP_TABS_COUNT,
                              static_cast<int>(tab_group->saved_tabs().size()))
                        : title,
          group_icon);
    }

    // Set the first tab group item with element id `kTabGroup`.
    if (i == 0) {
      menu_model->SetElementIdentifierAt(
          menu_model->GetIndexOfCommandId(command_id).value(), kTabGroup);
    }
  }
  return menu_model;
}

int STGEverythingMenu::GetAndIncrementLatestCommandId() {
  return latest_tab_group_command_id_ += kGap;
}

bool STGEverythingMenu::ShouldEnableCommand(int command_id) {
  auto it = command_id_to_action_.find(command_id);
  if (it != command_id_to_action_.end() &&
      it->second.type == Action::Type::OPEN_OR_MOVE_TO_NEW_WINDOW) {
    return should_enable_move_menu_item_;
  }
  return true;
}

void STGEverythingMenu::PopulateTabGroupSubMenu(views::MenuItemView* parent) {
  // Repopulate the submenu each time the user hovers the tab group. `parent` is
  // owned by the AppMenu, it keeps unchanged throughout the session of the
  // expanded "Tab groups" item. If not cleared, the user will see duplicate
  // submenus for the same tab group accumulate.
  if (parent->HasSubmenu()) {
    parent->GetSubmenu()->RemoveAllChildViews();
  }

  submenu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  int parent_command_id = parent->GetCommand();
  auto group_id = GetTabGroupIdFromCommandId(parent_command_id);
  TabGroupSyncService* tab_group_service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(browser_->profile());

  const std::optional<SavedTabGroup> saved_group =
      tab_group_service->GetGroup(group_id);
  // If the group has been deleted remotely.
  if (!saved_group.has_value()) {
    return;
  }
  const auto& local_group_id = saved_group->local_group_id();

  // Clear map.
  command_id_to_action_.clear();

  // Add item: open in browser.
  int latest_command_id = GetAndIncrementLatestCommandId();
  submenu_model_->AddItemWithStringIdAndIcon(
      latest_command_id, IDS_OPEN_GROUP_IN_BROWSER_MENU,
      ui::ImageModel::FromVectorIcon(kOpenInBrowserIcon, ui::kColorMenuIcon,
                                     kUIUpdateIconSize));
  submenu_model_->SetElementIdentifierAt(
      submenu_model_->GetIndexOfCommandId(latest_command_id).value(),
      kOpenGroup);
  command_id_to_action_.emplace(
      latest_command_id, Action{Action::Type::OPEN_IN_BROWSER, group_id});

  // Add item: open or move to new window.
  const std::u16string move_or_open_group_text =
      local_group_id.has_value()
          ? l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW)
          : l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW);
  if (local_group_id.has_value()) {
    const Browser* const browser_with_local_group_id =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id.value());
    const TabStripModel* const tab_strip_model =
        browser_with_local_group_id->tab_strip_model();

    // Show the menu item if there are tabs outside of the saved group.
    should_enable_move_menu_item_ =
        tab_strip_model->count() != tab_strip_model->group_model()
                                        ->GetTabGroup(local_group_id.value())
                                        ->tab_count();
  }
  latest_command_id = GetAndIncrementLatestCommandId();
  submenu_model_->AddItemWithIcon(
      latest_command_id, move_or_open_group_text,
      ui::ImageModel::FromVectorIcon(kMoveGroupToNewWindowRefreshIcon,
                                     ui::kColorMenuIcon, kUIUpdateIconSize));
  submenu_model_->SetElementIdentifierAt(
      submenu_model_->GetIndexOfCommandId(latest_command_id).value(),
      SavedTabGroupUtils::kMoveGroupToNewWindowMenuItem);
  command_id_to_action_.emplace(
      latest_command_id,
      Action{Action::Type::OPEN_OR_MOVE_TO_NEW_WINDOW, group_id});

  // Add item: pin or unpin.
  latest_command_id = GetAndIncrementLatestCommandId();
  bool group_pinned = saved_group->is_pinned();
  submenu_model_->AddItemWithStringIdAndIcon(
      latest_command_id,
      group_pinned ? IDS_TAB_GROUP_HEADER_CXMENU_UNPIN_GROUP
                   : IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP,
      ui::ImageModel::FromVectorIcon(group_pinned ? kKeepFilledIcon : kKeepIcon,
                                     ui::kColorMenuIcon, kUIUpdateIconSize));
  submenu_model_->SetElementIdentifierAt(
      submenu_model_->GetIndexOfCommandId(latest_command_id).value(),
      SavedTabGroupUtils::kToggleGroupPinStateMenuItem);
  command_id_to_action_.emplace(
      latest_command_id, Action{Action::Type::PIN_OR_UNPIN_GROUP, group_id});

  // Add item: delete group.
  latest_command_id = GetAndIncrementLatestCommandId();
  submenu_model_->AddItemWithStringIdAndIcon(
      latest_command_id, IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP,
      ui::ImageModel::FromVectorIcon(kCloseGroupRefreshIcon, ui::kColorMenuIcon,
                                     kUIUpdateIconSize));
  submenu_model_->SetElementIdentifierAt(
      submenu_model_->GetIndexOfCommandId(latest_command_id).value(),
      SavedTabGroupUtils::kDeleteGroupMenuItem);
  command_id_to_action_.emplace(latest_command_id,
                                Action{Action::Type::DELETE_GROUP, group_id});

  // Add a separator and title.
  submenu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  submenu_model_->AddTitleWithStringId(IDS_TABS_TITLE_CXMENU);
  submenu_model_->SetElementIdentifierAt(
      submenu_model_->GetIndexOfCommandId(ui::MenuModel::kTitleId).value(),
      SavedTabGroupUtils::kTabsTitleItem);

  // Append open urls.
  const auto& tabs = saved_group->saved_tabs();
  for (size_t i = 0; i < tabs.size(); ++i) {
    const auto& tab = tabs.at(i);
    const ui::ImageModel& image =
        tab.favicon().has_value()
            ? ui::ImageModel::FromImage(tab.favicon().value())
            : favicon::GetDefaultFaviconModel(
                  GetTabGroupBookmarkColorId(saved_group->color()));
    const std::u16string title =
        tab.title().empty() ? base::UTF8ToUTF16(tab.url().spec()) : tab.title();

    latest_command_id = GetAndIncrementLatestCommandId();
    submenu_model_->AddItemWithIcon(latest_command_id, title, image);
    command_id_to_action_.emplace(latest_command_id,
                                  Action{Action::Type::OPEN_URL, tab.url()});
    // Assign an element identifier to the first tab.
    if (i == 0) {
      submenu_model_->SetElementIdentifierAt(
          submenu_model_->GetIndexOfCommandId(latest_command_id).value(),
          SavedTabGroupUtils::kTab);
    }
  }

  AddModelToParent(submenu_model_.get(), parent);
  parent->GetSubmenu()->InvalidateLayout();
}

void STGEverythingMenu::PopulateMenu(views::MenuItemView* parent) {
  if (parent->HasSubmenu()) {
    parent->GetSubmenu()->RemoveAllChildViews();
  }
  model_ = CreateMenuModel();
  AddModelToParent(model_.get(), parent);
  parent->GetSubmenu()->InvalidateLayout();
}

void STGEverythingMenu::RunMenu() {
  auto root = std::make_unique<views::MenuItemView>(this);
  PopulateMenu(root.get());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(
      widget_, menu_button_controller_,
      menu_button_controller_->button()->GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE);
}

void STGEverythingMenu::ExecuteCommand(int command_id, int event_flags) {
  auto it = command_id_to_action_.find(command_id);
  if (it != command_id_to_action_.end()) {
    auto type = it->second.type;
    if (type == Action::Type::OPEN_URL) {
      SavedTabGroupUtils::OpenUrlToBrowser(browser_,
                                           std::get<GURL>(it->second.element));
      return;
    }

    auto uuid = std::get<base::Uuid>(it->second.element);
    switch (type) {
      case Action::Type::OPEN_IN_BROWSER: {
        base::RecordAction(base::UserMetricsAction(
            "TabGroups_SavedTabGroups_OpenedFromEverythingMenu"));
        TabGroupSyncService* tab_group_service =
            tab_groups::SavedTabGroupUtils::GetServiceForProfile(
                browser_->profile());
        tab_group_service->OpenTabGroup(
            uuid, std::make_unique<TabGroupActionContextDesktop>(
                      browser_, OpeningSource::kOpenedFromRevisitUi));
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
      default:
        break;
    }
  } else if (command_id == IDC_CREATE_NEW_TAB_GROUP) {
    base::RecordAction(base::UserMetricsAction(
        "TabGroups_SavedTabGroups_CreateNewGroupTriggeredFromEverythingMenu"));
    browser_->command_controller()->ExecuteCommand(command_id);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "TabGroups_SavedTabGroups_OpenedFromEverythingMenu"));
    const auto group_id = GetTabGroupIdFromCommandId(command_id);
    TabGroupSyncService* tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(
            browser_->profile());
    tab_group_service->OpenTabGroup(
        group_id, std::make_unique<TabGroupActionContextDesktop>(
                      browser_, OpeningSource::kOpenedFromRevisitUi));
  }
}

bool STGEverythingMenu::ShowContextMenu(views::MenuItemView* source,
                                        int command_id,
                                        const gfx::Point& p,
                                        ui::MenuSourceType source_type) {
  if (command_id == IDC_CREATE_NEW_TAB_GROUP) {
    return false;
  }
  base::RecordAction(base::UserMetricsAction(
      "TabGroups_SavedTabGroups_ContextMenuTriggeredFromEverythingMenu"));
  const auto group_id = GetTabGroupIdFromCommandId(command_id);
  context_menu_controller_ =
      std::make_unique<views::DialogModelContextMenuController>(
          widget_->GetRootView(),
          base::BindRepeating(
              &SavedTabGroupUtils::CreateSavedTabGroupContextMenuModel,
              browser_, group_id),
          views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED);
  context_menu_controller_->ShowContextMenuForViewImpl(widget_->GetRootView(),
                                                       p, source_type);
  return true;
}

STGEverythingMenu::~STGEverythingMenu() = default;

}  // namespace tab_groups
