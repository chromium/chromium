// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>

#include "base/ranges/algorithm.h"
#include "components/omnibox/browser/autocomplete_grouper_groups.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/omnibox_proto/groups.pb.h"

Section::Section(size_t limit) : limit_(limit) {}

Section::~Section() = default;

// static
ACMatches Section::GroupMatches(PSections sections, ACMatches matches) {
  for (auto& section : sections) {
    section->InitFromMatches(matches);
  }

  for (const auto& match : matches) {
    DCHECK(match.suggestion_group_id.has_value());
    for (const auto& section : sections) {
      if (section->Add(match))
        break;
    }
  }

  ACMatches grouped_matches = {};
  for (const auto& section : sections) {
    for (const auto& group : section->groups_) {
      grouped_matches.insert(grouped_matches.end(), group->matches().begin(),
                             group->matches().end());
    }
  }

  return grouped_matches;
}

Group* Section::FindGroup(const AutocompleteMatch& match) {
  auto iter = base::ranges::find_if(
      groups_, [&](const auto& group) { return group->CanAdd(match); });
  if (iter == groups_.end())
    return nullptr;
  return iter->get();
}

bool Section::Add(const AutocompleteMatch& match) {
  if (count_ >= limit_) {
    return false;
  }
  Group* group = FindGroup(match);
  if (group) {
    group->Add(match);
    count_++;
  }
  return group;
}

ZpsSection::ZpsSection(size_t limit) : Section(limit) {}

void ZpsSection::InitFromMatches(const ACMatches& matches) {
  // Iterate over the matches to see if they can be added to any `Group` in this
  // `Section`. If so, increment the total count for this `Section` and for the
  // respective group.
  for (const auto& match : matches) {
    Group* group = FindGroup(match);
    if (group) {
      count_++;
      group->Count(match);
    }
  }

  // Adjust the `Section`'s total limit based on the number of matches in the
  // `Section`. Ensure the limit is less than or equal to the original value.
  // Reset the count so that matches can actually be added to this `Section`.
  limit_ = std::min(limit_, count_);
  count_ = 0;

  size_t remaining = limit_;
  for (const auto& group : groups_) {
    group->AdjustLimitsAndResetCounts(remaining);
    remaining -= group->limit();
  }
  DCHECK_EQ(remaining, 0U);
}

AndroidZpsSection::AndroidZpsSection() : ZpsSection(15) {
  groups_.push_back(
      std::make_unique<Group>(1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX));
  groups_.push_back(
      std::make_unique<Group>(1, omnibox::GROUP_MOBILE_CLIPBOARD));
  groups_.push_back(
      std::make_unique<Group>(1, omnibox::GROUP_MOBILE_MOST_VISITED));
  groups_.push_back(
      std::make_unique<Group>(15, omnibox::GROUP_PREVIOUS_SEARCH_RELATED));
  groups_.push_back(
      std::make_unique<Group>(15, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST));
}

DesktopZpsSection::DesktopZpsSection() : ZpsSection(8) {
  groups_.push_back(
      std::make_unique<Group>(8, omnibox::GROUP_PREVIOUS_SEARCH_RELATED));
  groups_.push_back(
      std::make_unique<Group>(8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST));
  groups_.push_back(std::make_unique<Group>(8, omnibox::GROUP_TRENDS));
}

DesktopNonZpsSection::DesktopNonZpsSection() : Section(10) {
  groups_.push_back(std::make_unique<DefaultGroup>());
  groups_.push_back(std::make_unique<Group>(9, omnibox::GROUP_STARTER_PACK));
  groups_.push_back(std::make_unique<Group>(
      9, Group::GroupIdLimitsAndCounts{{omnibox::GROUP_SEARCH, {9}},
                                       {omnibox::GROUP_HISTORY_CLUSTER, {1}}}));
  groups_.push_back(std::make_unique<Group>(7, omnibox::GROUP_OTHER_NAVS));
}

void DesktopNonZpsSection::InitFromMatches(const ACMatches& matches) {
  auto* default_group = groups_[0].get();
  auto* search_group = groups_[2].get();
  auto* nav_group = groups_[3].get();

  // Determine if `matches` contains any searches.
  bool has_search = base::ranges::any_of(
      matches, [&](const auto& match) { return search_group->CanAdd(match); });

  // Determine if the default match will be a search.
  auto default_match = base::ranges::find_if(
      matches, [&](const auto& match) { return default_group->CanAdd(match); });
  bool default_is_search =
      default_match != matches.end() && search_group->CanAdd(*default_match);

  // Find the 1st nav's index.
  size_t first_nav_index = std::distance(
      matches.begin(), base::ranges::find_if(matches, [&](const auto& match) {
        return nav_group->CanAdd(match);
      }));

  // Show at most 8 suggestions if doing so includes navs; otherwise show 9 or
  // 10, if doing so doesn't include navs.
  limit_ = std::clamp<size_t>(first_nav_index, 8, 10);

  // Show at least 1 search, either in the default group or the search group.
  if (has_search && !default_is_search) {
    DCHECK_GE(limit_, 2U);
    nav_group->set_limit(limit_ - 2);
  }
}
