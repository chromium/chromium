// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_MANAGER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"

class RecentTabsDynamicMenu;
class BookmarksDynamicMenu;
class TabGroupDynamicMenu;
class SendTabToSelfDynamicMenu;

// Manages the ActionItem hierarchy for the Action App Menu, including
// constructing the menu tree and managing dynamic submenus.
class ActionAppMenuManager {
 public:
  enum class DisplayType {
    kRow,
    kBlock,
    kFooter,
    kDivider,
    kSection,
    kSearch,
    kCustom,
  };

  static const ui::ClassProperty<DisplayType>* const kDisplayTypeKey;
  static const ui::ClassProperty<ui::ColorId>* const kContainerColorKey;
  static const ui::ClassProperty<std::u16string*>* const kTextOverrideKey;
  static const ui::ClassProperty<ui::ImageModel*>* const kIconOverrideKey;

  static std::unique_ptr<actions::IndirectActionItem> CreateIndirectActionItem(
      actions::ActionId action_id,
      DisplayType display_type,
      std::optional<ui::ColorId> container_color = std::nullopt,
      std::optional<std::u16string> text_override = std::nullopt,
      std::optional<ui::ImageModel> icon_override = std::nullopt);

  static std::unique_ptr<actions::ActionItem> CreateSectionActionItem(
      DisplayType display_type,
      std::optional<ui::ColorId> container_color = std::nullopt);

  static std::unique_ptr<actions::ActionItem> CreateSectionHeaderActionItem(
      std::u16string text,
      std::optional<ui::ColorId> container_color = std::nullopt);

  static std::unique_ptr<actions::ActionItem> CreateDividerActionItem();

  static actions::ActionItem* GetAppMenuRoot(
      BrowserWindowInterface* browser_window_interface);

  explicit ActionAppMenuManager(
      BrowserWindowInterface* browser_window_interface);
  ActionAppMenuManager(const ActionAppMenuManager&) = delete;
  ActionAppMenuManager& operator=(const ActionAppMenuManager&) = delete;
  ~ActionAppMenuManager();

  // Populates the menu action hierarchy under the app menu root.
  void CreateMenuHierarchy();

  actions::ActionItem* GetAppMenuRoot() const;

 private:
  void AddSearchBarAction(actions::ActionItem* root);
  void AddBlockHeaderActions(actions::ActionItem* root);
  void AddYourChromeActions(actions::ActionItem* root);
  void AddToolsAndActionsActions(actions::ActionItem* root);
  void AddFooterActions(actions::ActionItem* root);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<RecentTabsDynamicMenu> recent_tabs_menu_;
  std::unique_ptr<BookmarksDynamicMenu> bookmarks_menu_;
  std::unique_ptr<TabGroupDynamicMenu> tab_groups_menu_;
  std::unique_ptr<SendTabToSelfDynamicMenu> send_tab_to_self_menu_;
};

DECLARE_UI_CLASS_PROPERTY_TYPE(ActionAppMenuManager::DisplayType)
DECLARE_UI_CLASS_PROPERTY_TYPE(ui::ImageModel*)

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_MANAGER_H_
