// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"

#include <memory>
#include <optional>

#include "base/metrics/user_metrics.h"
#include "base/uuid.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_metrics.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_tabs_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
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

// Provides the ui::MenuModelDelegate implementation for the sub menu
class STGEverythingMenu::AppMenuSubMenuModelDelegate
    : public ui::MenuModelDelegate,
      public views::ViewObserver {
 public:
  AppMenuSubMenuModelDelegate(ui::MenuModel* model,
                              views::MenuItemView* menu_item)
      : model_(model), menu_item_(menu_item) {
    model_->SetMenuModelDelegate(this);
    menu_item_->GetSubmenu()->AddObserver(this);
  }

  AppMenuSubMenuModelDelegate(const AppMenuSubMenuModelDelegate&) = delete;
  AppMenuSubMenuModelDelegate& operator=(const AppMenuSubMenuModelDelegate&) =
      delete;

  ~AppMenuSubMenuModelDelegate() override {
    if (menu_item_) {
      menu_item_->GetSubmenu()->RemoveObserver(this);
    }
    if (model_) {
      model_->SetMenuModelDelegate(nullptr);
    }
  }

  // ui::MenuModelDelegate implementation:
  void OnIconChanged(int command_id) override {
    ui::MenuModel* model = model_;
    size_t index;
    model_->GetModelAndIndexForCommandId(command_id, &model, &index);
    views::MenuItemView* item = menu_item_->GetMenuItemByID(command_id);
    CHECK(item);
    item->SetIcon(model->GetIconAt(index));
  }

  void OnMenuClearingDelegate() override { model_ = nullptr; }

  // views::ViewObserver implementation:
  void OnViewIsDeleting(views::View* observed_view) override {
    if (model_) {
      model_->SetMenuModelDelegate(nullptr);
    }
    menu_item_->GetSubmenu()->RemoveObserver(this);
    menu_item_ = nullptr;
  }

 private:
  raw_ptr<ui::MenuModel> model_;
  raw_ptr<views::MenuItemView> menu_item_;
};

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

    return saved_tab_group_a->creation_time() >
           saved_tab_group_b->creation_time();
  };
  std::sort(sorted_tab_groups.begin(), sorted_tab_groups.end(),
            compare_by_creation_time);
  return sorted_tab_groups;
}

std::unique_ptr<ui::SimpleMenuModel> STGEverythingMenu::CreateMenuModel(
    TabGroupSyncService* tab_group_service) {
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddItemWithIcon(
      IDC_CREATE_NEW_TAB_GROUP,
      l10n_util::GetStringUTF16(IDS_CREATE_NEW_TAB_GROUP),
      ui::ImageModel::FromVectorIcon(kCreateNewTabGroupIcon, ui::kColorMenuIcon,
                                     kUIUpdateIconSize));
  menu_model->SetElementIdentifierAt(
      menu_model->GetIndexOfCommandId(IDC_CREATE_NEW_TAB_GROUP).value(),
      kCreateNewTabGroup);

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
    auto title = tab_group->title();
    if (title.empty()) {
      title = l10n_util::GetPluralStringFUTF16(
          IDS_SAVED_TAB_GROUP_TABS_COUNT,
          static_cast<int>(tab_group->saved_tabs().size()));
    }
    // For saved tab group items, use the indice in `sorted_tab_groups_` as the
    // command ids.
    const int command_id = GenerateTabGroupCommandID(i);

    // Tab group items in the app menu have submenus but the Everything button
    // in bookmarks bar has normal tab groups items which show context menus
    // with right click.
    if (show_submenu_) {
      menu_model->AddSubMenuWithIcon(command_id, title, nullptr, group_icon);
    } else {
      menu_model->AddItemWithIcon(command_id, title, group_icon);
    }

    std::optional<int> index = menu_model->GetIndexOfCommandId(command_id);
    CHECK(index);

    menu_model->SetMayHaveMnemonicsAt(index.value(), false);

    if (tab_group->is_shared_tab_group()) {
      menu_model->SetMinorIcon(index.value(),
                               ui::ImageModel::FromVectorIcon(
                                   kPeopleGroupIcon, ui::kColorMenuIcon,
                                   ui::SimpleMenuModel::kDefaultIconSize));
    }

    std::u16string shared_state =
        tab_group->is_shared_tab_group()
            ? l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_SHARED)
            : u"";
    std::u16string open_state =
        tab_group->local_group_id().has_value()
            ? l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_OPENED)
            : l10n_util::GetStringUTF16(IDS_SAVED_GROUP_AX_LABEL_CLOSED);
    menu_model->SetAccessibleNameAt(
        index.value(),
        l10n_util::GetStringFUTF16(IDS_GROUP_AX_LABEL_GROUP_MENU_ITEM_FORMAT,
                                   shared_state, title, open_state));

    // Set the first tab group item with element id `kTabGroup`.
    if (i == 0) {
      menu_model->SetElementIdentifierAt(index.value(), kTabGroup);
    }
  }
  return menu_model;
}

int STGEverythingMenu::GetAndIncrementLatestCommandId() {
  return latest_tab_group_command_id_ += kGap;
}

