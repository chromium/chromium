// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_grouper_sections.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/omnibox/browser/autocomplete_grouper_groups.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "third_party/omnibox_proto/groups.pb.h"

namespace {
constexpr size_t kMobileMostVisitedTilesLimit = 10;
constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);
constexpr size_t kMaxSuggestionsPerUnscopedExtension = 4;
constexpr size_t kMaxExtensions = 2;
}  // namespace

Section::Section(size_t limit,
                 Groups groups,
                 const omnibox::GroupConfigMap& group_configs,
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
      const auto render_type =
          group_configs.contains(group_id)
              ? group_configs.at(group_id).render_type()
              : static_cast<::omnibox::GroupConfig_RenderType>(0);
      DCHECK_EQ(last_render_type.value_or(render_type), render_type)
          << "GroupId " << group_id
          << " has different RenderType than the previous one.";
      last_render_type = render_type;
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
      group.GroupMatches();
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
  return std::ranges::find_if(
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
                       const omnibox::GroupConfigMap& group_configs,
                       omnibox::GroupConfig_SideType side_type)
    : Section(limit, std::move(groups), group_configs, side_type) {}

void ZpsSection::InitFromMatches(ACMatches& matches) {
  // Ensure matches are sorted in the order of their potential containing
  // groups. E.g., if `groups_ = {group 1, group 2}, matches that can be added
  // to group 1 must appear before those that can only be added to group 2.
  size_t last_group_index = 0;
  for (const auto& match : matches) {
    auto group_itr = FindGroup(match);
    if (group_itr == groups_.end()) {
      continue;
    }
    size_t current_group_index = std::distance(groups_.begin(), group_itr);
    if (current_group_index < last_group_index) {
      const std::string match_type =
          AutocompleteMatchType::ToString(match.type);
      const std::string match_group_id =
          omnibox::GroupId_Name(match.suggestion_group_id.value());
      const std::string match_relevance = base::NumberToString(match.relevance);
      const std::string group_description = base::JoinString(
          [&]() {
            std::vector<std::string> transformed;
            std::ranges::transform(
                group_itr->group_id_limits_and_counts(),
                std::back_inserter(transformed), [](const auto& pair) {
                  return omnibox::GroupId_Name(pair.first) + " (" +
                         base::NumberToString(
                             static_cast<int>(pair.second.limit)) +
                         ")";
                });
            return transformed;
          }(),
          ", ");
#if DCHECK_IS_ON()
      NOTREACHED() << "Match with type " << match_type << " and group id "
                   << match_group_id << " and relevance " << match_relevance
                   << " is not sorted correctly while being added to Group "
                   << group_description;
#endif  // DCHECK_IS_ON()
    }
    last_group_index = current_group_index;
  }
}

ZpsSectionWithLocalHistory::ZpsSectionWithLocalHistory(
    size_t limit,
    Groups groups,
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(limit, std::move(groups), group_configs) {}

void ZpsSectionWithLocalHistory::InitFromMatches(ACMatches& matches) {
  // Sort matches in the order of their potential containing groups. E.g., if
  // `groups_ = {group 1, group 2}, this sorts all matches that can be added to
  // group 1 before those that can only be added to group 2.
  std::ranges::stable_sort(matches, std::less<int>{}, [&](const auto& match) {
    // Don't have to handle `FindGroup()` returning `groups_.end()` since
    // those matches won't be added to the section anyways.
    return std::distance(groups_.begin(), FindGroup(match));
  });
  ZpsSection::InitFromMatches(matches);
}

// Number of matches that fit in the visible section of the screen.
// This number includes the Default match, shown in the top section.
// The default match needs to be kept separate, because it should not be
// moved when we group suggestions by Search vs URL.
// TODO(b/328617350): plumb the value via AutocompleteInput.
/* static */ size_t AndroidNonZPSSection::num_visible_matches_{6};

AndroidNonZPSSection::AndroidNonZPSSection(
    bool show_only_search_suggestions,
    const omnibox::GroupConfigMap& group_configs)
    : Section(15,
              {
                  // Default match Group, not part of the Grouping.
                  Group(1,
                        {
                            {omnibox::GROUP_SEARCH, 1},
                            {omnibox::GROUP_OTHER_NAVS, 1},
                        },
                        /*is_zps=*/false),
                  // Top Group / above the keyboard.
                  Group(num_visible_matches_ - 1,
                        {
                            {omnibox::GROUP_SEARCH, 14},
                            {omnibox::GROUP_OTHER_NAVS,
                             show_only_search_suggestions ? 0 : 14},
                        },
                        /*is_zps=*/false),
                  // Dedicated Group for rich answer card just above the fold.
                  Group(1,
                        {
                            {omnibox::GROUP_MOBILE_RICH_ANSWER, 0},
                        },
                        /*is_zps=*/false),
                  // Bottom Group, up to the Section limit.
                  Group(14,
                        {
                            {omnibox::GROUP_SEARCH, 14},
                            {omnibox::GROUP_OTHER_NAVS,
                             show_only_search_suggestions ? 0 : 14},
                        },
                        /*is_zps=*/false),
              },
              group_configs) {}

void AndroidNonZPSSection::InitFromMatches(ACMatches& matches) {
  auto rich_answer_match = std::ranges::find_if(
      matches,
      [&](const auto& match) { return match.answer_template.has_value(); });
  bool has_rich_answer = rich_answer_match != matches.end();
  if (!has_rich_answer) {
    return;
  }

  auto& above_keyboard_group = groups_[1];
  above_keyboard_group.set_limit(above_keyboard_group.limit() - 1);
}

/* static */ size_t AndroidComposeboxNonZPSSection::num_attachments_;
/* static */ omnibox::ChromeAimToolsAndModels
    AndroidComposeboxNonZPSSection::tool_mode_;

AndroidComposeboxNonZPSSection::AndroidComposeboxNonZPSSection(
    const omnibox::GroupConfigMap& group_configs)
    : Section(
          tool_mode_ ==
                      omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED &&
                  num_attachments_ == 0
              ? 15
              : 1,
          {// Default match Group only, unless no attachments
           Group(15, {{omnibox::GROUP_SEARCH, 15}})},
          group_configs) {}

AndroidHubZPSSection::AndroidHubZPSSection(
    const omnibox::GroupConfigMap& group_configs)
    : Section(5,
              {
                  Group(5,
                        {
                            {omnibox::GROUP_MOBILE_OPEN_TABS, 5},
                        }),
              },
              group_configs) {}

AndroidHubNonZPSSection::AndroidHubNonZPSSection(
    const omnibox::GroupConfigMap& group_configs)
    : Section(
          35,
          {
              // Reserve most of the spots for open tabs.
              Group(20,
                    {
                        {omnibox::GROUP_MOBILE_OPEN_TABS, 20},
                    },
                    /*is_zps=*/false),
              Group(5,
                    {
                        {omnibox::GROUP_MOBILE_BOOKMARKS, 5},
                    },
                    /*is_zps=*/false),
              // LINT.IfChange(HubHistorySectionSlots)
              Group(5,
                    {
                        {omnibox::GROUP_MOBILE_HISTORY, 5},
                    },
                    /*is_zps=*/false),
              // LINT.ThenChange(//components/omnibox/browser/history_quick_provider.cc:HubHistoryMaxMatches)
              // Fallback to search suggestions at the bottom of the results.
              Group(5,
                    {
                        {omnibox::GROUP_SEARCH, 5},
                    },
                    /*is_zps=*/false),
          },
          group_configs) {}

void AndroidNTPZpsSection::InitFromMatches(ACMatches& matches) {
  bool mia_suggestions_detected =
      std::ranges::any_of(matches, [&](const auto& match) {
        return match.suggestion_group_id.value_or(omnibox::GROUP_INVALID) ==
               omnibox::GROUP_MIA_RECOMMENDATIONS;
      });

  if (omnibox_feature_configs::MiaZPS::Get()
          .suppress_psuggest_backfill_with_mia &&
      mia_suggestions_detected) {
    // Hacky and delicate, but follows a pattern found in other sections of this
    // file.
    const_cast<Group::LimitAndCount&>(
        groups_[1].group_id_limits_and_counts().at(
            omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST))
        .limit = 0;
  }

  ZpsSectionWithLocalHistory::InitFromMatches(matches);
}

AndroidNTPZpsSection::AndroidNTPZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    bool mia_enabled)
    : ZpsSectionWithLocalHistory(
          30,
          {
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_CLIPBOARD, 1},
                    }),
              Group(OmniboxFieldTrial::kOmniboxNumNtpZpsRecentSearches.Get(),
                    {
                        {
                            omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA,
                            mia_enabled
                                ? OmniboxFieldTrial::
                                      kOmniboxNumNtpZpsRecentSearches.Get()
                                : 0,
                        },
                        {
                            omnibox::GROUP_MIA_RECOMMENDATIONS,
                            mia_enabled
                                ? OmniboxFieldTrial::
                                      kOmniboxNumNtpZpsRecentSearches.Get()
                                : 0,
                        },
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
                         OmniboxFieldTrial::kOmniboxNumNtpZpsRecentSearches
                             .Get()},
                    }),
              Group(OmniboxFieldTrial::kOmniboxNumNtpZpsTrendingSearches.Get(),
                    {
                        {omnibox::GROUP_TRENDS,
                         OmniboxFieldTrial::kOmniboxNumNtpZpsTrendingSearches
                             .Get()},
                    }),
          },
          group_configs) {}

