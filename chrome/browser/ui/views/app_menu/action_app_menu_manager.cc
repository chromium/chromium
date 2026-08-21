// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_app_menu_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"
#include "chrome/browser/ui/views/toolbar/recent_tabs_dynamic_menu.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ActionAppMenuManager::DisplayType)
DEFINE_UI_CLASS_PROPERTY_TYPE(ui::ImageModel*)

DEFINE_UI_CLASS_PROPERTY_KEY(ActionAppMenuManager::DisplayType,
                             kAppMenuDisplayTypeInternal,
                             ActionAppMenuManager::DisplayType::kRow)

DEFINE_UI_CLASS_PROPERTY_KEY(ui::ColorId,
                             kAppMenuContainerColorInternal,
                             ui::kColorMenuBackground)

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::u16string, kAppMenuTextOverrideInternal)
DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(ui::ImageModel, kAppMenuIconOverrideInternal)

const ui::ClassProperty<ActionAppMenuManager::DisplayType>* const
    ActionAppMenuManager::kDisplayTypeKey = kAppMenuDisplayTypeInternal;

const ui::ClassProperty<ui::ColorId>* const
    ActionAppMenuManager::kContainerColorKey = kAppMenuContainerColorInternal;

const ui::ClassProperty<std::u16string*>* const
    ActionAppMenuManager::kTextOverrideKey = kAppMenuTextOverrideInternal;

const ui::ClassProperty<ui::ImageModel*>* const
    ActionAppMenuManager::kIconOverrideKey = kAppMenuIconOverrideInternal;

// Creates the Indirect Action Item which is the basis for the app menu in
// order to preserve hierarchy in action items
std::unique_ptr<actions::IndirectActionItem>
ActionAppMenuManager::CreateIndirectActionItem(
    actions::ActionId action_id,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color,
    std::optional<std::u16string> text_override,
    std::optional<ui::ImageModel> icon_override) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(action_id);
  if (!action) {
    return nullptr;
  }

  action->SetProperty(kDisplayTypeKey, display_type);

  if (container_color.has_value()) {
    action->SetProperty(kContainerColorKey, container_color.value());
  }

  auto item = std::make_unique<actions::IndirectActionItem>(action);

  if (text_override.has_value()) {
    item->SetProperty(kTextOverrideKey,
                      std::make_unique<std::u16string>(text_override.value()));
  }

  if (icon_override.has_value()) {
    item->SetProperty(kIconOverrideKey,
                      std::make_unique<ui::ImageModel>(icon_override.value()));
  }

  return item;
}

// Creates the Action Item for the headers of each section in the app menu
std::unique_ptr<AppMenuSectionActionItem>
ActionAppMenuManager::CreateSectionActionItem(
    std::u16string text,
    DisplayType display_type,
    std::optional<ui::ColorId> container_color) {
  auto section_item = std::make_unique<AppMenuSectionActionItem>(text);

  section_item->SetProperty(kDisplayTypeKey, display_type);

  if (container_color.has_value()) {
    section_item->SetProperty(kContainerColorKey, container_color.value());
  }

  return section_item;
}

actions::ActionItem* ActionAppMenuManager::GetAppMenuRoot(
    BrowserWindowInterface* browser_window_interface) {
  return actions::ActionManager::Get().FindAction(
      kActionAppMenuRoot,
      BrowserActions::From(browser_window_interface)->root_action_item());
}

ActionAppMenuManager::ActionAppMenuManager(
    BrowserWindowInterface* browser_window_interface)
    : browser_window_interface_(browser_window_interface),
      recent_tabs_menu_(
          std::make_unique<RecentTabsDynamicMenu>(browser_window_interface)),
      bookmarks_menu_(
          std::make_unique<BookmarksDynamicMenu>(browser_window_interface)) {}

ActionAppMenuManager::~ActionAppMenuManager() = default;

actions::ActionItem* ActionAppMenuManager::GetAppMenuRoot() const {
  return GetAppMenuRoot(browser_window_interface_);
}