bool STGEverythingMenu::ShouldEnableCommand(int command_id) {
  if (latest_group_id_ &&
      tabs_models_[latest_group_id_.value()]->HasCommandId(command_id)) {
    return tabs_models_[latest_group_id_.value()]->IsCommandIdEnabled(
        command_id);
  }
  return true;
}

void STGEverythingMenu::PopulateTabGroupSubMenu(views::MenuItemView* parent) {
  base::Uuid group_id =
      GetTabGroupIdFromCommandId(/*command_id=*/parent->GetCommand());

  if (latest_group_id_ == group_id) {
    return;
  }

  latest_group_id_ = group_id;

  submenu_delegate_ = std::make_unique<AppMenuSubMenuModelDelegate>(
      tabs_models_[group_id].get(), parent);

  if (!parent->HasSubmenu() || parent->GetSubmenu()->children().empty()) {
    AddModelToParent(tabs_models_[group_id].get(), parent);
  }
}

void STGEverythingMenu::PopulateMenu(views::MenuItemView* parent) {
  if (!groups_model_) {
    TabGroupSyncService* tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(
            browser_->profile());

    // Only recreate the model if we have to.
    groups_model_ = CreateMenuModel(tab_group_service);

    for (const auto& group_guid : sorted_non_empty_tab_groups_) {
      const std::optional<SavedTabGroup> tab_group =
          tab_group_service->GetGroup(group_guid);

      auto tabs_model = std::make_unique<STGTabsMenuModel>(this, browser_);
      tabs_model->Build(tab_group.value(),
                        base::BindRepeating(
                            &STGEverythingMenu::GetAndIncrementLatestCommandId,
                            base::Unretained(this)));

      tabs_models_.emplace(group_guid, std::move(tabs_model));
    }

    AddModelToParent(groups_model_.get(), parent);
    latest_group_id_ = std::nullopt;
  }
}

void STGEverythingMenu::RunMenu() {
  auto root = std::make_unique<views::MenuItemView>(this);
  PopulateMenu(root.get());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(
      widget_, menu_button_controller_,
      menu_button_controller_->button()->GetAnchorBoundsInScreen(),
      views::MenuAnchorPosition::kTopLeft, ui::mojom::MenuSourceType::kNone);
}

void STGEverythingMenu::ExecuteCommand(int command_id, int event_flags) {
  if (latest_group_id_ &&
      tabs_models_[latest_group_id_.value()]->HasCommandId(command_id)) {
    tabs_models_[latest_group_id_.value()]->ExecuteCommand(command_id,
                                                           event_flags);
  } else if (command_id == IDC_CREATE_NEW_TAB_GROUP) {
    if (show_submenu_) {
      base::RecordAction(base::UserMetricsAction(
          "TabGroups_SavedTabGroups_"
          "CreateNewGroupTriggeredFromTabGroupsAppMenu"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "TabGroups_SavedTabGroups_CreateNewGroupTriggeredFromEverythingMenu_"
          "2"));
    }

    browser_->command_controller()->ExecuteCommand(command_id);
  } else {
    base::RecordAction(base::UserMetricsAction(
        "TabGroups_SavedTabGroups_OpenedFromEverythingMenu_2"));
    const auto group_id = GetTabGroupIdFromCommandId(command_id);
    TabGroupSyncService* tab_group_service =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(
            browser_->profile());

    bool will_open_shared_group = false;
    if (std::optional<tab_groups::SavedTabGroup> saved_group =
            tab_group_service->GetGroup(group_id)) {
      will_open_shared_group = !saved_group->local_group_id().has_value() &&
                               saved_group->is_shared_tab_group();
    }

    tab_group_service->OpenTabGroup(
        group_id, std::make_unique<TabGroupActionContextDesktop>(
                      browser_, OpeningSource::kOpenedFromRevisitUi));

    if (will_open_shared_group) {
      saved_tab_groups::metrics::RecordSharedTabGroupRecallType(
          saved_tab_groups::metrics::SharedTabGroupRecallTypeDesktop::
              kOpenedFromEverythingMenu);
    }
  }
}

bool STGEverythingMenu::ShowContextMenu(views::MenuItemView* source,
                                        int command_id,
                                        const gfx::Point& p,
                                        ui::mojom::MenuSourceType source_type) {
  if (command_id == IDC_CREATE_NEW_TAB_GROUP) {
    return false;
  }
  base::RecordAction(base::UserMetricsAction(
      "TabGroups_SavedTabGroups_ContextMenuTriggeredFromEverythingMenu"));
  const base::Uuid group_id = GetTabGroupIdFromCommandId(command_id);

  latest_group_id_ = group_id;

  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      tabs_models_[group_id].get(),
      views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED);

  context_menu_runner_->RunMenuAt(
      widget_, /*button_controller=*/nullptr, gfx::Rect(p, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, source_type);
  return true;
}

bool STGEverythingMenu::GetAccelerator(int id,
                                       ui::Accelerator* accelerator) const {
  if (id == IDC_CREATE_NEW_TAB_GROUP) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    CHECK(browser_view);
    return browser_view->GetAccelerator(id, accelerator);
  }

  return false;
}

STGEverythingMenu::~STGEverythingMenu() = default;

}  // namespace tab_groups
