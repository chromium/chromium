// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/ranges/algorithm.h"
#include "components/omnibox/browser/autocomplete_grouper_groups.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace {
constexpr size_t kMobileMostVisitedTilesLimit = 10;
constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);
}

Section::Section(size_t limit,
                 Groups groups,
                 omnibox::GroupConfigMap& group_configs,
                 omnibox::GroupConfig_SideType side_type)
    : limit_(limit),
      groups_(std::move(groups)),
      group_configs_(group_configs),
      side_type_(side_type) {
#if DCHECK_IS_ON()
  // Make sure all the `GroupId`s in the `Group`s have the same `RenderType`.
  for (const auto& group : groups_) {
    std::optional<omnibox::GroupConfig_RenderType> last_render_type;
    for (const auto& [group_id, _] : group.group_id_limits_and_counts()) {
      const auto& group_config = group_configs[group_id];

      DCHECK(last_render_type.value_or(group_config.render_type()) ==
             group_config.render_type());
      last_render_type = group_config.render_type();
    }
  }
#endif  // DCHECK_IS_ON()
}

Section::~Section() = default;

// static
ACMatches Section::GroupMatches(PSections sections, ACMatches& matches) {
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
  for (auto& section : sections) {
    for (auto& group : section->groups_) {
      if constexpr (is_android) {
        group.GroupMatchesBySearchVsUrl();
      }

      for (AutocompleteMatch* match : group.matches()) {
        grouped_matches.push_back(std::move(*match));
      }
    }
  }

  return grouped_matches;
}

Groups::iterator Section::FindGroup(const AutocompleteMatch& match) {
  // Check if match is allowed in this `Section` by its GroupId `SideType`.
  const auto& group_id = match.suggestion_group_id.value();
  if (group_configs_[group_id].side_type() != side_type_) {
    return groups_.end();
  }
  return base::ranges::find_if(
      groups_, [&](const auto& group) { return group.CanAdd(match); });
}

bool Section::Add(const AutocompleteMatch& match) {
  if (count_ >= limit_) {
    return false;
  }
  auto group_itr = FindGroup(match);
  if (group_itr == groups_.end()) {
    return false;
  }
  group_itr->Add(match);
  count_++;
  return true;
}

ZpsSection::ZpsSection(size_t limit,
                       Groups groups,
                       omnibox::GroupConfigMap& group_configs,
                       omnibox::GroupConfig_SideType side_type)
    : Section(limit, groups, group_configs, side_type) {}

void ZpsSection::InitFromMatches(ACMatches& matches) {
  // Sort matches in the order of their potential containing groups. E.g., if
  // `groups_ = {group 1, group 2}, this sorts all matches that can be added to
  // group 1 before those that can only be added to group 2.
  base::ranges::stable_sort(matches, std::less<int>{}, [&](const auto& match) {
    // Don't have to handle `FindGroup()` returning `groups_.end()` since
    // those matches won't be added to the section anyways.
    return std::distance(groups_.begin(), FindGroup(match));
  });
}

// Number of matches that fit in the visible section of the screen.
// This number includes the Default match, shown in the top section.
// The default match needs to be kept separate, because it should not be
// moved when we group suggestions by Search vs URL.
// TODO(b/328617350): plumb the value via AutocompleteInput.
/* static */ size_t AndroidNonZPSSection::num_visible_matches_{6};

AndroidNonZPSSection::AndroidNonZPSSection(
    bool show_only_search_suggestions,
    omnibox::GroupConfigMap& group_configs)
    : Section(
          15,
          {{1,  // Default match, not part of the Grouping.
            {{omnibox::GROUP_SEARCH, {1}},
             {omnibox::GROUP_OTHER_NAVS, {1u}},
             {omnibox::GROUP_MOBILE_RICH_ANSWER,
              {OmniboxFieldTrial::kAnswerActionsShowRichCard.Get() &&
                       !OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.Get()
                   ? 1u
                   : 0u}}}},

           {num_visible_matches_ - 1,  // Top section / above the keyboard.
            {{omnibox::GROUP_SEARCH, {14}},
             {omnibox::GROUP_OTHER_NAVS,
              {show_only_search_suggestions ? 0u : 14u}}}},
           {1,  // Dedicated section for rich answer card just above the fold.
            {{omnibox::GROUP_MOBILE_RICH_ANSWER,
              {OmniboxFieldTrial::kAnswerActionsShowRichCard.Get() &&
                       OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.Get()
                   ? 1u
                   : 0u}}}},
           {14,  // Bottom section, up to the Section limit.
            {{omnibox::GROUP_SEARCH, {14}},
             {omnibox::GROUP_OTHER_NAVS,
              {show_only_search_suggestions ? 0u : 14u}}}}},
          group_configs,
          omnibox::GroupConfig_SideType_DEFAULT_PRIMARY) {}

