// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_
#define CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/range/range.h"

class Browser;

namespace commander {

// Intermediate result type for browser windows that are eligible to be
// presented to the user as an option for a particular command.
struct WindowMatch {
  WindowMatch(Browser* browser, const std::u16string& title, double score);
  ~WindowMatch();

  WindowMatch(WindowMatch&& other);
  WindowMatch& operator=(WindowMatch&& other);

  std::unique_ptr<CommandItem> ToCommandItem() const;

  raw_ptr<Browser> browser;
  std::u16string title;
  std::vector<gfx::Range> matched_ranges;
  double score;
};
// Intermediate result type for tab groups that are eligible to be
// presented to the user as an option for a particular command.
struct GroupMatch {
  GroupMatch(tab_groups::TabGroupId group,
             const std::u16string& title,
             double score);
  ~GroupMatch();

  GroupMatch(GroupMatch&& other);
  GroupMatch& operator=(GroupMatch&& other);

  std::unique_ptr<CommandItem> ToCommandItem() const;

  tab_groups::TabGroupId group;
  std::u16string title;
  std::vector<gfx::Range> matched_ranges;
  double score;
};

// Intermediate result type for tabs that are eligible to be
// presented to the user as an option for a particular command.
struct TabMatch {
  TabMatch(int index,
           int session_id,
           const std::u16string& title,
           double score);
  ~TabMatch();

  TabMatch(TabMatch&& other);
  TabMatch& operator=(TabMatch&& other);

  std::unique_ptr<CommandItem> ToCommandItem() const;

  // Index in tab strip.
  int index;
  // As obtained by sessions::SessionTabHelper::IdForTab. Used to ensure that
  // the tab at `index` is the one we expect for destructive actions.
  int session_id;
  std::u16string title;
  std::vector<gfx::Range> matched_ranges;
  double score;
};

// Returns browser windows whose titles fuzzy match `input`. If input is empty,
// returns all eligible browser windows with score reflecting MRU order.
// `browser_to_exclude` is excluded from the list, as are all browser windows
// from a different profile unless `match_profile` is false.
std::vector<WindowMatch> WindowsMatchingInput(const Browser* browser_to_exclude,
                                              const std::u16string& input,
                                              bool match_profile = false);

// Returns tab groups in `browser` whose titles fuzzy match `input`. If input is
// empty, returns all groups in an arbitrary order. If `group_to_exclude` is
// set, it is excluded from the list.
std::vector<GroupMatch> GroupsMatchingInput(
    const Browser* browser,
    const std::u16string& input,
    absl::optional<tab_groups::TabGroupId> group_to_exclude = absl::nullopt);

// Options for narrowing results from `TabsMatchingInput`.
struct TabSearchOptions {
  TabSearchOptions();
  ~TabSearchOptions();
  // Return only pinned tabs. Mutually exclusive with `only_unpinned`.
  bool only_pinned = false;
  // Return only unpinned tabs. Mutually exclusive with `only_pinned`.
  bool only_unpinned = false;
  // Return only audible tabs. Mutually exclusive with `only_muted`.
  bool only_audible = false;
  // Return only muted tabs. Mutually exclusive with `only_audible`.
  bool only_muted = false;
  // Exclude tabs that belong to this group. Explicitly setting this to the
  // same value as `only_tab_group` is invalid.
  absl::optional<tab_groups::TabGroupId> exclude_tab_group = absl::nullopt;
  // Exclude tabs that do not belong to this group. Explicitly setting this to
  // the same value as `exclude_tab_group` is invalid.
  absl::optional<tab_groups::TabGroupId> only_tab_group = absl::nullopt;
};

// Returns tabs in `browser` whose titles fuzzy match `input`. If input is
// empty, returns all groups in the order they appear in the tab strip.
std::vector<TabMatch> TabsMatchingInput(const Browser* browser,
                                        const std::u16string& input,
                                        const TabSearchOptions& options = {});

}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_
