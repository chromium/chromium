// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_action_manager.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(DisplayType)

DEFINE_UI_CLASS_PROPERTY_KEY(DisplayType,
                             kAppMenuDisplayTypeInternal,
                             DisplayType::kRow)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::ColorId,
                             kAppMenuContainerColorInternal,
                             ui::kColorMenuBackground)

// static
const ui::ClassProperty<DisplayType>* const
    AppMenuActionManager::kAppMenuDisplayTypeKey = kAppMenuDisplayTypeInternal;

// static
const ui::ClassProperty<ui::ColorId>* const
    AppMenuActionManager::kAppMenuContainerColorKey =
        kAppMenuContainerColorInternal;

// Initializes the AppMenuActionManager with an optional search scope.
// When creating proxy items, CreateAppMenuProxyActionItem() will search inside
// this scope before checking the global ActionManager.
AppMenuActionManager::AppMenuActionManager(actions::ActionItem* action_scope)
    : action_scope_(action_scope) {}

AppMenuActionManager::~AppMenuActionManager() = default;

void AppMenuActionManager::Initialize() {
  root_action_item_ = actions::ActionItem::Builder().Build();
  PopulateAppMenu(root_action_item_.get());
}

void AppMenuActionManager::PopulateAppMenu(
    actions::ActionItem* root,
    std::optional<ui::ColorId> inherited_container_color) {
  std::optional<ui::ColorId> your_chrome_background =
      kColorAppMenuYourChromeBackground;
  std::optional<ui::ColorId> tools_actions_background =
      kColorAppMenuToolsAndActionsBackground;

  // Chrome Heading (Your Chrome)

  std::unique_ptr<actions::BaseAction> your_chrome_heading =
      CreateAppMenuSectionActionItem(
          l10n_util::GetStringUTF16(IDS_APP_MENU_YOUR_CHROME_HEADER),
          DisplayType::kRow, your_chrome_background);

  auto* chrome_ptr = root->AddChild(std::move(your_chrome_heading));

  // Your Chrome Children Setup
  chrome_ptr->AddChild(CreateAppMenuIndirectActionItem(
      kActionShowPasswordManager, DisplayType::kRow, your_chrome_background));

  chrome_ptr->AddChild(CreateAppMenuIndirectActionItem(
      kActionShowHistory, DisplayType::kRow, your_chrome_background));

  chrome_ptr->AddChild(CreateAppMenuIndirectActionItem(
      kActionManageExtensions, DisplayType::kRow, your_chrome_background));

  // Tools and Actions Heading
  std::unique_ptr<actions::BaseAction> tools_actions_heading =
      CreateAppMenuSectionActionItem(
          l10n_util::GetStringUTF16(IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER),
          DisplayType::kRow, tools_actions_background);

  auto* tools_actions_ptr = root->AddChild(std::move(tools_actions_heading));

  // Tools and Actions Setup
  tools_actions_ptr->AddChild(CreateAppMenuIndirectActionItem(
      kActionPrint, DisplayType::kRow, tools_actions_background));

  tools_actions_ptr->AddChild(CreateAppMenuIndirectActionItem(
      kActionFind, DisplayType::kRow, tools_actions_background));
}

std::unique_ptr<actions::IndirectActionItem>
AppMenuActionManager::CreateAppMenuIndirectActionItem(
    actions::ActionId action_id,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(action_id, action_scope_);
  if (!action && action_scope_) {
    action = actions::ActionManager::Get().FindAction(action_id);
  }
  if (!action) {
    return nullptr;
  }

  std::unique_ptr<actions::IndirectActionItem> indirect_item =
      std::make_unique<actions::IndirectActionItem>(action);
  indirect_item->GetActionItem()->SetProperty(kAppMenuDisplayTypeKey,
                                              display_type);
  if (container_color.has_value()) {
    indirect_item->GetActionItem()->SetProperty(kAppMenuContainerColorKey,
                                                container_color.value());
  }

  return indirect_item;
}

std::unique_ptr<AppMenuSectionActionItem>
AppMenuActionManager::CreateAppMenuSectionActionItem(
    std::u16string text,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color) {
  std::unique_ptr<AppMenuSectionActionItem> section_item =
      std::make_unique<AppMenuSectionActionItem>(text);
  section_item->SetProperty(kAppMenuDisplayTypeKey, display_type);
  if (container_color.has_value()) {
    section_item->SetProperty(kAppMenuContainerColorKey,
                              container_color.value());
  }
  return section_item;
}