void AndroidNonZPSSection::InitFromMatches(ACMatches& matches) {
  auto rich_answer_match = base::ranges::find_if(
      matches,
      [&](const auto& match) { return match.answer_template.has_value(); });
  bool has_rich_answer = rich_answer_match != matches.end();
  if (!has_rich_answer) {
    return;
  }

  bool has_url = base::ranges::any_of(matches, [](const auto& match) {
    return !AutocompleteMatch::IsSearchType(match.type);
  });
  bool hide_if_urls_present =
      !OmniboxFieldTrial::kAnswerActionsShowIfUrlsPresent.Get();
  if (has_url && hide_if_urls_present) {
    rich_answer_match->suggestion_group_id = omnibox::GROUP_SEARCH;
  }

  if (!OmniboxFieldTrial::kAnswerActionsShowRichCard.Get() ||
      !OmniboxFieldTrial::kAnswerActionsShowAboveKeyboard.Get()) {
    return;
  }

  auto& above_keyboard_group = groups_[1];
  above_keyboard_group.set_limit(above_keyboard_group.limit() - 1);
}

AndroidHubNonZPSSection::AndroidHubNonZPSSection(
    omnibox::GroupConfigMap& group_configs)
    : Section(30,
              {{30, omnibox::GROUP_MOBILE_OPEN_TABS}},
              group_configs,
              omnibox::GroupConfig_SideType_DEFAULT_PRIMARY) {}

