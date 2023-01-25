// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_GROUPS_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_GROUPS_H_

#include <map>
#include <memory>

#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/omnibox_proto/groups.pb.h"

// `Group` class and subclasses used to compose `Section`s.

// Group containing matches with the given `GroupId`s, limited per `GroupId` and
// the overall `limit`.
// E.g., this can describe a group that can have up to 3 search matches, 4
// document matches, and 5 matches total.
class Group {
 public:
  struct LimitAndCount {
    size_t limit;
    size_t count;
  };
  using GroupIdLimitsAndCounts = std::map<omnibox::GroupId, LimitAndCount>;

  Group(size_t limit, GroupIdLimitsAndCounts group_id_limits_and_counts);
  // Construct a `Group` with just 1 `GroupId`.
  Group(size_t limit, omnibox::GroupId group_id);
  virtual ~Group();

  // Returns if `match` can be added to this `Group`. Checks if the `GroupId` of
  // the match is permitted in this `Group`, this `Group`'s total limit, and the
  // limit for the `GroupId` of the match.
  virtual bool CanAdd(const AutocompleteMatch& match) const;
  // Adds `match` to this `Group` and increments this `Group`'s total count and
  // the count for the `GroupId` of the match.
  // `CanAdd()` should be verified by the caller.
  void Add(const AutocompleteMatch& match);
  // Increments this `Group`'s total count and the count for the `GroupId` of
  // the match but does not add `match` to this `Group`.
  void Count(const AutocompleteMatch& match);
  // Adjusts the `Group`'s total limit and the limits for the `GroupId`s in the
  // `Group` based on the number of matches counted and the given max limit.
  // Ensures that the limits are less than or equal to their original values.
  // Resets the `Group`'s total count and the counts for the `GroupId`s in the
  // `Group` so that matches can actually be added to the `Group`.
  void AdjustLimitsAndResetCounts(size_t max_limit);

  size_t limit() { return limit_; }
  void set_limit(size_t limit) { limit_ = limit; }
  const ACMatches& matches() { return matches_; }

 private:
  // Max number of matches this `Group` can contain.
  size_t limit_{0};
  // The number of matches this `Group` contains.
  size_t count_{0};
  // The limit and count per `GroupId`.
  GroupIdLimitsAndCounts group_id_limits_and_counts_;
  // The matches this `Group` contains.
  ACMatches matches_;
};

// Group containing up to 1 match that's `allowed_to_be_default` with the
// `GroupId`s `omnibox::GROUP_STARTER_PACK`, `omnibox::GROUP_SEARCH`, or
// `omnibox::GROUP_OTHER_NAVS`.
class DefaultGroup : public Group {
 public:
  DefaultGroup();
  bool CanAdd(const AutocompleteMatch& match) const override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_GROUPS_H_
