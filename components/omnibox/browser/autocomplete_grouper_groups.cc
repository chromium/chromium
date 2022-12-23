// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_groups.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/omnibox/browser/autocomplete_match.h"

Group::Group(size_t limit, GroupIdLimitsAndCounts group_id_limits_and_counts)
    : limit_(limit), group_id_limits_and_counts_(group_id_limits_and_counts) {}

Group::Group(size_t limit, omnibox::GroupId group_id)
    : Group(limit, GroupIdLimitsAndCounts{{group_id, {limit}}}) {}

Group::~Group() = default;

bool Group::CanAdd(const AutocompleteMatch& match) const {
  DCHECK(match.suggestion_group_id.has_value());
  const auto group_id = match.suggestion_group_id.value();
  // Check if `group_id` is permitted in this `Group`.
  if (!base::Contains(group_id_limits_and_counts_, group_id)) {
    return false;
  }
  const auto& limit_and_count = group_id_limits_and_counts_.at(group_id);
  // Check this `Group`s total limit and the limit the particular `group_id`.
  return matches_.size() < limit_ &&
         limit_and_count.count < limit_and_count.limit;
}

void Group::Add(const AutocompleteMatch& match) {
  DCHECK(CanAdd(match));
  matches_.push_back(match);
  group_id_limits_and_counts_[match.suggestion_group_id.value()].count++;
}

DefaultGroup::DefaultGroup()
    : Group(1,
            GroupIdLimitsAndCounts{{omnibox::GROUP_STARTER_PACK, {1}},
                                   {omnibox::GROUP_SEARCH, {1}},
                                   {omnibox::GROUP_OTHER_NAVS, {1}}}) {}

bool DefaultGroup::CanAdd(const AutocompleteMatch& match) const {
  return Group::CanAdd(match) && match.allowed_to_be_default_match;
}
