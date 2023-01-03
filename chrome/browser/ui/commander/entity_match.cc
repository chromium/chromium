// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/entity_match.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "components/sessions/content/session_tab_helper.h"

namespace commander {

namespace {
// TODO(lgrey): Just guessing for now! Not even sure if we need a max width,
// but right now, the code that does "<title> and x other tabs" wants a max.
double constexpr kMaxTitleWidth = 1000;

}  // namespace

WindowMatch::WindowMatch(Browser* browser,
                         const std::u16string& title,
                         double score)
    : browser(browser), title(title), score(score) {}
WindowMatch::~WindowMatch() = default;
WindowMatch::WindowMatch(WindowMatch&& other) = default;
WindowMatch& WindowMatch::operator=(WindowMatch&& other) = default;

std::unique_ptr<CommandItem> WindowMatch::ToCommandItem() const {
  auto item = std::make_unique<CommandItem>(title, score, matched_ranges);
  item->entity_type = CommandItem::Entity::kWindow;
  return item;
}

GroupMatch::GroupMatch(tab_groups::TabGroupId group,
                       const std::u16string& title,
                       double score)
    : group(group), title(title), score(score) {}
GroupMatch::~GroupMatch() = default;
GroupMatch::GroupMatch(GroupMatch&& other) = default;
GroupMatch& GroupMatch::operator=(GroupMatch&& other) = default;

std::unique_ptr<CommandItem> GroupMatch::ToCommandItem() const {
  auto item = std::make_unique<CommandItem>(title, score, matched_ranges);
  item->entity_type = CommandItem::Entity::kGroup;
  return item;
}

TabMatch::TabMatch(int index,
                   int session_id,
                   const std::u16string& title,
                   double score)
    : index(index), session_id(session_id), title(title), score(score) {}
TabMatch::~TabMatch() = default;
TabMatch::TabMatch(TabMatch&& other) = default;
TabMatch& TabMatch::operator=(TabMatch&& other) = default;

std::unique_ptr<CommandItem> TabMatch::ToCommandItem() const {
  auto item = std::make_unique<CommandItem>(title, score, matched_ranges);
  item->entity_type = CommandItem::Entity::kTab;
  return item;
}

TabSearchOptions::TabSearchOptions() = default;
TabSearchOptions::~TabSearchOptions() = default;

std::vector<TabMatch> TabsMatchingInput(const Browser* browser,
                                        const std::u16string& input,
                                        const TabSearchOptions& options) {
  DCHECK(browser);
  DCHECK(!(options.only_pinned && options.only_unpinned));
  DCHECK(!(options.only_audible && options.only_muted));
  DCHECK(!options.exclude_tab_group.has_value() ||
         options.exclude_tab_group != options.only_tab_group);
  double ordering_score = 1.0;
  std::vector<TabMatch> results;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    if (tab_strip_model->IsTabPinned(i) ? options.only_unpinned
                                        : options.only_pinned) {
      continue;
    }
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    if (options.only_audible && !contents->IsCurrentlyAudible())
      continue;
    if (options.only_muted && !contents->IsAudioMuted())
      continue;
    auto group = tab_strip_model->GetTabGroupForTab(i);
    if (options.only_tab_group.has_value() && options.only_tab_group != group)
      continue;
    if (options.exclude_tab_group.has_value() &&
        options.exclude_tab_group == group)
      continue;

    auto title = contents->GetTitle();
    if (input.empty()) {
      TabMatch match(i, sessions::SessionTabHelper::IdForTab(contents).id(),
                     title, ordering_score);
      results.push_back(std::move(match));
      ordering_score *= .95;
    } else {
      double score = finder.Find(title, &ranges);
      if (score > 0) {
        TabMatch match(i, sessions::SessionTabHelper::IdForTab(contents).id(),
                       title, score);
        match.matched_ranges = ranges;
        results.push_back(std::move(match));
      }
    }
  }
  return results;
}

std::vector<WindowMatch> WindowsMatchingInput(const Browser* browser_to_exclude,
                                              const std::u16string& input,
                                              bool match_profile) {
  std::vector<WindowMatch> results;
  double mru_score = .95;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  for (Browser* browser : BrowserList::GetInstance()->OrderedByActivation()) {
    if (browser == browser_to_exclude || !browser->is_type_normal())
      continue;
    if (match_profile && browser->profile() != browser_to_exclude->profile())
      continue;
    std::u16string title = browser->GetWindowTitleForMaxWidth(kMaxTitleWidth);
    if (input.empty()) {
      WindowMatch match(browser, title, mru_score);
      results.push_back(std::move(match));
      mru_score *= .95;
    } else {
      double score = finder.Find(title, &ranges);
      if (score > 0) {
        WindowMatch match(browser, std::move(title), score);
        match.matched_ranges = ranges;
        results.push_back(std::move(match));
      }
    }
  }
  return results;
}

std::vector<GroupMatch> GroupsMatchingInput(
    const Browser* browser,
    const std::u16string& input,
    absl::optional<tab_groups::TabGroupId> group_to_exclude) {
  DCHECK(browser);
  std::vector<GroupMatch> results;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  TabGroupModel* model = browser->tab_strip_model()->group_model();
  if (!model)
    return results;
  // For empty input, use this to preserve TabGroupModel's ordering, which is
  // arbitrary but still helpful to keep consistent across calls and surfaces.
  double ordering_score = .95;
  for (const tab_groups::TabGroupId& group_id : model->ListTabGroups()) {
    if (group_to_exclude == group_id)
      continue;
    TabGroup* group = model->GetTabGroup(group_id);
    const std::u16string& group_title = group->visual_data()->title();
    const std::u16string& title =
        group_title.empty() ? group->GetContentString() : group_title;
    if (input.empty()) {
      GroupMatch match(group_id, title, ordering_score);
      results.push_back(std::move(match));
      ordering_score *= .95;
    } else {
      double score = finder.Find(title, &ranges);
      if (score > 0) {
        GroupMatch match(group_id, title, score);
        match.matched_ranges = ranges;
        results.push_back(std::move(match));
      }
    }
  }
  return results;
}

}  // namespace commander
