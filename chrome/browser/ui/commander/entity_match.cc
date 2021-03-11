// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/entity_match.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"

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

std::vector<WindowMatch> WindowsMatchingInput(const Browser* browser_to_exclude,
                                              const std::u16string& input,
                                              bool match_profile) {
  std::vector<WindowMatch> results;
  const BrowserList* browser_list = BrowserList::GetInstance();
  double mru_score = .95;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active(); ++it) {
    Browser* browser = *it;
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
    base::Optional<tab_groups::TabGroupId> group_to_exclude) {
  DCHECK(browser);
  std::vector<GroupMatch> results;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  TabGroupModel* model = browser->tab_strip_model()->group_model();
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