AndroidSRPZpsSection::AndroidSRPZpsSection(
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(
          15,
          {
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, 1},
                    }),
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_CLIPBOARD, 1},
                    }),
              Group(OmniboxFieldTrial::kOmniboxNumSrpZpsRelatedSearches.Get(),
                    {
                        {omnibox::GROUP_PREVIOUS_SEARCH_RELATED,
                         OmniboxFieldTrial::kOmniboxNumSrpZpsRelatedSearches
                             .Get()},
                    }),
              Group(OmniboxFieldTrial::kOmniboxNumSrpZpsRecentSearches.Get(),
                    {
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
                         OmniboxFieldTrial::kOmniboxNumSrpZpsRecentSearches
                             .Get()},
                    }),
          },
          group_configs) {}

AndroidWebZpsSection::AndroidWebZpsSection(
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(
          15,  // Excludes MV tile count (calculated at runtime).
          {
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, 1},
                    }),
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_CLIPBOARD, 1},
                    }),
              Group(OmniboxFieldTrial::kOmniboxNumWebZpsMostVisitedUrls.Get(),
                    {
                        {omnibox::GROUP_MOBILE_MOST_VISITED,
                         OmniboxFieldTrial::kOmniboxNumWebZpsMostVisitedUrls
                             .Get()},
                    }),
              Group(OmniboxFieldTrial::kOmniboxNumWebZpsRelatedSearches.Get(),
                    {
                        {omnibox::GROUP_VISITED_DOC_RELATED,
                         OmniboxFieldTrial::kOmniboxNumWebZpsRelatedSearches
                             .Get()},
                    }),
              Group(OmniboxFieldTrial::kOmniboxNumWebZpsRecentSearches.Get(),
                    {
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
                         OmniboxFieldTrial::kOmniboxNumWebZpsRecentSearches
                             .Get()},
                    }),
          },
          group_configs) {}

