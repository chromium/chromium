// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_groups.h"

#include "base/containers/contains.h"
#include "components/omnibox/browser/autocomplete_match.h"

Group::Group(size_t limit,
             GroupIdLimitsAndCounts group_id_limits_and_counts,
             bool is_default)
    : limit_(limit),
      group_id_limits_and_counts_(group_id_limits_and_counts),
      is_default_(is_default) {}

Group::Group(size_t limit, omnibox::GroupId group_id)
    : Group(limit, GroupIdLimitsAndCounts{{group_id, {limit}}}) {}

Group::Group(const Group& group) = default;
Group& Group::operator=(const Group& group) = default;

Group::~Group() = default;

bool Group::CanAdd(const AutocompleteMatch& match) const {
  DCHECK(match.suggestion_group_id.has_value());
  const auto group_id = match.suggestion_group_id.value();
  // Check if `group_id` is permitted in this `Group`.
  if (!base::Contains(group_id_limits_and_counts_, group_id)) {
    return false;
  }
  const auto& limit_and_count = group_id_limits_and_counts_.at(group_id);
  // Check this `Group`'s total limit and the limit for the `group_id`. For a
  // default group, also check if the match is `allowed_to_be_default_match`.
  return count_ < limit_ && limit_and_count.count < limit_and_count.limit &&
         (!is_default_ || match.allowed_to_be_default_match);
}

void Group::Add(const AutocompleteMatch& match) {
  DCHECK(CanAdd(match));
  matches_.push_back(&const_cast<AutocompleteMatch&>(match));
  count_++;
  DCHECK_EQ(count_, matches_.size());
  group_id_limits_and_counts_[match.suggestion_group_id.value()].count++;
}

void Group::GroupMatchesBySearchVsUrl() {
  base::ranges::stable_sort(matches_.begin(), matches_.end(), {},
                            [](const auto& m) { return m->GetSortingOrder(); });
}
