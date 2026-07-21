// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_action_manager.h"

#include <utility>

#include "base/check.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/app_menu/app_menu_proxy_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_section_action_item.h"
#include "ui/actions/actions.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(MenuEntry::DisplayType)

DEFINE_UI_CLASS_PROPERTY_KEY(MenuEntry::DisplayType,
                             kAppMenuDisplayTypeInternal,
                             MenuEntry::DisplayType::kRow)

// static
const ui::ClassProperty<MenuEntry::DisplayType>* const
    AppMenuActionManager::kAppMenuDisplayTypeKey = kAppMenuDisplayTypeInternal;

// Initializes the AppMenuActionManager with an optional search scope.
// When creating proxy items, CreateAppMenuProxyActionItem() will search inside
// this scope before checking the global ActionManager.
AppMenuActionManager::AppMenuActionManager(actions::ActionItem* action_scope)
    : action_scope_(action_scope) {}

AppMenuActionManager::~AppMenuActionManager() = default;

// static
const MenuEntry& AppMenuActionManager::GetMenuHierarchy() {
  // Defines the hierarchical tree of the Block ChroMenu:
  // - Entries without an action_id become section headings.
  // - Entries with an action_id become proxy action items.
  static const base::NoDestructor<MenuEntry> hierarchy(
      {.text = u"Root",
       // TODO(crbug.com/535705944): Localize ChroMenu headers.
       .children = {{.text = u"Your Chrome",
                     .children = {
                         {.action_id = kActionShowDownloads},
                         {.action_id = kActionClearBrowsingData},
                     }}}});
  return *hierarchy;
}

void AppMenuActionManager::Initialize() {
  root_action_item_ = actions::ActionItem::Builder().Build();
  PopulateSubtree(root_action_item_.get(), GetMenuHierarchy());
}

void AppMenuActionManager::PopulateSubtree(actions::ActionItem* parent,
                                           const MenuEntry& entry) {
  for (const auto& child : entry.children) {
    std::unique_ptr<actions::ActionItem> child_item;
    if (child.action_id.has_value()) {
      child_item = CreateAppMenuProxyActionItem(child.action_id.value());
    } else {
      child_item = std::make_unique<AppMenuSectionActionItem>(child.text);
    }

    actions::ActionItem* child_item_ptr = child_item.get();
    child_item_ptr->SetProperty(kAppMenuDisplayTypeKey, child.display_type);
    parent->AddChild(std::move(child_item));
    PopulateSubtree(child_item_ptr, child);
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
  CHECK(action) << "Action ID " << action_id << " not found in ActionManager.";

  return std::make_unique<AppMenuProxyActionItem>(action);
}
