// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_
#define CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/gfx/range/range.h"

class Browser;

namespace commander {

// Intermediate result type for browser windows that are eligible to be
// presented to the user as an option for a particular command.
struct WindowMatch {
  WindowMatch(Browser* browser, const base::string16& title, double score);
  ~WindowMatch();

  WindowMatch(WindowMatch&& other);
  WindowMatch& operator=(WindowMatch&& other);

  std::unique_ptr<CommandItem> ToCommandItem() const;

  Browser* browser;
  base::string16 title;
  std::vector<gfx::Range> matched_ranges;
  double score;
};
// Intermediate result type for tab groups that are eligible to be
// presented to the user as an option for a particular command.
struct GroupMatch {
  GroupMatch(tab_groups::TabGroupId group,
             const base::string16& title,
             double score);
  ~GroupMatch();

  GroupMatch(GroupMatch&& other);
  GroupMatch& operator=(GroupMatch&& other);

  std::unique_ptr<CommandItem> ToCommandItem() const;

  tab_groups::TabGroupId group;
  base::string16 title;
  std::vector<gfx::Range> matched_ranges;
  double score;
};

// Returns browser windows whose titles fuzzy match `input`. If input is empty,
// returns all eligible browser windows with score reflecting MRU order.
// `browser_to_exclude` is excluded from the list, as are all browser windows
// from a different profile unless `match_profile` is false.
std::vector<WindowMatch> WindowsMatchingInput(const Browser* browser_to_exclude,
                                              const base::string16& input,
                                              bool match_profile = false);

// Returns tab groups in `browser` whose titles fuzzy match `input`. If input is
// empty, returns all groups in an arbitrary order. If `group_to_exclude` is
// set, it is excluded from the list.
std::vector<GroupMatch> GroupsMatchingInput(
    const Browser* browser,
    const base::string16& input,
    base::Optional<tab_groups::TabGroupId> group_to_exclude = base::nullopt);
}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_
