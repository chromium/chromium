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
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Maximum number of candidates returned by fuzzy finder across all categories.
constexpr size_t kMaxGlobalCandidates = 50;

// Maximum number of search results displayed per section.
constexpr size_t kMaxActions = 6;
constexpr size_t kMaxBookmarks = 3;
constexpr size_t kMaxRecentTabs = 3;
constexpr size_t kMaxTabGroups = 3;

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
    case kActionOtherDeviceTabsSubmenu:
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
bool CanProcessItem(actions::BaseAction* action) {
  if (!action || !action->GetActionItem() ||
      !action->GetActionItem()->GetVisible() ||
      !action->GetActionItem()->GetEnabled()) {
    return false;
  }

  return action->GetProperty(AppMenuActionItem::kDisplayTypeKey) <
         AppMenuActionItem::DisplayType::kMaxSearchable;
}

// Determines if an action item is a submenu or container (e.g. has children or
// has a dynamic populate callback).
bool IsSubmenuContainer(const actions::BaseAction* action) {
  return !action->GetChildren().children().empty() ||
         action->HasPopulateChildActionsCallback();
}

// Extracts the display title for an item with fallback priority:
// 1. Text override -> 2. ActionItem text -> 3. Tooltip text fallback.
std::u16string_view ExtractItemTitle(actions::BaseAction* action) {
  const std::u16string* text_override =
      action->GetProperty(AppMenuActionItem::kTextOverrideKey);
  const std::u16string_view raw_text = text_override
                                           ? std::u16string_view(*text_override)
                                           : action->GetActionItem()->GetText();
  return raw_text.empty() ? action->GetActionItem()->GetTooltipText()
                          : raw_text;
}

}  // namespace

AppMenuSearchController::SearchResults::SearchResults() = default;
AppMenuSearchController::SearchResults::SearchResults(SearchResults&&) =
    default;
AppMenuSearchController::SearchResults&
AppMenuSearchController::SearchResults::operator=(SearchResults&&) = default;
AppMenuSearchController::SearchResults::~SearchResults() = default;

AppMenuSearchController::AppMenuSearchController(
    actions::ActionItem* menu_root,
    AppMenuSearchBarView* search_bar,
    Delegate* delegate)
    : menu_root_(menu_root), search_bar_(search_bar), delegate_(delegate) {
  CHECK(menu_root_);
  CHECK(search_bar_);
  CHECK(delegate_);
  query_changed_subscription_ =
      search_bar_->AddTextChangedCallback(base::BindRepeating(
          &AppMenuSearchController::OnQueryChanged, base::Unretained(this)));
  back_pressed_subscription_ =
      search_bar_->AddBackPressedCallback(base::BindRepeating(
          &AppMenuSearchController::OnBackPressed, base::Unretained(this)));
}

AppMenuSearchController::~AppMenuSearchController() = default;

void AppMenuSearchController::InitializeSearchIndex() {
  CHECK(!fuzzy_finder_);

  FlattenHierarchyRecursive(menu_root_, AppMenuSearchItem::Type::kAction,
                            /*context=*/u"");

  std::vector<raw_ptr<FuzzySearchItem>> raw_items;
  raw_items.reserve(search_items_.size());
  for (const auto& item : search_items_) {
    raw_items.push_back(item.get());
  }

  fuzzy_finder_ = std::make_unique<FuzzyFinder>(std::move(raw_items));
}

std::unique_ptr<actions::ActionItem> AppMenuSearchController::Search(
    std::u16string_view query) {
  CHECK(fuzzy_finder_);

  const std::u16string_view trimmed_query =
      base::TrimWhitespace(query, base::TRIM_ALL);
  if (!FuzzyFinder::HasMinQueryLength(trimmed_query)) {
    return nullptr;
  }

  const std::vector<FuzzySearchResult> matches =
      fuzzy_finder_->FuzzyFind(trimmed_query, kMaxGlobalCandidates);

  return BuildSearchResultsTree(ProcessSearchResults(matches));
}

void AppMenuSearchController::OnQueryChanged() {
  std::unique_ptr<actions::ActionItem> results = Search(search_bar_->GetText());
  if (!results) {
    // The query is below the minimum query length threshold. Keep the current
    // rows, so only the back button leaves the results.
    return;
  }

  delegate_->ShowSearchResults(results.get());
  search_results_ = std::move(results);
  search_bar_->SetLeadingIcon(AppMenuSearchBarView::LeadingIcon::kBack);
}

