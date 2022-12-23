// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_

#include <memory>
#include <vector>

#include "components/omnibox/browser/autocomplete_match.h"

class Group;
class Section;
using PGroups = std::vector<std::unique_ptr<Group>>;
using PSections = std::vector<std::unique_ptr<Section>>;

// `Section` class and subclasses used to implement the various autocomplete
// grouping algorithms.

// Section containing no `Groups` and, therefore, no matches.
class Section {
 public:
  explicit Section(size_t limit);
  virtual ~Section();
  // Returns `matches` ranked and culled according to `sections`. All `matches`
  // should have `suggestion_group_id` set and be sorted by relevance.
  static ACMatches GroupMatches(PSections sections, ACMatches matches);

 protected:
  // Return the `Group` `match` can be added to, or `nullptr` if it can't be
  // added to any group in `groups_`.
  virtual Group* CanAdd(const AutocompleteMatch& match);
  // Tries to add `match` to the appropriate `groups_`. Returns if it was added
  // to any group in `groups_`.
  bool Add(const AutocompleteMatch& match);

  // Max number of matches this `Section` can contain across `groups_`.
  size_t limit_ = 0;
  // The number of matches this `Section` contains across `groups_`.
  size_t size_ = 0;
  // The `groups_` this `Section` contains.
  PGroups groups_ = {};
};

// Section containing up to 15 searches and 5 trending suggestions.
class MobileZeroInputSection : public Section {
 public:
  MobileZeroInputSection();
};

// Section expressing the desktop, non-zps limits and grouping. The rules are:
// - Contains up to 1 default, 10 starer packs, 10 search, 8 nav, and 1 history
//   cluster suggestions.
// - Allow up to 10 suggestions total.
// - Only allow more than 8 suggestions if the section does not contain navs.
// - Only allow more than 7 navs if there are no non-navs to show.
// - The history cluster suggestion should count against the search limit.
// - The default suggestion should count against either the search or nav limit.
// - Group defaults 1st, then searches and history clusters, then navs.
class DesktopNonZpsSection : public Section {
 public:
  explicit DesktopNonZpsSection(const ACMatches& matches);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_