void ActionAppMenuManager::CreateMenuHierarchy() {
  actions::ActionItem* root = GetAppMenuRoot();
  if (!root) {
    return;
  }

  std::optional<ui::ColorId> your_chrome_background =
      kColorAppMenuYourChromeBackground;
  std::optional<ui::ColorId> tools_actions_background =
      kColorAppMenuToolsAndActionsBackground;

  // Block Header Setup
  root->AddChild(CreateIndirectActionItem(
      kActionNewTab, DisplayType::kBlock,
      /*container_color=*/std::nullopt,
      /*text_override=*/std::nullopt,
      /*icon_override=*/
      ui::ImageModel::FromVectorIcon(
          features::IsRoundedIconsEnabled() ? kTabIcon : kNewTabRefreshOldIcon,
          ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize)));

  root->AddChild(
      CreateIndirectActionItem(kActionNewWindow, DisplayType::kBlock));

  root->AddChild(CreateIndirectActionItem(
      kActionNewIncognitoWindow, DisplayType::kBlock,
      /*container_color=*/std::nullopt,
      /*text_override=*/l10n_util::GetStringUTF16(IDS_INCOGNITO)));

  // Chrome Heading (Your Chrome)
  std::unique_ptr<actions::BaseAction> your_chrome_heading =
      CreateSectionActionItem(
          l10n_util::GetStringUTF16(IDS_APP_MENU_YOUR_CHROME_HEADER),
          DisplayType::kRow, your_chrome_background);

  auto* chrome_ptr = root->AddChild(std::move(your_chrome_heading));

  // Your Chrome Children Setup
  chrome_ptr->AddChild(CreateIndirectActionItem(
      kActionShowPasswordManager, DisplayType::kRow, your_chrome_background));

  auto recent_tabs_menu = CreateIndirectActionItem(
      kActionRecentTabsSubmenu, DisplayType::kRow, your_chrome_background);

  recent_tabs_menu->SetPopulateChildrenCallback(
      base::BindRepeating(&RecentTabsDynamicMenu::BuildRecentTabsActions,
                          recent_tabs_menu_->GetWeakPtr()));

  recent_tabs_menu->PopulateChildItems();

  chrome_ptr->AddChild(std::move(recent_tabs_menu));

  chrome_ptr->AddChild(CreateIndirectActionItem(
      kActionShowDownloads, DisplayType::kRow, your_chrome_background));

  chrome_ptr->AddChild(CreateIndirectActionItem(
      kActionManageExtensions, DisplayType::kRow, your_chrome_background));

  auto bookmarks_submenu = CreateIndirectActionItem(
      kActionBookmarksSubmenu, DisplayType::kRow, your_chrome_background);

  bookmarks_submenu->AddChild(CreateIndirectActionItem(
      kActionBookmarkThisTab, DisplayType::kRow, your_chrome_background));

  bookmarks_submenu->AddChild(CreateIndirectActionItem(
      kActionBookmarkAllTabs, DisplayType::kRow, your_chrome_background));

  bookmarks_submenu->SetPopulateChildrenCallback(
      base::BindRepeating(&BookmarksDynamicMenu::BuildBookmarksActions,
                          bookmarks_menu_->GetWeakPtr()));

  bookmarks_submenu->PopulateChildItems();

  chrome_ptr->AddChild(std::move(bookmarks_submenu));

  chrome_ptr->AddChild(CreateIndirectActionItem(
      kActionClearBrowsingData, DisplayType::kRow, your_chrome_background));

  // Tools and Actions Heading
  std::unique_ptr<actions::BaseAction> tools_actions_heading =
      CreateSectionActionItem(
          l10n_util::GetStringUTF16(IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER),
          DisplayType::kRow, tools_actions_background);

  auto* tools_actions_ptr = root->AddChild(std::move(tools_actions_heading));

  // Tools and Actions Setup
  tools_actions_ptr->AddChild(CreateIndirectActionItem(
      kActionPrint, DisplayType::kRow, tools_actions_background));

  if (glic::GlicEnabling::IsEnabledForProfile(
          browser_window_interface_->GetProfile())) {
    tools_actions_ptr->AddChild(CreateIndirectActionItem(
        kActionOpenGlic, DisplayType::kRow, tools_actions_background));
  }

  tools_actions_ptr->AddChild(
      CreateIndirectActionItem(kActionShowLensOverlayFromAppMenu,
                               DisplayType::kRow, tools_actions_background));

  tools_actions_ptr->AddChild(CreateIndirectActionItem(
      kActionShowTranslate, DisplayType::kRow, tools_actions_background));

  tools_actions_ptr->AddChild(CreateIndirectActionItem(
      kActionFind, DisplayType::kRow, tools_actions_background));

  // Footer Setup
  root->AddChild(
      CreateIndirectActionItem(kActionOptions, DisplayType::kFooter));

  root->AddChild(
      CreateIndirectActionItem(kActionHelpSubmenu, DisplayType::kFooter));

  root->AddChild(CreateIndirectActionItem(kActionExit, DisplayType::kFooter));
}
