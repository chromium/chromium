// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_groups.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "components/omnibox/browser/autocomplete_match.h"

GroupBase::GroupBase(size_t limit) : limit_(limit) {}

GroupBase::~GroupBase() = default;

bool GroupBase::CanAdd(const AutocompleteMatch& match) const {
  return matches_.size() < limit_;
}

void GroupBase::Add(const AutocompleteMatch& match) {
  DCHECK(CanAdd(match));
  matches_.push_back(match);
}

MultiGroup::MultiGroup(size_t limit,
                       GroupLimitsAndCounts group_id_limits_and_counts)
    : GroupBase(limit),
      group_id_limits_and_counts_(group_id_limits_and_counts) {}

MultiGroup::MultiGroup(size_t limit, omnibox::GroupId group_id)
    : MultiGroup(limit, GroupLimitsAndCounts{{group_id, {limit}}}) {}

MultiGroup::~MultiGroup() = default;

bool MultiGroup::CanAdd(const AutocompleteMatch& match) const {
  DCHECK(match.suggestion_group_id.has_value());
  const auto group_id = match.suggestion_group_id.value();
  // Check the `group_id` is included in this `Group`.
  if (!group_id_limits_and_counts_.count(group_id))
    return false;
  const LimitAndCount& limit_and_count =
      group_id_limits_and_counts_.at(group_id);
  // Check this `Group`s total limit and the limit the particular `group_id`.
  return GroupBase::CanAdd(match) &&
         limit_and_count.count < limit_and_count.limit;
}

void MultiGroup::Add(const AutocompleteMatch& match) {
  GroupBase::Add(match);
  group_id_limits_and_counts_[match.suggestion_group_id.value()].count++;
}

DefaultGroup::DefaultGroup() : GroupBase(1) {}

bool DefaultGroup::CanAdd(const AutocompleteMatch& match) const {
  return GroupBase::CanAdd(match) && match.allowed_to_be_default_match;
}
