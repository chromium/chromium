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

// Group containing N matches without restrictions.
class GroupBase {
 public:
  explicit GroupBase(size_t limit);
  virtual ~GroupBase();
  // Returns if `match` can be added to this `Group`.
  virtual bool CanAdd(const AutocompleteMatch& match) const;
  // Adds `match` to this `Group`. `CanAdd()` should be verified by the caller.
  virtual void Add(const AutocompleteMatch& match);

  // Max number of matches this `Group` can contain.
  size_t limit_ = 0;
  // The matches this `Group` contains.
  ACMatches matches_;
};

// Group containing matches with specified `GroupId`s, limited per `GroupId`.
// E.g., this can describe a group that can have up to 3 search matches, 4
// document matches, and 5 matches total.
class MultiGroup : public GroupBase {
 public:
  struct LimitAndCount {
    size_t limit;
    size_t count;
  };
  using GroupLimitsAndCounts = std::map<omnibox::GroupId, LimitAndCount>;

  MultiGroup(size_t limit, GroupLimitsAndCounts group_id_limits_and_counts_);
  ~MultiGroup() override;
  bool CanAdd(const AutocompleteMatch& match) const override;
  void Add(const AutocompleteMatch& match) override;

  // The limit and count per `GroupId`.
  GroupLimitsAndCounts group_id_limits_and_counts_;
};

// Group containing 1 match that's `allowed_to_be_default`.
class DefaultGroup : public GroupBase {
 public:
  DefaultGroup();
  bool CanAdd(const AutocompleteMatch& match) const override;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_GROUPER_GROUPS_H_