DesktopNTPZpsSection::DesktopNTPZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t limit,
    bool mia_enabled)
    : ZpsSectionWithLocalHistory(
          limit,
          {
              Group(
                  8,
                  {
                      {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA,
                       mia_enabled ? 8 : 0},
                      {omnibox::GROUP_MIA_RECOMMENDATIONS, mia_enabled ? 8 : 0},
                      {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, 8},
                  }),
              Group(8,
                    {
                        {omnibox::GROUP_TRENDS, 8},
                    }),
              Group(5,
                    {
                        {omnibox::GROUP_CONTEXTUAL_SEARCH, 5},
                    }),
          },
          group_configs) {}

DesktopNTPZpsIPHSection::DesktopNTPZpsIPHSection(
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(1,
                 {
                     Group(1,
                           {
                               {omnibox::GROUP_ZERO_SUGGEST_IN_PRODUCT_HELP, 1},
                           }),
                 },
                 group_configs) {}

DesktopZpsUnscopedExtensionSection::DesktopZpsUnscopedExtensionSection(
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(kMaxSuggestionsPerUnscopedExtension * kMaxExtensions,
                 {
                     Group(kMaxSuggestionsPerUnscopedExtension,
                           {
                               {omnibox::GROUP_UNSCOPED_EXTENSION_1,
                                kMaxSuggestionsPerUnscopedExtension},
                           }),
                     Group(kMaxSuggestionsPerUnscopedExtension,
                           {
                               {omnibox::GROUP_UNSCOPED_EXTENSION_2,
                                kMaxSuggestionsPerUnscopedExtension},
                           }),
                 },
                 group_configs) {}

DesktopSecondaryNTPZpsSection::DesktopSecondaryNTPZpsSection(
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(
          (omnibox_feature_configs::RealboxContextualAndTrendingSuggestions::
               Get()
                   .enabled)
              ? omnibox_feature_configs::
                    RealboxContextualAndTrendingSuggestions::Get()
                        .total_limit
              : 3,
          {
              Group(
                  3,
                  {
                      {omnibox::GROUP_PREVIOUS_SEARCH_RELATED_ENTITY_CHIPS, 3},
                  }),
              Group(
                  (omnibox_feature_configs::
                       RealboxContextualAndTrendingSuggestions::Get()
                           .enabled)
                      ? omnibox_feature_configs::
                            RealboxContextualAndTrendingSuggestions::Get()
                                .contextual_suggestions_limit
                      : 0,
                  {
                      {omnibox::GROUP_PREVIOUS_SEARCH_RELATED,
                       (omnibox_feature_configs::
                            RealboxContextualAndTrendingSuggestions::Get()
                                .enabled)
                           ? omnibox_feature_configs::
                                 RealboxContextualAndTrendingSuggestions::Get()
                                     .contextual_suggestions_limit
                           : 0},
                  }),
              Group(
                  (omnibox_feature_configs::
                       RealboxContextualAndTrendingSuggestions::Get()
                           .enabled)
                      ? omnibox_feature_configs::
                            RealboxContextualAndTrendingSuggestions::Get()
                                .trending_suggestions_limit
                      : 0,
                  {
                      {omnibox::GROUP_TRENDS,
                       (omnibox_feature_configs::
                            RealboxContextualAndTrendingSuggestions::Get()
                                .enabled)
                           ? omnibox_feature_configs::
                                 RealboxContextualAndTrendingSuggestions::Get()
                                     .trending_suggestions_limit
                           : 0},
                  }),
          },
          group_configs,
          omnibox::GroupConfig_SideType_SECONDARY) {}

DesktopSRPZpsSection::DesktopSRPZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t max_suggestions,
    size_t search_limit,
    size_t url_limit,
    size_t contextual_action_limit)
    : ZpsSection(
          max_suggestions,
          {
              Group(
                  search_limit,
                  {
                      {omnibox::GROUP_PREVIOUS_SEARCH_RELATED, search_limit},
                      {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, search_limit},
                  }),
              Group(url_limit,
                    {
                        {omnibox::GROUP_MOST_VISITED, url_limit},
                    }),
#if 1
              Group(contextual_action_limit,
                    {
                        {omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION,
                         contextual_action_limit},
                    }),
#endif
          },
          group_configs) {
}

DesktopWebURLZpsSection::DesktopWebURLZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t limit)
    : ZpsSection(limit,
                 {
                     Group(limit,
                           {
                               {omnibox::GROUP_MOST_VISITED, limit},
                           }),
                 },
                 group_configs) {}

DesktopWebSearchZpsSection::DesktopWebSearchZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t limit,
    size_t contextual_action_limit,
    size_t contextual_search_limit)
    : Section(limit,
              {
                  Group(limit,
                        {
                            {omnibox::GROUP_VISITED_DOC_RELATED, limit},
                            {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, limit},
                        }),
                  Group(contextual_action_limit,
                        {
                            {omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION,
                             contextual_action_limit},
                        }),
                  Group(contextual_search_limit,
                        {
                            {omnibox::GROUP_CONTEXTUAL_SEARCH,
                             contextual_search_limit},
                        }),
              },
              group_configs) {}