void AppMenuSearchController::OnBackPressed() {
  if (!search_results_) {
    return;
  }

  delegate_->RestoreMenu();
  search_results_.reset();
  search_bar_->SetLeadingIcon(AppMenuSearchBarView::LeadingIcon::kSearch);
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
  if (!CanProcessItem(action)) {
    return;
  }

  const std::u16string_view title = ExtractItemTitle(action);

  // Tab groups: only the groups are searchable, one result each.
  if (current_type == AppMenuSearchItem::Type::kTabGroup) {
    if (!action->GetActionItem()->GetActionId().has_value() && !title.empty()) {
      AddSearchItem(action, current_type, title, /*secondary_text=*/u"");
    }
    return;
  }

  const auto& children = action->GetChildren().children();

  // Recurse if the node is a container/submenu (even if currently empty).
  // Filter out all submenu headers/containers.
  if (IsSubmenuContainer(action)) {
    const AppMenuSearchItem::Type next_type =
        GetSubmenuType(action->GetActionItem()->GetActionId(), current_type);
    const std::u16string_view next_context = title.empty() ? context : title;

    for (const auto& child : children) {
      FlattenHierarchyRecursive(child.get(), next_type, next_context);
    }
    return;
  }

  if (!title.empty()) {
    AddSearchItem(action,
                  GetSearchItemType(action->GetActionItem(), current_type),
                  title, context);
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
      .SetSecondaryText(std::u16string(secondary_text))
      .SetIsCheckable(action->GetProperty(AppMenuActionItem::kIsCheckableKey));
  if (const ui::ImageModel* icon_override =
          action->GetProperty(AppMenuActionItem::kIconOverrideKey)) {
    builder.SetIconOverride(*icon_override);
  }
  search_items_.push_back(builder.Build());
}

AppMenuSearchController::SearchResults
AppMenuSearchController::ProcessSearchResults(
    const std::vector<FuzzySearchResult>& matches) const {
  SearchResults results;
  results.actions.reserve(kMaxActions);
  results.bookmarks.reserve(kMaxBookmarks);
  results.recent_tabs.reserve(kMaxRecentTabs);
  results.tab_groups.reserve(kMaxTabGroups);

  size_t remaining_capacity =
      kMaxActions + kMaxBookmarks + kMaxRecentTabs + kMaxTabGroups;

  auto try_add_item = [&remaining_capacity](
                          std::vector<SearchResults::Item>& section,
                          size_t max_cap,
                          const AppMenuSearchItem* search_item) {
    if (section.size() < max_cap) {
      section.push_back({
          .action_item = search_item->GetActionItem(),
          .title = search_item->GetTitle(),
          .breadcrumb = search_item->GetSecondaryText(),
          .icon_override = search_item->GetIconOverride(),
          .is_checkable = search_item->IsCheckable(),
      });
      --remaining_capacity;
    }
  };

  for (const auto& match : matches) {
    // Early exit if all category sections have reached their maximum caps.
    if (remaining_capacity <= 0) {
      break;
    }

    auto* search_item = static_cast<AppMenuSearchItem*>(match.item.get());
    if (!search_item || !search_item->GetActionItem()) {
      continue;
    }

    // Partition items into their respective category sections and enforce
    // maximum per-section result caps to keep search results concise.
    switch (search_item->GetType()) {
      case AppMenuSearchItem::Type::kAction:
        try_add_item(results.actions, kMaxActions, search_item);
        break;
      case AppMenuSearchItem::Type::kBookmark:
        try_add_item(results.bookmarks, kMaxBookmarks, search_item);
        break;
      case AppMenuSearchItem::Type::kRecentTabs:
        try_add_item(results.recent_tabs, kMaxRecentTabs, search_item);
        break;
      case AppMenuSearchItem::Type::kTabGroup:
        try_add_item(results.tab_groups, kMaxTabGroups, search_item);
        break;
    }
  }

  return results;
}

std::unique_ptr<actions::ActionItem>
AppMenuSearchController::BuildSearchResultsTree(SearchResults results) const {
  std::unique_ptr<actions::ActionItem> root =
      actions::ActionItem::Builder().Build();

  if (results.empty()) {
    root->AddChild(
        actions::ActionItem::Builder()
            .SetText(l10n_util::GetStringUTF16(IDS_SEARCH_NO_RESULTS))
            .SetEnabled(false)
            .Build());
    return root;
  }

  auto append_section = [&root](std::optional<std::u16string_view> title,
                                const std::vector<SearchResults::Item>& items) {
    if (items.empty()) {
      return;
    }
    actions::ActionItem* container = root.get();
    if (title.has_value()) {
      auto section = actions::ActionItem::Builder()
                         .SetText(std::u16string(*title))
                         .Build();
      section->SetProperty(AppMenuActionItem::kDisplayTypeKey,
                           AppMenuActionItem::DisplayType::kSection);
      section->AddChild(
          AppMenuActionItem::CreateHeader(std::u16string(*title)));
      container = root->AddChild(std::move(section));
    }
    for (const auto& item : items) {
      auto indirect_item =
          std::make_unique<actions::IndirectActionItem>(item.action_item);
      // Always show the title that matched, even when it's the action's own
      // text. Otherwise the row shows the action's text, which can differ
      // (e.g. a text override on the menu row) or be empty (icon-only
      // buttons, which are titled by their tooltip).
      indirect_item->SetProperty(AppMenuActionItem::kTextOverrideKey,
                                 item.title);
      if (!item.breadcrumb.empty()) {
        indirect_item->SetProperty(AppMenuActionItem::kSecondaryTextKey,
                                   item.breadcrumb);
      }
      if (item.icon_override) {
        indirect_item->SetProperty(AppMenuActionItem::kIconOverrideKey,
                                   *item.icon_override);
      }
      if (item.is_checkable) {
        indirect_item->SetProperty(AppMenuActionItem::kIsCheckableKey, true);
      }
      container->AddChild(std::move(indirect_item));
    }
  };

  append_section(std::nullopt, results.actions);
  append_section(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_BOOKMARKS_HEADER),
      results.bookmarks);
  append_section(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_TAB_GROUPS_HEADER),
      results.tab_groups);
  append_section(
      l10n_util::GetStringUTF16(IDS_APP_MENU_SEARCH_RECENT_TABS_HEADER),
      results.recent_tabs);

  return root;
}
