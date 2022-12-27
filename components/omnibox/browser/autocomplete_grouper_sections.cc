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

Group* Section::CanAdd(const AutocompleteMatch& match) {
  if (size_ >= limit_)
    return nullptr;
  auto iter = base::ranges::find_if(
      groups_, [&](const auto& group) { return group->CanAdd(match); });
  if (iter == groups_.end())
    return nullptr;
  return iter->get();
}

bool Section::Add(const AutocompleteMatch& match) {
  Group* group = CanAdd(match);
  if (group) {
    group->Add(match);
    size_++;
  }
  return group;
}

MobileZeroInputSection::MobileZeroInputSection() : Section(20) {
  groups_.push_back(std::make_unique<Group>(
      10, Group::GroupIdLimitsAndCounts{
              {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, {1}},
              {omnibox::GROUP_MOBILE_CLIPBOARD, {1}},
              {omnibox::GROUP_MOBILE_MOST_VISITED, {8}},
              {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, {10}}}));
  groups_.push_back(std::make_unique<Group>(5, omnibox::GROUP_TRENDS));
}

DesktopNonZpsSection::DesktopNonZpsSection(const ACMatches& matches)
    : Section(10) {
  // Create the 4 groups with reasonable placeholder limits. Some of the limits
  // will be adjusted below.
  auto default_group = std::make_unique<DefaultGroup>();
  auto starter_pack_group =
      std::make_unique<Group>(9, omnibox::GROUP_STARTER_PACK);
  auto search_group = std::make_unique<Group>(
      9, Group::GroupIdLimitsAndCounts{{omnibox::GROUP_SEARCH, {9}},
                                       {omnibox::GROUP_HISTORY_CLUSTER, {1}}});
  auto nav_group = std::make_unique<Group>(7, omnibox::GROUP_OTHER_NAVS);

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
  if (has_search && !default_is_search)
    nav_group->set_limit(limit_ - 2);

  groups_.push_back(std::move(default_group));
  groups_.push_back(std::move(starter_pack_group));
  groups_.push_back(std::move(search_group));
  groups_.push_back(std::move(nav_group));
}