DesktopWebSearchZpsContextualOnlySection::
    DesktopWebSearchZpsContextualOnlySection(
        const omnibox::GroupConfigMap& group_configs,
        size_t contextual_action_limit,
        size_t contextual_search_limit)
    : Section(contextual_action_limit + contextual_search_limit,
              {
                  Group(contextual_action_limit,
                        {
                            {omnibox::GROUP_CONTEXTUAL_SEARCH_ACTION,
                             contextual_action_limit},
                        }),
                  Group(contextual_search_limit,
                        {
                            {omnibox::GROUP_CONTEXTUAL_SEARCH,
                             contextual_search_limit},
                        }),
              },
              group_configs) {}

DesktopLensContextualZpsSection::DesktopLensContextualZpsSection(
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(5,
                 {
                     Group(5,
                           {
                               {omnibox::GROUP_CONTEXTUAL_SEARCH, 5},
                           }),
                 },
                 group_configs) {}

DesktopLensMultimodalZpsSection::DesktopLensMultimodalZpsSection(
    const omnibox::GroupConfigMap& group_configs)
    : DesktopLensMultimodalZpsSection(group_configs, 8) {}

DesktopLensMultimodalZpsSection::DesktopLensMultimodalZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t max_suggestions)
    : ZpsSection(max_suggestions,
                 {
                     Group(max_suggestions,
                           {
                               {omnibox::GROUP_MULTIMODAL, max_suggestions},
                           }),
                 },
                 group_configs) {}

