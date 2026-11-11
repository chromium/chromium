// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/app_menu_search_controller.h"

#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_finder.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/app_menu/app_menu_action_item.h"
#include "chrome/browser/ui/views/app_menu/app_menu_search_bar_view.h"
#include "chrome/browser/ui/views/app_menu/bookmarks_dynamic_menu.h"
#include "chrome/browser/ui/views/app_menu/recent_tabs_dynamic_menu.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "ui/actions/actions.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/text_utils.h"
#include "url/gurl.h"

namespace {

// Returns the fuzzy search scoring params, starting from the kChroMenuSearch
// feature params. If the --chro-menu-search-scoring switch (settable from
// chrome://flags) is present, each valid "title,secondary,synonym,min_score"
// entry in it overrides the corresponding feature param value. Empty or invalid
// entries are ignored.
FuzzyFinder::ScoringParams GetScoringParams() {
  FuzzyFinder::ScoringParams params{
      .title = features::kChroMenuSearchTitleWeight.Get(),
      .secondary_text = features::kChroMenuSearchSecondaryWeight.Get(),
      .synonym = features::kChroMenuSearchSynonymWeight.Get(),
      .min_score = features::kChroMenuSearchMinScore.Get(),
  };

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kChroMenuSearchScoring);
  if (switch_value.empty()) {
    return params;
  }

  const std::array<double*, 4> fields = {&params.title, &params.secondary_text,
                                         &params.synonym, &params.min_score};
  const std::vector<std::string_view> parts = base::SplitStringPiece(
      switch_value, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t i = 0; i < fields.size() && i < parts.size(); ++i) {
    double value;
    if (base::StringToDouble(parts[i], &value) && std::isfinite(value)) {
      *fields[i] = value;
    }
  }
  return params;
}

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

// Determines if an action item is a submenu or container (e.g. has children,
// has a dynamic populate callback, or is explicitly marked as a submenu).
bool IsSubmenuContainer(const actions::BaseAction* action) {
  return !action->GetChildren().children().empty() ||
         action->HasPopulateChildActionsCallback() ||
         action->GetProperty(AppMenuActionItem::kIsSubmenuKey);
}

// Extracts the display title for an item with fallback priority:
// 1. Text override -> 2. ActionItem text -> 3. Tooltip text fallback.
// The returned title is plain display text: menu label mnemonic markers are
// stripped and escaped ampersands ("&&") are collapsed to a literal "&".
std::u16string ExtractItemTitle(actions::BaseAction* action) {
  const std::u16string* text_override =
      action->GetProperty(AppMenuActionItem::kTextOverrideKey);
  // Text overrides are menu labels and may contain mnemonics, whereas
  // ActionItem::GetText() already has them removed.
  const std::u16string text =
      text_override ? gfx::RemoveAccelerator(*text_override)
                    : std::u16string(action->GetActionItem()->GetText());
  if (!text.empty()) {
    return text;
  }
  return std::u16string(action->GetActionItem()->GetTooltipText());
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

  fuzzy_finder_ =
      std::make_unique<FuzzyFinder>(std::move(raw_items), GetScoringParams());
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
  const std::u16string_view query = search_bar_->GetText();
  if (base::TrimWhitespace(query, base::TRIM_ALL).empty()) {
    // The query was cleared. Show the main menu
    // instead of previous search results.
    RestoreMenu();
    return;
  }

  std::unique_ptr<actions::ActionItem> results = Search(query);
  if (!results) {
    // The query is below the minimum query length threshold. Keep the current
    // rows until the query is long enough or cleared.
    return;
  }

  actions::ActionItem* results_ptr = results.get();
  std::unique_ptr<actions::ActionItem> old_results = std::move(search_results_);
  search_results_ = std::move(results);
  delegate_->ShowSearchResults(results_ptr, std::move(old_results));
}

void AppMenuSearchController::RestoreMenu() {
  if (!search_results_) {
    return;
  }

  delegate_->RestoreMenu(std::move(search_results_));
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

  const std::u16string title = ExtractItemTitle(action);

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

  if (title.empty()) {
    return;
  }

  const AppMenuSearchItem::Type item_type =
      GetSearchItemType(action->GetActionItem(), current_type);

  // Bookmarks and recent tabs show the domain of the page they open instead of
  // where they live in the menu.
  const GURL* url = nullptr;
  if (item_type == AppMenuSearchItem::Type::kBookmark) {
    url = BookmarksDynamicMenu::GetBookmarkUrl(action);
  } else if (item_type == AppMenuSearchItem::Type::kRecentTabs) {
    url = RecentTabsDynamicMenu::GetTabUrl(action);
    // Only the tabs are searchable, not the commands that restore a whole
    // window, group or split.
    if (!url) {
      return;
    }
  }

  AddSearchItem(
      action, item_type, title,
      url ? url_formatter::
                FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(*url)
          : std::u16string(context));
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
                          size_t max_cap, const AppMenuSearchItem* search_item,
                          const gfx::Range& highlight_range) {
    if (section.size() < max_cap) {
      section.push_back({
          .action_item = search_item->GetActionItem(),
          .title = search_item->GetTitle(),
          .breadcrumb = search_item->GetSecondaryText(),
          .icon_override = search_item->GetIconOverride(),
          .is_checkable = search_item->IsCheckable(),
          .highlight_range = highlight_range,
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
        try_add_item(results.actions, kMaxActions, search_item,
                     match.exact_match_range);
        break;
      case AppMenuSearchItem::Type::kBookmark:
        try_add_item(results.bookmarks, kMaxBookmarks, search_item,
                     match.exact_match_range);
        break;
      case AppMenuSearchItem::Type::kRecentTabs:
        try_add_item(results.recent_tabs, kMaxRecentTabs, search_item,
                     match.exact_match_range);
        break;
      case AppMenuSearchItem::Type::kTabGroup:
        try_add_item(results.tab_groups, kMaxTabGroups, search_item,
                     match.exact_match_range);
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
      // buttons, which are titled by their tooltip). Highlighted rows disable
      // mnemonics on the MenuItemView so UTF-16 offsets match `item.title`;
      // unhighlighted rows keep mnemonics enabled, so both title and
      // secondary title need ampersands escaped.
      const bool has_highlight = item.highlight_range.IsValid();
      if (has_highlight) {
        indirect_item->SetProperty(AppMenuActionItem::kHighlightRangeKey,
                                   item.highlight_range);
      }
      indirect_item->SetProperty(
          AppMenuActionItem::kTextOverrideKey,
          has_highlight
              ? std::u16string(item.title)
              : ui::EscapeMenuLabelAmpersands(std::u16string(item.title)));
      if (!item.breadcrumb.empty()) {
        indirect_item->SetProperty(AppMenuActionItem::kSecondaryTextKey,
                                   has_highlight
                                       ? std::u16string(item.breadcrumb)
                                       : ui::EscapeMenuLabelAmpersands(
                                             std::u16string(item.breadcrumb)));
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
