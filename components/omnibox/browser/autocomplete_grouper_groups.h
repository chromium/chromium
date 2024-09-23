// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_GROUPS_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_GROUPS_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/omnibox_proto/groups.pb.h"

using PMatches = std::vector<raw_ptr<AutocompleteMatch, VectorExperimental>>;

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

  Group(size_t limit,
        GroupIdLimitsAndCounts group_id_limits_and_counts,
        bool is_default = false);
  // Construct a `Group` with just 1 `GroupId`.
  Group(size_t limit, omnibox::GroupId group_id);
  Group(const Group& group);
  Group& operator=(const Group& group);
  virtual ~Group();

  // Returns if `match` can be added to this `Group`. Checks if the `GroupId` of
  // the match is permitted in this `Group`, this `Group`'s total limit, and the
  // limit for the `GroupId` of the match.
  virtual bool CanAdd(const AutocompleteMatch& match) const;
  // Adds `match` to this `Group` and increments this `Group`'s total count and
  // the count for the `GroupId` of the match. `CanAdd()` should be verified by
  // the caller.
  void Add(const AutocompleteMatch& match);
  // Performs semantic grouping by Search vs URL.
  // TODO(ender): investigate whether we should split `Group` into ZPS and
  // non-ZPS specific subclasses. If this proves valuable, move the call below
  // to the non-ZPS subclass.
  void GroupMatchesBySearchVsUrl();

  size_t limit() const { return limit_; }
  void set_limit(size_t limit) { limit_ = limit; }
  const GroupIdLimitsAndCounts& group_id_limits_and_counts() const {
    return group_id_limits_and_counts_;
  }
  const PMatches& matches() const { return matches_; }

 private:
  // Max number of matches this `Group` can contain.
  size_t limit_{0};
  // The number of matches this `Group` contains.
  size_t count_{0};
  // The limit and count per `GroupId`.
  GroupIdLimitsAndCounts group_id_limits_and_counts_;
  // The matches this `Group` contains.
  PMatches matches_;
  // Whether is a default `Group`, i.e., allows only matches that are
  // `allowed_to_be_default_match`.
  bool is_default_{false};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_GROUPS_H_