/* static */ size_t AndroidComposeboxZpsSection::num_attachments_;

AndroidComposeboxZpsSection::AndroidComposeboxZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t max_suggestions,
    size_t max_aim_suggestions,
    size_t max_contextual_suggestions)
    : ZpsSection(
          max_suggestions,
          {
              Group(max_suggestions,
                    {
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
                         num_attachments_ > 1 ? 0 : max_aim_suggestions},
                        {omnibox::GROUP_MIA_RECOMMENDATIONS,
                         num_attachments_ > 1 ? 0 : max_aim_suggestions},
                    }),
              Group(max_suggestions,
                    {
                        {omnibox::GROUP_AI_MODE_ZERO_SUGGEST_CANNED,
                         num_attachments_ > 1 ? 0 : max_aim_suggestions},
                    }),
              Group(max_suggestions,
                    {
                        {omnibox::GROUP_CONTEXTUAL_SEARCH,
                         num_attachments_ > 1 ? 0 : max_contextual_suggestions},
                    }),
          },
          group_configs) {}

IOSComposeboxZpsSection::IOSComposeboxZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t max_suggestions,
    size_t max_aim_suggestions,
    size_t max_contextual_suggestions)
    : ZpsSection(max_suggestions,
                 {
                     Group(max_suggestions,
                           {
                               {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
                                max_aim_suggestions},
                               {omnibox::GROUP_MIA_RECOMMENDATIONS,
                                max_aim_suggestions},
                           }),
                     Group(max_suggestions,
                           {
                               {omnibox::GROUP_AI_MODE_ZERO_SUGGEST_CANNED,
                                max_aim_suggestions},
                           }),
                     Group(max_suggestions,
                           {
                               {omnibox::GROUP_CONTEXTUAL_SEARCH,
                                max_contextual_suggestions},
                           }),
                 },
                 group_configs) {}

