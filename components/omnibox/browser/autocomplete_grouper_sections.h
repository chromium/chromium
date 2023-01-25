// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_

#include <memory>
#include <vector>

#include "components/omnibox/browser/autocomplete_grouper_groups.h"
#include "components/omnibox/browser/autocomplete_match.h"

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
  // Used to adjust this `Section`'s total limit and the total limits for the
  // `Group`s in this `Section` based on the given matches.
  virtual void InitFromMatches(const ACMatches& matches) {}

 protected:
  // Returns the first `Group` in this `Section` `match` can be added to or
  // `nullptr` if none can be found. Does not take the total limit into account.
  Group* FindGroup(const AutocompleteMatch& match);
  // Returns whether `match` was added to a `Group` in this `Section`. Does not
  // add a match beyond the total limit.
  bool Add(const AutocompleteMatch& match);

  // Max number of matches this `Section` can contain across `groups_`.
  size_t limit_{0};
  // The number of matches this `Section` contains across `groups_`.
  size_t count_{0};
  // The `Group`s this `Section` contains.
  PGroups groups_{};
};

// Base section for zps limits and grouping.
// Since zero-prefix matches are seen in descending order of relevance, the
// default implementation of `InitFromMatches()` ensures that matches with
// higher relevance scores do not fill up the section if others with lower
// scores are expected to be placed earlier based on their `Group`s position.
class ZpsSection : public Section {
 public:
  explicit ZpsSection(size_t limit);
  // Section:
  void InitFromMatches(const ACMatches& matches) override;
};

// Section expressing the Android zps limits and grouping. The rules are:
// - Contains up to 1 verbatim, 1 clipboard, 1 most visited, 8 related search
//   suggestions, and 15 personalized suggestions.
// - Allow up to 15 suggestions total.
class AndroidZpsSection : public ZpsSection {
 public:
  AndroidZpsSection();
};

// Section expressing the Desktop zps limits and grouping. The rules are:
// - Containing up to 8 related search suggestions, 8 personalized suggestions,
//   and 8 trending search suggestions.
// - Allow up to 8 suggestions total.
class DesktopZpsSection : public ZpsSection {
 public:
  DesktopZpsSection();
};

// Section expressing the Desktop, non-zps limits and grouping. The rules are:
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
  DesktopNonZpsSection();
  // Section:
  void InitFromMatches(const ACMatches& matches) override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_SECTIONS_H_
