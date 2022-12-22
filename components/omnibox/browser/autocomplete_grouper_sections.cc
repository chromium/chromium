// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "components/omnibox/browser/autocomplete_grouper_groups.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "third_party/omnibox_proto/groups.pb.h"

Section::Section(size_t limit) : limit_(limit) {}

Section::~Section() = default;

// static
ACMatches Section::GroupMatches(PSections sections, ACMatches matches) {
  int last_relevance = -1;
  for (const auto& match : matches) {
    DCHECK(match.suggestion_group_id.has_value());
    DCHECK(last_relevance == -1 || match.relevance <= last_relevance);
    last_relevance = match.relevance;
    for (const auto& section : sections) {
      if (section->Add(match))
        break;
    }
  }

  ACMatches grouped_matches = {};
  for (const auto& section : sections) {
    for (const auto& group : section->groups_) {
      grouped_matches.insert(grouped_matches.end(), group->matches_.begin(),
                             group->matches_.end());
    }
  }

  return grouped_matches;
}

GroupBase* Section::CanAdd(const AutocompleteMatch& match) {
  if (size_ >= limit_)
    return nullptr;
  auto iter = base::ranges::find_if(
      groups_, [&](const auto& group) { return group->CanAdd(match); });
  if (iter == groups_.end())
    return nullptr;
  return iter->get();
}

bool Section::Add(const AutocompleteMatch& match) {
  GroupBase* group = CanAdd(match);
  if (group) {
    group->Add(match);
    size_++;
  }
  return group;
}

MobileZeroInputSection::MobileZeroInputSection() : Section(20) {
  groups_.push_back(std::make_unique<MultiGroup>(
      10, MultiGroup::GroupLimitsAndCounts{
              {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, {1, 0}},
              {omnibox::GROUP_MOBILE_CLIPBOARD, {1, 0}},
              {omnibox::GROUP_MOBILE_MOST_VISITED, {8, 0}},
              {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, {10, 0}}}));
  groups_.push_back(std::make_unique<MultiGroup>(
      5, MultiGroup::GroupLimitsAndCounts{{omnibox::GROUP_TRENDS, {5, 0}}}));
}

DesktopNonZpsSection::DesktopNonZpsSection(const ACMatches& matches)
    : Section(10) {
  groups_.push_back(std::make_unique<DefaultGroup>());
  groups_.push_back(std::make_unique<MultiGroup>(
      9, MultiGroup::GroupLimitsAndCounts{
             {omnibox::GROUP_SEARCH, {9, 0}},
             {omnibox::GROUP_HISTORY_CLUSTER, {1, 0}}}));
  groups_.push_back(std::make_unique<MultiGroup>(
      7,
      MultiGroup::GroupLimitsAndCounts{{omnibox::GROUP_OTHER_NAVS, {7, 0}}}));

  bool has_search = base::ranges::any_of(
      matches, [&](const auto& match) { return groups_[1]->CanAdd(match); });

  auto default_match = base::ranges::find_if(
      matches, [&](const auto& match) { return groups_[0]->CanAdd(match); });
  bool default_is_search =
      default_match != matches.end() && groups_[1]->CanAdd(*default_match);

  if (has_search && !default_is_search)
    groups_[2]->limit_--;
}

GroupBase* DesktopNonZpsSection::CanAdd(const AutocompleteMatch& match) {
  if (groups_[2]->CanAdd(match))
    limit_ = 8;
  return Section::CanAdd(match);
}