DesktopComposeboxZpsSection::DesktopComposeboxZpsSection(
    const omnibox::GroupConfigMap& group_configs,
    size_t max_suggestions,
    size_t max_aim_suggestions,
    size_t max_contextual_suggestions)
    : ZpsSection(max_suggestions,
                 {
                     Group(max_suggestions,
                           {
                               {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST,
                                max_aim_suggestions},
                               {omnibox::GROUP_MIA_RECOMMENDATIONS,
                                max_aim_suggestions},
                           }),
                     Group(max_suggestions,
                           {
                               {omnibox::GROUP_AI_MODE_ZERO_SUGGEST_CANNED,
                                max_aim_suggestions},
                           }),
                     Group(max_suggestions,
                           {
                               {omnibox::GROUP_CONTEXTUAL_SEARCH,
                                max_contextual_suggestions},
                           }),
                 },
                 group_configs) {}

ToolbeltSection::ToolbeltSection(const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(1,
                 {
                     Group(1,
                           {
                               {omnibox::GROUP_SEARCH_TOOLBELT, 1},
                           }),
                 },
                 group_configs) {}

DesktopNonZpsSection::DesktopNonZpsSection(
    const omnibox::GroupConfigMap& group_configs)
    : Section(10,
              {
                  Group(1,
                        {
                            {omnibox::GROUP_STARTER_PACK, 1},
                            {omnibox::GROUP_SEARCH, 1},
                            {omnibox::GROUP_OTHER_NAVS, 1},
                        },
                        /*is_zps=*/false,
                        /*is_default=*/true),
                  Group(9,
                        {
                            {omnibox::GROUP_STARTER_PACK, 9},
                        },
                        /*is_zps=*/false),
                  Group(9,
                        {
                            {omnibox::GROUP_SEARCH, 9},
                            {omnibox::GROUP_HISTORY_CLUSTER, 1},
                        },
                        /*is_zps=*/false),
                  Group(7,
                        {
                            {omnibox::GROUP_OTHER_NAVS, 7},
                        },
                        /*is_zps=*/false),
              },
              group_configs) {}

void DesktopNonZpsSection::InitFromMatches(ACMatches& matches) {
  auto& default_group = groups_[0];
  auto& search_group = groups_[2];
  auto& nav_group = groups_[3];

  // Determine if `matches` contains any searches.
  bool has_search = std::ranges::any_of(
      matches, [&](const auto& match) { return search_group.CanAdd(match); });

  // Determine if the default match will be a search.
  auto default_match = std::ranges::find_if(
      matches, [&](const auto& match) { return default_group.CanAdd(match); });
  bool default_is_search =
      default_match != matches.end() && search_group.CanAdd(*default_match);

  // Find the 1st nav's index.
  size_t first_nav_index = std::distance(
      matches.begin(), std::ranges::find_if(matches, [&](const auto& match) {
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
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(limit, std::move(groups), group_configs) {}

void ZpsSectionWithMVTiles::InitFromMatches(ACMatches& matches) {
  size_t tile_count = std::count_if(
      matches.begin(), matches.end(), [](const AutocompleteMatch& m) {
        return m.suggestion_group_id.value_or(omnibox::GROUP_INVALID) ==
               omnibox::GROUP_MOBILE_MOST_VISITED;
      });
  const size_t max_most_visited_tiles =
      is_android ? OmniboxFieldTrial::kOmniboxNumWebZpsMostVisitedUrls.Get()
                 : kMobileMostVisitedTilesLimit;
  // In the event we find more MV tiles than we can accommodate, trim the limit.
  limit_ += std::min(tile_count, max_most_visited_tiles);
  // Note that the horizontal render group takes a single slot in vertical list:
  // we therefore count it as an individual item, meaning this list:
  //     [ URL_WHAT_YOU_TYPED    ]
  //     [ [MV] [MV] [MV] [MV]   ]
  //     [ SEARCH_SUGGEST        ]
  // has 3 elements built from 6 AutocompleteMatch objects.
  limit_ -= (tile_count ? 1 : 0);
  ZpsSection::InitFromMatches(matches);
}

void IOSNTPZpsSection::InitFromMatches(ACMatches& matches) {
  bool mia_suggestions_detected =
      std::ranges::any_of(matches, [&](const auto& match) {
        return match.suggestion_group_id.value_or(omnibox::GROUP_INVALID) ==
               omnibox::GROUP_MIA_RECOMMENDATIONS;
      });

  if (omnibox_feature_configs::MiaZPS::Get()
          .suppress_psuggest_backfill_with_mia &&
      mia_suggestions_detected) {
    // Hacky and delicate, but follows a pattern found in other sections of this
    // file.
    const_cast<Group::LimitAndCount&>(
        groups_[1].group_id_limits_and_counts().at(
            omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST))
        .limit = 0;
  }

  ZpsSectionWithLocalHistory::InitFromMatches(matches);
}

IOSNTPZpsSection::IOSNTPZpsSection(const omnibox::GroupConfigMap& group_configs,
                                   bool mia_enabled)
    : ZpsSectionWithLocalHistory(
          26,
          {
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_CLIPBOARD, 1},
                    }),
              Group(20,
                    {
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST_WITH_MIA,
                         mia_enabled ? 20 : 0},
                        {omnibox::GROUP_MIA_RECOMMENDATIONS,
                         mia_enabled ? 20 : 0},
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, 20},
                    }),
              Group(5,
                    {
                        {omnibox::GROUP_TRENDS, 5},
                    }),
          },
          group_configs) {}

