// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

#include <memory>
#include <string>

#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_group.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view_class_properties.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel, kNewTab);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel, kNewTabInGroup);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel, kNewSplitView);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(NewTabButtonMenuModel,
                                      kCreateNewTabGroup);

NewTabButton::NewTabButton(TabStripController* tab_strip,
                           PressedCallback callback,
                           const gfx::VectorIcon& icon,
                           Edge fixed_flat_edge,
                           Edge animated_flat_edge,
                           BrowserWindowInterface* browser)
    : TabStripControlButton(tab_strip,
                            std::move(callback),
                            icon,
                            fixed_flat_edge,
                            animated_flat_edge),
      browser_(browser) {
  set_context_menu_controller(this);
  SetProperty(views::kElementIdentifierKey, kNewTabButtonElementId);
}

NewTabButton::~NewTabButton() = default;

void NewTabButton::ShowContextMenuForViewImpl(
    View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (features::IsTabGroupMenuMoreEntryPointsEnabled()) {
    context_menu_model_ = std::make_unique<NewTabButtonMenuModel>(browser_);

    int32_t menu_runner_flags =
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU;

    context_menu_runner_ = std::make_unique<views::MenuRunner>(
        context_menu_model_.get(), menu_runner_flags);

    context_menu_runner_->RunMenuAt(
        source->GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
        views::MenuAnchorPosition::kTopLeft, source_type);
  }
}

NewTabButtonMenuModel::NewTabButtonMenuModel(BrowserWindowInterface* browser)
    : ui::SimpleMenuModel(this), browser_(browser) {
  CHECK(browser_);

  // Build the menu.
  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_NEW_TAB).value(), kNewTab);
  AddNewTabInGroupItem();

  AddSeparator(ui::NORMAL_SEPARATOR);

  AddItemWithStringId(IDC_CREATE_NEW_TAB_GROUP, IDS_NEW_TAB_GROUP);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_CREATE_NEW_TAB_GROUP).value(),
                         kCreateNewTabGroup);

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddNewSplitTabItem();
  }
}

NewTabButtonMenuModel::~NewTabButtonMenuModel() = default;

void NewTabButtonMenuModel::ExecuteCommand(int command_id, int event_flags) {
  CHECK(browser_);

  switch (command_id) {
    case IDC_NEW_TAB:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewTab"));
      break;
    case IDC_ADD_NEW_TAB_RECENT_GROUP:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewTabInGroup"));
      break;
    case IDC_CREATE_NEW_TAB_GROUP:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewGroup"));
      break;
    case IDC_NEW_SPLIT_TAB:
      base::RecordAction(
          base::UserMetricsAction("NewTabButton_ContextMenu_NewSplitTab"));
      break;
  }

  if (command_id == IDC_NEW_SPLIT_TAB) {
    // Handle this command directly because we want to specify the source
    // as the new tab button.
    chrome::NewSplitTab(browser_,
                        split_tabs::SplitTabCreatedSource::kNewTabButton);
    return;
  }

  chrome::ExecuteCommand(browser_, command_id);
}

bool NewTabButtonMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  CHECK(browser_);
  if (command_id < 0) {
    // This is a non-interactive title.
    return false;
  }

  return browser_->GetFeatures()
      .accelerator_provider()
      ->GetAcceleratorForCommandId(command_id, accelerator);
}

void NewTabButtonMenuModel::AddNewTabInGroupItem() {
  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  CHECK(tab_strip_model);

  TabGroupModel* tab_group_model = tab_strip_model->group_model();
  CHECK(tab_group_model);

  std::optional<tab_groups::TabGroupId> group_id =
      tab_group_model->GetMostRecentTabGroupId();

  if (!group_id) {
    // There is no most recent group. So we don't enable this option.
    AddItem(IDC_ADD_NEW_TAB_RECENT_GROUP,
            l10n_util::GetStringUTF16(IDS_NEW_TAB_IN_GROUP_NO_GROUPS));
    SetEnabledAt(GetIndexOfCommandId(IDC_ADD_NEW_TAB_RECENT_GROUP).value(),
                 false);
  } else {
    // The most recent tab group exists.
    std::u16string group_name =
        tab_group_model->GetTabGroup(*group_id)->visual_data()->title();

    std::u16string menu_item_label;

    if (group_name.empty()) {
      // "New tab in 2 tabs
      int num_tabs = tab_group_model->GetTabGroup(*group_id)->tab_count();
      menu_item_label = l10n_util::GetPluralStringFUTF16(
          IDS_NEW_TAB_IN_GROUP_NO_NAME, num_tabs);
    } else {
      // "New tab in |group_name|".
      menu_item_label =
          l10n_util::GetStringFUTF16(IDS_NEW_TAB_IN_GROUP, group_name);
    }
    AddItem(IDC_ADD_NEW_TAB_RECENT_GROUP, menu_item_label);
  }

  SetElementIdentifierAt(
      GetIndexOfCommandId(IDC_ADD_NEW_TAB_RECENT_GROUP).value(),
      kNewTabInGroup);
}

void NewTabButtonMenuModel ::AddNewSplitTabItem() {
  AddItemWithStringId(IDC_NEW_SPLIT_TAB, IDS_TAB_CXMENU_NEW_SPLIT_WITH_CURRENT);
  SetElementIdentifierAt(GetIndexOfCommandId(IDC_NEW_SPLIT_TAB).value(),
                         kNewSplitView);

  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  CHECK(tab_strip_model);

  SetEnabledAt(GetIndexOfCommandId(IDC_NEW_SPLIT_TAB).value(),
               !tab_strip_model->IsActiveTabSplit());
}