AndroidNTPZpsSection::AndroidNTPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(
          30,
          {
              {1, omnibox::GROUP_MOBILE_CLIPBOARD},
              {15, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
              {5, omnibox::GROUP_TRENDS},
          },
          group_configs) {}

AndroidSRPZpsSection::AndroidSRPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(15,
                 {
                     {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {15, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                     {15, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

AndroidWebZpsSection::AndroidWebZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(
          15,  // Excludes MV tile count (calculated at runtime).
          {
              {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
              {1, omnibox::GROUP_MOBILE_CLIPBOARD},
              {kMobileMostVisitedTilesLimit,
               omnibox::GROUP_MOBILE_MOST_VISITED},
              {8, omnibox::GROUP_VISITED_DOC_RELATED},
              {15, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
          },
          group_configs) {}

DesktopNTPZpsSection::DesktopNTPZpsSection(
    omnibox::GroupConfigMap& group_configs,
    size_t limit)
    : ZpsSection(limit,
                 {
                     {8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                     {8, omnibox::GROUP_TRENDS},
                 },
                 group_configs) {}

DesktopNTPZpsIPHSection::DesktopNTPZpsIPHSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(1,
                 {
                     {1, omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP},
                 },
                 group_configs) {}

DesktopSecondaryNTPZpsSection::DesktopSecondaryNTPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection((omnibox_feature_configs::
                      RealboxContextualAndTrendingSuggestions::Get()
                          .enabled)
                     ? omnibox_feature_configs::
                           RealboxContextualAndTrendingSuggestions::Get()
                               .total_limit
                     : 3,
                 {
                     {3, omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS},
                     {(omnibox_feature_configs::
                           RealboxContextualAndTrendingSuggestions::Get()
                               .enabled)
                          ? omnibox_feature_configs::
                                RealboxContextualAndTrendingSuggestions::Get()
                                    .contextual_suggestions_limit
                          : 0,
                      omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                     {(omnibox_feature_configs::
                           RealboxContextualAndTrendingSuggestions::Get()
                               .enabled)
                          ? omnibox_feature_configs::
                                RealboxContextualAndTrendingSuggestions::Get()
                                    .trending_suggestions_limit
                          : 0,
                      omnibox::GROUP_TRENDS},
                 },
                 group_configs,
                 omnibox::GroupConfig_SideType_SECONDARY) {}

DesktopSRPZpsSection::DesktopSRPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(8,
                 {
                     {8, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                     {8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

DesktopWebZpsSection::DesktopWebZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(8,
                 {
                     {8, omnibox::GROUP_VISITED_DOC_RELATED},
                     {8, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

DesktopLensContextualZpsSection::DesktopLensContextualZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(5,
                 {
                     {5, omnibox::GROUP_CONTEXTUAL_SEARCH},
                 },
                 group_configs) {}

DesktopLensMultimodalZpsSection::DesktopLensMultimodalZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(8,
                 {
                     {8, omnibox::GROUP_MULTIMODAL},
                 },
                 group_configs) {}

DesktopNonZpsSection::DesktopNonZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : Section(10,
              {
                  {1,
                   {
                       {omnibox::GROUP_STARTER_PACK, {1}},
                       {omnibox::GROUP_SEARCH, {1}},
                       {omnibox::GROUP_OTHER_NAVS, {1}},
                   },
                   /*is_default=*/true},
                  {9, omnibox::GROUP_STARTER_PACK},
                  {9,
                   {
                       {omnibox::GROUP_SEARCH, {9}},
                       {omnibox::GROUP_HISTORY_CLUSTER, {1}},
                   }},
                  {7, omnibox::GROUP_OTHER_NAVS},
              },
              group_configs,
              omnibox::GroupConfig_SideType_DEFAULT_PRIMARY) {}

void DesktopNonZpsSection::InitFromMatches(ACMatches& matches) {
  auto& default_group = groups_[0];
  auto& search_group = groups_[2];
  auto& nav_group = groups_[3];

  // Determine if `matches` contains any searches.
  bool has_search = base::ranges::any_of(
      matches, [&](const auto& match) { return search_group.CanAdd(match); });

  // Determine if the default match will be a search.
  auto default_match = base::ranges::find_if(
      matches, [&](const auto& match) { return default_group.CanAdd(match); });
  bool default_is_search =
      default_match != matches.end() && search_group.CanAdd(*default_match);

  // Find the 1st nav's index.
  size_t first_nav_index = std::distance(
      matches.begin(), base::ranges::find_if(matches, [&](const auto& match) {
        return nav_group.CanAdd(match);
      }));

  // Show at most 8 suggestions if doing so includes navs; otherwise show 9 or
  // 10, if doing so doesn't include navs.
  limit_ = std::clamp<size_t>(first_nav_index, 8, 10);

  // Show at least 1 search, either in the default group or the search group.
  if (has_search && !default_is_search) {
    DCHECK_GE(limit_, 2U);
    nav_group.set_limit(limit_ - 2);
  }
}

ZpsSectionWithMVTiles::ZpsSectionWithMVTiles(
    size_t limit,
    Groups groups,
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(limit, groups, group_configs) {}

void ZpsSectionWithMVTiles::InitFromMatches(ACMatches& matches) {
  size_t tile_count = std::count_if(
      matches.begin(), matches.end(), [](const AutocompleteMatch& m) {
        return m.suggestion_group_id.value_or(omnibox::GROUP_INVALID) ==
               omnibox::GROUP_MOBILE_MOST_VISITED;
      });
  // In the event we find more MV tiles than we can accommodate, trim the limit.
  limit_ += std::min(tile_count, kMobileMostVisitedTilesLimit);
  // Note that the horizontal render group takes a single slot in vertical list:
  // we therefore count it as an individual item, meaning this list:
  //     [ URL_WHAT_YOU_TYPED    ]
  //     [ [MV] [MV] [MV] [MV]   ]
  //     [ SEARCH_SUGGEST        ]
  // has 3 elements built from 6 AutocompleteMatch objects.
  limit_ -= (tile_count ? 1 : 0);
  ZpsSection::InitFromMatches(matches);
}

IOSNTPZpsSection::IOSNTPZpsSection(omnibox::GroupConfigMap& group_configs)
    : ZpsSection(26,
                 {
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {20, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                     {5, omnibox::GROUP_TRENDS},
                 },
                 group_configs) {}

IOSSRPZpsSection::IOSSRPZpsSection(omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(20,
                            {
                                // Verbatim match:
                                {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                                {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                                {kMobileMostVisitedTilesLimit,
                                 omnibox::GROUP_MOBILE_MOST_VISITED},
                                {8, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                                {20, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                            },
                            group_configs) {}

IOSWebZpsSection::IOSWebZpsSection(omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(20,
                            {
                                // Verbatim match:
                                {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                                {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                                {kMobileMostVisitedTilesLimit,
                                 omnibox::GROUP_MOBILE_MOST_VISITED},
                                {8, omnibox::GROUP_VISITED_DOC_RELATED},
                                {20, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                            },
                            group_configs) {}

IOSLensMultimodalZpsSection::IOSLensMultimodalZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(10,
                 {
                     {10, omnibox::GROUP_MULTIMODAL},
                 },
                 group_configs) {}

IOSIpadNTPZpsSection::IOSIpadNTPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSection(10,
                 {
                     {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                     {10, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                 },
                 group_configs) {}

IOSIpadSRPZpsSection::IOSIpadSRPZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(10,
                            {
                                // Verbatim match:
                                {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                                {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                                {kMobileMostVisitedTilesLimit,
                                 omnibox::GROUP_MOBILE_MOST_VISITED},
                                {8, omnibox::GROUP_PREVIOUS_SEARCH_RELATED},
                                {10, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                            },
                            group_configs) {}

IOSIpadWebZpsSection::IOSIpadWebZpsSection(
    omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(10,
                            {
                                // Verbatim match:
                                {1, omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX},
                                {1, omnibox::GROUP_MOBILE_CLIPBOARD},
                                {kMobileMostVisitedTilesLimit,
                                 omnibox::GROUP_MOBILE_MOST_VISITED},
                                {8, omnibox::GROUP_VISITED_DOC_RELATED},
                                {10, omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST},
                            },
                            group_configs) {}