IOSSRPZpsSection::IOSSRPZpsSection(const omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(
          20,
          {
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, 1},
                    }),
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_CLIPBOARD, 1},
                    }),
              Group(kMobileMostVisitedTilesLimit,
                    {
                        {omnibox::GROUP_MOBILE_MOST_VISITED,
                         kMobileMostVisitedTilesLimit},
                    }),
              Group(8,
                    {
                        {omnibox::GROUP_PREVIOUS_SEARCH_RELATED, 8},
                    }),
              Group(20,
                    {
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, 20},
                    }),
          },
          group_configs) {}

IOSWebZpsSection::IOSWebZpsSection(const omnibox::GroupConfigMap& group_configs)
    : ZpsSectionWithMVTiles(
          20,
          {
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_SEARCH_READY_OMNIBOX, 1},
                    }),
              Group(1,
                    {
                        {omnibox::GROUP_MOBILE_CLIPBOARD, 1},
                    }),
              Group(kMobileMostVisitedTilesLimit,
                    {
                        {omnibox::GROUP_MOBILE_MOST_VISITED,
                         kMobileMostVisitedTilesLimit},
                    }),
              Group(8,
                    {
                        {omnibox::GROUP_VISITED_DOC_RELATED, 8},
                    }),
              Group(20,
                    {
                        {omnibox::GROUP_PERSONALIZED_ZERO_SUGGEST, 20},
                    }),
          },
          group_configs) {}

IOSLensMultimodalZpsSection::IOSLensMultimodalZpsSection(
    const omnibox::GroupConfigMap& group_configs)
    : ZpsSection(10,
                 {
                     Group(10,
                           {
                               {omnibox::GROUP_MULTIMODAL, 10},
                           }),
                 },
                 group_configs) {}
