// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_everything_menu.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "ui/base/models/dialog_model.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/widget/widget.h"

namespace tab_groups {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGEverythingMenu, kCreateNewTabGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(STGEverythingMenu, kTabGroup);

STGEverythingMenu::STGEverythingMenu(views::MenuButtonController* controller,
                                     views::Widget* widget,
                                     Browser* browser)
    : menu_button_controller_(controller), browser_(browser), widget_(widget) {}

const SavedTabGroupModel*
STGEverythingMenu::GetSavedTabGroupModelFromBrowser() {
  CHECK(browser_);
  auto* profile = browser_->profile();
  CHECK(!profile->IsOffTheRecord());
  auto* keyed_service = SavedTabGroupServiceFactory::GetForProfile(profile);
  return keyed_service->model();
}

std::vector<const SavedTabGroup*>
STGEverythingMenu::GetSortedTabGroupsByCreationTime(
    const SavedTabGroupModel* stg_model) {
  CHECK(stg_model);
  std::vector<const SavedTabGroup*> sorted_tab_groups;
  for (const SavedTabGroup& group : stg_model->saved_tab_groups()) {
    sorted_tab_groups.push_back(&group);
  }
  auto compare_by_creation_time = [=](const SavedTabGroup* a,
                                      const SavedTabGroup* b) {
    return a->creation_time_windows_epoch_micros() >
           b->creation_time_windows_epoch_micros();
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
      ui::ImageModel::FromVectorIcon(kCreateNewTabGroupIcon));
  menu_model->SetElementIdentifierAt(
      menu_model->GetIndexOfCommandId(IDC_CREATE_NEW_TAB_GROUP).value(),
      kCreateNewTabGroup);

  const auto* stg_model = GetSavedTabGroupModelFromBrowser();
  if (!stg_model->IsEmpty()) {
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  }

  sorted_tab_groups_ = GetSortedTabGroupsByCreationTime(stg_model);
  const auto* const color_provider = browser_->window()->GetColorProvider();
  for (size_t i = 0; i < sorted_tab_groups_.size(); ++i) {
    const auto* const tab_group = sorted_tab_groups_[i];
    const auto color_id = GetTabGroupDialogColorId(tab_group->color());
    const auto group_icon = ui::ImageModel::FromVectorIcon(
        kTabGroupIcon, color_provider->GetColor(color_id), gfx::kFaviconSize);
    const auto title = tab_group->title();
    // For saved tab group items, use the indice in `sorted_tab_groups_` as the
    // command ids.
    menu_model->AddItemWithIcon(
        i /*command_id*/,
        title.empty() ? l10n_util::GetPluralStringFUTF16(
                            IDS_SAVED_TAB_GROUP_TABS_COUNT,
                            static_cast<int>(tab_group->saved_tabs().size()))
                      : title,
        group_icon);
    // Set the first tab group item with element id `kTabGroup`.
    if (i == 0) {
      menu_model->SetElementIdentifierAt(
          menu_model->GetIndexOfCommandId(0).value(), kTabGroup);
    }
  }
  return menu_model;
}

void STGEverythingMenu::PopulateMenu(views::MenuItemView* parent) {
  model_ = CreateMenuModel();
  for (size_t i = 0, max = model_->GetItemCount(); i < max; ++i) {
    views::MenuModelAdapter::AppendMenuItemFromModel(model_.get(), i, parent,
                                                     model_->GetCommandIdAt(i));
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
      views::MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE);
}

void STGEverythingMenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_CREATE_NEW_TAB_GROUP) {
    browser_->command_controller()->ExecuteCommand(command_id);
  } else {
    const auto* const group = sorted_tab_groups_[command_id];
    if (group->saved_tabs().empty()) {
      return;
    }
    auto* const keyed_service =
        SavedTabGroupServiceFactory::GetForProfile(browser_->profile());
    keyed_service->OpenSavedTabGroupInBrowser(browser_, group->saved_guid());
  }
}

bool STGEverythingMenu::ShowContextMenu(views::MenuItemView* source,
                                        int command_id,
                                        const gfx::Point& p,
                                        ui::MenuSourceType source_type) {
  if (command_id == IDC_CREATE_NEW_TAB_GROUP) {
    return false;
  }

  const auto* const group = sorted_tab_groups_[command_id];
  context_menu_controller_ =
      std::make_unique<views::DialogModelContextMenuController>(
          widget_->GetRootView(),
          base::BindRepeating(
              &SavedTabGroupUtils::CreateSavedTabGroupContextMenuModel,
              browser_, group->saved_guid()),
          views::MenuRunner::CONTEXT_MENU | views::MenuRunner::IS_NESTED);
  context_menu_controller_->ShowContextMenuForViewImpl(widget_->GetRootView(),
                                                       p, source_type);
  return true;
}

STGEverythingMenu::~STGEverythingMenu() = default;

}  // namespace tab_groups
