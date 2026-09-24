// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_controller.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "ui/actions/actions.h"

namespace {

// Maps known dynamic submenus to their corresponding search category type.
AppMenuSearchItem::Type GetSubmenuType(
    std::optional<actions::ActionId> action_id,
    AppMenuSearchItem::Type current_type) {
  if (!action_id.has_value()) {
    return current_type;
  }
  switch (*action_id) {
    case kActionBookmarksSubmenu:
      return AppMenuSearchItem::Type::kBookmark;
    case kActionRecentTabsSubmenu:
      return AppMenuSearchItem::Type::kRecentTabs;
    case kActionSavedTabGroupsSubmenu:
      return AppMenuSearchItem::Type::kTabGroup;
    default:
      return current_type;
  }
}

// Returns the search category for a leaf menu item.
// A submenu like "Bookmarks" or "History" (`current_type` = `kBookmark` or
// `kRecentTabs`) contains both built-in browser commands (e.g. "Bookmark this
// tab" or "Open history page", which have a predefined `ActionId`) and
// dynamically generated user entries (e.g. a saved bookmark or a recently
// closed tab, which are created without an `ActionId`). Built-in commands
// should always be categorized as `Type::kAction`, whereas dynamic user entries
// without an `ActionId` inherit the enclosing submenu's `current_type`.
AppMenuSearchItem::Type GetSearchItemType(
    const actions::ActionItem* action_item,
    AppMenuSearchItem::Type current_type) {
  return (action_item && action_item->GetActionId().has_value())
             ? AppMenuSearchItem::Type::kAction
             : current_type;
}

// Determines if an action item can be processed for search indexing (must be
// visible, enabled, and have an actionable or traversable container display
// type).
bool CanProcessItem(const actions::ActionItem* action_item) {
  if (!action_item || !action_item->GetVisible() ||
      !action_item->GetEnabled()) {
    return false;
  }

  return action_item->GetProperty(AppMenuActionItem::kDisplayTypeKey) <
         AppMenuActionItem::DisplayType::kMaxSearchable;
}

// Determines if an action item is a submenu or container (e.g. has children or
// has a dynamic populate callback).
bool IsSubmenuContainer(const actions::BaseAction* action,
                        const actions::ActionItem* action_item) {
  return !action->GetChildren().children().empty() ||
         action->HasPopulateChildActionsCallback() ||
         (action_item && action_item->HasPopulateChildActionsCallback());
}

// Extracts the display title for an item with fallback priority:
// 1. Text override -> 2. ActionItem text -> 3. Tooltip text fallback.
std::u16string_view ExtractItemTitle(const actions::BaseAction* action,
                                     const actions::ActionItem* action_item) {
  const std::u16string* text_override =
      action->GetProperty(AppMenuActionItem::kTextOverrideKey);
  const std::u16string_view raw_text = text_override
                                           ? std::u16string_view(*text_override)
                                           : action_item->GetText();
  return raw_text.empty() ? action_item->GetTooltipText() : raw_text;
}

}  // namespace

AppMenuSearchController::AppMenuSearchController(actions::ActionItem* menu_root)
    : menu_root_(menu_root) {
  CHECK(menu_root_);
}

AppMenuSearchController::~AppMenuSearchController() = default;

void AppMenuSearchController::InitializeSearchIndex() {
  CHECK(!fuzzy_finder_);

  FlattenHierarchyRecursive(menu_root_, AppMenuSearchItem::Type::kAction,
                            /*context=*/u"");

  std::vector<FuzzySearchItem*> raw_items;
  raw_items.reserve(search_items_.size());
  for (const auto& item : search_items_) {
    raw_items.push_back(item.get());
  }

  fuzzy_finder_ = std::make_unique<FuzzyFinder>(std::move(raw_items));
}

void AppMenuSearchController::FlattenHierarchyRecursive(
    actions::BaseAction* action,
    AppMenuSearchItem::Type current_type,
    std::u16string_view context) {
  if (!action) {
    return;
  }

  // Root node traverses all children with empty context.
  if (action == menu_root_) {
    for (const auto& child : action->GetChildren().children()) {
      FlattenHierarchyRecursive(child.get(), current_type, /*context=*/u"");
    }
    return;
  }

  // Skip invisible, disabled, or non-actionable items (dividers, headers,
  // etc.).
  actions::ActionItem* const action_item = action->GetActionItem();
  if (!CanProcessItem(action_item)) {
    return;
  }

  const std::u16string_view title = ExtractItemTitle(action, action_item);
  const auto& children = action->GetChildren().children();

  // Recurse if the node is a container/submenu (even if currently empty).
  // Filter out all submenu headers/containers.
  if (IsSubmenuContainer(action, action_item)) {
    const AppMenuSearchItem::Type next_type =
        GetSubmenuType(action_item->GetActionId(), current_type);
    const std::u16string_view next_context = title.empty() ? context : title;

    for (const auto& child : children) {
      FlattenHierarchyRecursive(child.get(), next_type, next_context);
    }
    return;
  }

  if (!title.empty()) {
    AddSearchItem(action, GetSearchItemType(action_item, current_type), title,
                  context);
  }
}

void AppMenuSearchController::AddSearchItem(
    actions::BaseAction* action,
    AppMenuSearchItem::Type type,
    std::u16string_view title,
    std::u16string_view secondary_text) {
  AppMenuSearchItem::Builder builder;
  builder.SetType(type)
      .SetAction(action)
      .SetTitle(std::u16string(title))
      .SetSecondaryText(std::u16string(secondary_text));
  search_items_.push_back(builder.Build());
}
