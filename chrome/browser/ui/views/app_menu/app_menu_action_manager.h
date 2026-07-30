// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_MANAGER_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/color/color_id.h"

class AppMenuProxyActionItem;

// Represents a node in the block-style ChroMenu hierarchy.
struct MenuEntry {
  enum class DisplayType {
    kRow,
    kBlock,
    kFooter,
    kCustom,  // Used for specific items like zoom and profile.
  };

  std::optional<actions::ActionId> action_id;
  std::u16string text;
  std::vector<MenuEntry> children;
  DisplayType display_type = DisplayType::kRow;
  std::optional<ui::ColorId> container_color;
};

DECLARE_UI_CLASS_PROPERTY_TYPE(MenuEntry::DisplayType)
DECLARE_UI_CLASS_PROPERTY_TYPE(ui::ColorId)

// Manages the action item tree and its visual properties (display
// types) for the Block Style ChroMenu.
class AppMenuActionManager : public actions::ActionManager {
 public:
  static const ui::ClassProperty<MenuEntry::DisplayType>* const
      kAppMenuDisplayTypeKey;
  static const ui::ClassProperty<ui::ColorId>* const kAppMenuContainerColorKey;

  explicit AppMenuActionManager(actions::ActionItem* action_scope = nullptr);
  AppMenuActionManager(const AppMenuActionManager&) = delete;
  AppMenuActionManager& operator=(const AppMenuActionManager&) = delete;
  ~AppMenuActionManager() override;

  // Returns the canonical static hierarchy of entries for the app menu.
  static const MenuEntry& GetMenuHierarchy();

  // Initializes the root action item and inflates the menu hierarchy.
  void Initialize();

  actions::ActionItem* root_action_item() { return root_action_item_.get(); }

 private:
  void PopulateSubtree(
      actions::ActionItem* parent,
      const MenuEntry& entry,
      std::optional<ui::ColorId> inherited_container_color = std::nullopt);

  std::unique_ptr<AppMenuProxyActionItem> CreateAppMenuProxyActionItem(
      actions::ActionId action_id);

  std::unique_ptr<actions::IndirectActionItem> CreateAppMenuIndirectActionItem(
      actions::ActionId action_id);

  raw_ptr<actions::ActionItem> action_scope_ = nullptr;
  std::unique_ptr<actions::ActionItem> root_action_item_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_ACTION_MANAGER_H_
