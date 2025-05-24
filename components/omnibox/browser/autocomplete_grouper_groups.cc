// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_groups.h"

#include <algorithm>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace {
constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);
}  // namespace

Group::Group(size_t limit,
             std::map<omnibox::GroupId, size_t> group_id_limits,
             bool is_zps,
             bool is_default)
    : limit_(limit), is_zps_(is_zps), is_default_(is_default) {
  for (const auto& [group_id, group_id_limit] : group_id_limits) {
    group_id_limits_and_counts_[group_id] = {.limit = group_id_limit,
                                             .count = 0};
  }
}

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

void Group::GroupMatches() {
  if (is_zps_) {
    GroupMatchesByGroupId();
  } else if (is_android) {
    GroupMatchesBySearchVsUrl();
  }
}

void Group::GroupMatchesBySearchVsUrl() {
  std::ranges::stable_sort(matches_.begin(), matches_.end(), {},
                           [](const auto& m) { return m->GetSortingOrder(); });
}

void Group::GroupMatchesByGroupId() {
  // Assign a position to each GroupId based on its first occurrence in the list
  // of matches and perform a stable sort to group matches by their GroupId
  // while preserving the relative order of matches within the same GroupId.
  base::flat_map<omnibox::GroupId, size_t> group_id_position;
  size_t position = 0;
  for (const auto& match : matches_) {
    auto group_id = match->suggestion_group_id.value();
    if (!base::Contains(group_id_position, group_id)) {
      group_id_position[group_id] = position++;
    }
  }
  std::stable_sort(
      matches_.begin(), matches_.end(), [&](const auto& lhs, const auto& rhs) {
        return group_id_position[lhs->suggestion_group_id.value()] <
               group_id_position[rhs->suggestion_group_id.value()];
      });
}
