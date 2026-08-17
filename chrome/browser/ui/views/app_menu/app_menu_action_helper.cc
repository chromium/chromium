// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_action_helper.h"

#include <memory>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(app_menu::DisplayType)

DEFINE_UI_CLASS_PROPERTY_KEY(app_menu::DisplayType,
                             kAppMenuDisplayTypeInternal,
                             app_menu::DisplayType::kRow)

DEFINE_UI_CLASS_PROPERTY_KEY(ui::ColorId,
                             kAppMenuContainerColorInternal,
                             ui::kColorMenuBackground)

namespace app_menu {

const ui::ClassProperty<DisplayType>* const kAppMenuDisplayTypeKey =
    kAppMenuDisplayTypeInternal;

const ui::ClassProperty<ui::ColorId>* const kAppMenuContainerColorKey =
    kAppMenuContainerColorInternal;

// Creates the Indirect Action Item which is the basis for the app menu in
//  order to preserve hierarchy in action items
std::unique_ptr<actions::IndirectActionItem> CreateAppMenuIndirectActionItem(
    actions::ActionId action_id,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(action_id);
  if (!action) {
    return nullptr;
  }

  action->SetProperty(kAppMenuDisplayTypeKey, display_type);

  if (container_color.has_value()) {
    action->SetProperty(kAppMenuContainerColorKey, container_color.value());
  }

  return std::make_unique<actions::IndirectActionItem>(action);
}

// Creates the Action Item for the headers of each section in the app menu
std::unique_ptr<AppMenuSectionActionItem> CreateAppMenuSectionActionItem(
    std::u16string text,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color) {
  auto section_item = std::make_unique<AppMenuSectionActionItem>(text);

  section_item->SetProperty(kAppMenuDisplayTypeKey, display_type);

  if (container_color.has_value()) {
    section_item->SetProperty(kAppMenuContainerColorKey,
                              container_color.value());
  }

  return section_item;
}

actions::ActionItem* GetAppMenuRoot(
    BrowserWindowInterface* browser_window_interface) {
  return actions::ActionManager::Get().FindAction(
      kActionAppMenuRoot,
      BrowserActions::From(browser_window_interface)->root_action_item());
}

}  // namespace app_menu
