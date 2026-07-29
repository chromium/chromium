// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_action_manager.h"

#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/app_menu/app_menu_proxy_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(MenuEntry::DisplayType)

DEFINE_UI_CLASS_PROPERTY_KEY(MenuEntry::DisplayType,
                             kAppMenuDisplayTypeInternal,
                             MenuEntry::DisplayType::kRow)
DEFINE_UI_CLASS_PROPERTY_KEY(ui::ColorId,
                             kAppMenuContainerColorInternal,
                             ui::kColorMenuBackground)

// static
const ui::ClassProperty<MenuEntry::DisplayType>* const
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

// static
const MenuEntry& AppMenuActionManager::GetMenuHierarchy() {
  // Defines the hierarchical tree of the Block ChroMenu
  static const base::NoDestructor<MenuEntry> hierarchy(
      {.text = u"Root",
       .children = {
           {.text = l10n_util::GetStringUTF16(IDS_APP_MENU_YOUR_CHROME_HEADER),
            .children =
                {
                    {.action_id = kActionShowPasswordManager},
                    {.action_id = kActionShowHistory},
                    {.action_id = kActionManageExtensions},
                },
            // Theme background color of Your Chrome section
            .container_color = kColorAppMenuYourChromeBackground},
           {.text = l10n_util::GetStringUTF16(
                IDS_APP_MENU_TOOLS_AND_ACTIONS_HEADER),
            .children =
                {
                    {.action_id = kActionPrint},
                    {.action_id = kActionFind},
                },
            // Theme background color of Tools and Actions section
            .container_color = kColorAppMenuToolsAndActionsBackground},
       }});
  return *hierarchy;
}

void AppMenuActionManager::Initialize() {
  root_action_item_ = actions::ActionItem::Builder().Build();
  PopulateSubtree(root_action_item_.get(), GetMenuHierarchy());
}

void AppMenuActionManager::PopulateSubtree(
    actions::ActionItem* parent,
    const MenuEntry& entry,
    std::optional<ui::ColorId> inherited_container_color) {
  // Every item created during this loop will be added as a child of `parent`.
  // - Entries without an action_id become section headings.
  // - Entries with an action_id become proxy action items.
  for (const auto& child : entry.children) {
    std::unique_ptr<actions::ActionItem> child_item;
    if (child.action_id.has_value()) {
      child_item = CreateAppMenuProxyActionItem(child.action_id.value());
      if (!child_item) {
        continue;
      }
    } else {
      child_item = std::make_unique<AppMenuSectionActionItem>(child.text);
    }

    std::optional<ui::ColorId> effective_container_color =
        child.container_color.has_value() ? child.container_color
                                          : inherited_container_color;

    actions::ActionItem* child_item_ptr = child_item.get();
    child_item_ptr->SetProperty(kAppMenuDisplayTypeKey, child.display_type);
    if (effective_container_color.has_value()) {
      child_item_ptr->SetProperty(kAppMenuContainerColorKey,
                                  effective_container_color.value());
    }

    parent->AddChild(std::move(child_item));
    // Recursively create child action items, propagating their background color
    // down.
    PopulateSubtree(child_item_ptr, child, effective_container_color);
  }
}

std::unique_ptr<AppMenuProxyActionItem>
AppMenuActionManager::CreateAppMenuProxyActionItem(
    actions::ActionId action_id) {
  actions::ActionItem* action =
      actions::ActionManager::Get().FindAction(action_id, action_scope_);
  if (!action && action_scope_) {
    action = actions::ActionManager::Get().FindAction(action_id);
  }
  if (!action) {
    return nullptr;
  }

  return std::make_unique<AppMenuProxyActionItem>(action);
}
