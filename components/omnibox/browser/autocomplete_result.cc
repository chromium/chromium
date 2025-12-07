// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/actions/contextual_search_action.h"
#include "components/omnibox/browser/actions/omnibox_action_concepts.h"
#include "components/omnibox/browser/actions/omnibox_action_in_suggest.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/actions/tab_switch_action.h"
#include "components/omnibox/browser/autocomplete_grouper_sections.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/intranet_redirector_state.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/omnibox/common/omnibox_feature_configs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "extensions/buildflags/buildflags.h"
#include "omnibox_triggered_feature_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_features.h"  // nogncheck
#endif

using metrics::OmniboxEventProto;

typedef AutocompleteMatchType ACMatchType;

namespace {

constexpr bool is_android = !!BUILDFLAG(IS_ANDROID);
constexpr bool is_ios = !!BUILDFLAG(IS_IOS);
constexpr bool is_desktop = !(is_android || is_ios);

// Rotates |it| to be in the front of |matches|.
// |it| must be a valid iterator of |matches| or equal to |matches->end()|.
void RotateMatchToFront(ACMatches::iterator it, ACMatches* matches) {
  if (it == matches->end())
    return;

  auto next = std::next(it);
  std::rotate(matches->begin(), it, next);
}

// Maximum number of pedals to show.
// On iOS, the UI for pedals gets too visually cluttered with too many pedals.
constexpr size_t kMaxPedalCount =
    is_ios ? 1 : std::numeric_limits<size_t>::max();
// Maximum index of a match in a result for which the pedal should be displayed.
constexpr size_t kMaxPedalMatchIndex =
    is_ios ? 3 : std::numeric_limits<size_t>::max();

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
// The entrypoint id associated with aim being invoked from the AIM shortcut of
// typed state. Used for logging purposes.
// Do not change without changing the IDs in chrome_aim_entry_point.proto
omnibox::ChromeAimEntryPoint GetAimActionEntrypointID() {
  return is_android
             ? omnibox::ANDROID_CHROME_AIM_SHORTCUT_TYPED_STATE_ENTRY_POINT
             : omnibox::IOS_CHROME_OMNIBOX_SEARCH_ENTRY_POINT;
}
#endif

}  // namespace

// static
size_t AutocompleteResult::GetMaxMatches(
    bool is_zero_suggest,
    AutocompleteInput::FeaturedKeywordMode featured_keyword_mode) {
  constexpr size_t kDefaultMaxAutocompleteMatches =
      is_android ? 10 : (is_ios ? 10 : 8);
  constexpr size_t kDefaultMaxZeroSuggestMatches =
      is_android ? 15 : (is_ios ? 20 : 8);
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  constexpr size_t kMaxFeaturedKeywordAutocompleteMatches = 9;
#endif

  // Verify possible return values are between (0,
  // `kMaxAutocompletePositionValue`).
  static_assert(
      kDefaultMaxAutocompleteMatches > 0 &&
          kDefaultMaxAutocompleteMatches < kMaxAutocompletePositionValue,
      "Bad kDefaultMaxAutocompleteMatches.");
  static_assert(
      kDefaultMaxZeroSuggestMatches > 0 &&
          kDefaultMaxZeroSuggestMatches < kMaxAutocompletePositionValue,
      "Bad kDefaultMaxZeroSuggestMatches.");
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  static_assert(kMaxFeaturedKeywordAutocompleteMatches > 0 &&
                    kMaxFeaturedKeywordAutocompleteMatches <
                        kMaxAutocompletePositionValue,
                "Bad kMaxFeaturedKeywordAutocompleteMatches.");
#endif

// When the user types '@', show 9, instead of the usual 8, matches on desktop.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (featured_keyword_mode == AutocompleteInput::FeaturedKeywordMode::kExact)
    return kMaxFeaturedKeywordAutocompleteMatches;
#endif

  // If we're interested in the zero suggest match limit, and one has been
  // specified, return it.
  if (is_zero_suggest) {
    size_t field_trial_value = base::GetFieldTrialParamByFeatureAsInt(
        omnibox::kMaxZeroSuggestMatches,
        OmniboxFieldTrial::kMaxZeroSuggestMatchesParam,
        kDefaultMaxZeroSuggestMatches);
    DCHECK(kMaxAutocompletePositionValue > field_trial_value);
    return field_trial_value;
  }

  // Otherwise, i.e. if no zero suggest specific limit has been specified or the
  // input is not from omnibox focus, return the general max matches limit.
  size_t field_trial_value = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam,
      kDefaultMaxAutocompleteMatches);
  DCHECK(kMaxAutocompletePositionValue > field_trial_value);
  return field_trial_value;
}

// static
size_t AutocompleteResult::GetDynamicMaxMatches() {
  constexpr const int kDynamicMaxMatchesLimit = is_android ? 15 : 10;
  if (!base::FeatureList::IsEnabled(omnibox::kDynamicMaxAutocomplete))
    return AutocompleteResult::GetMaxMatches();
  return base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDynamicMaxAutocomplete,
      OmniboxFieldTrial::kDynamicMaxAutocompleteIncreasedLimitParam,
      kDynamicMaxMatchesLimit);
}

AutocompleteResult::AutocompleteResult()
    : max_url_matches_(is_android || is_ios ? 5 : 7) {
  matches_.reserve(kMaxAutocompletePositionValue);
}

AutocompleteResult::~AutocompleteResult() {
#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
#endif
}

void AutocompleteResult::TransferOldMatches(const AutocompleteInput& input,
                                            AutocompleteResult* old_matches) {
  // Skip any matches that would have already been added to the new matches if
  // they're still relevant:
  // - Don't transfer matches from done providers. If the match is still
  //   relevant, it'll already be in `internal_result_`, potentially with
  //   updated fields that shouldn't be deduped with the out-of-date match.
  //   Otherwise, the irrelevant match shouldn't be re-added. Adding outdated
  //   matches is particularly noticeable when the user types the next char
  //   before the copied matches are expired leading to outdated matches
  //   surviving multiple input changes, e.g. 'gooooooooo[oogle.com]'.
  // - Don't transfer match types that are guaranteed to be sync as they too
  //   would have been replaced by the new sync pass. E.g., It doesn't look good
  //   to show 2 URL-what-you-typed suggestions.
  // - Don't transfer action matches since matches are annotated and converted
  //   on every pass to keep them associated with the triggering match.
  // Exclude specialized suggestion types from being transferred to prevent
  // user-visible artifacts.
  std::erase_if(old_matches->matches_, [](const auto& old_match) {
    return old_match.type == AutocompleteMatchType::PEDAL ||
           (old_match.provider && old_match.provider->done()) ||
           old_match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
           old_match.type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
           old_match.type == AutocompleteMatchType::TILE_NAVSUGGEST ||
           old_match.type == AutocompleteMatchType::TILE_SUGGESTION;
  });

  if (old_matches->empty())
    return;

  if (empty()) {
    // If we've got no matches we can copy everything from the last result.
    SwapMatchesWith(old_matches);
    for (auto& match : *this)
      match.from_previous = true;
    return;
  }

  // In hopes of providing a stable popup we try to keep the number of matches
  // per provider consistent. Other schemes (such as blindly copying the most
  // relevant matches) typically result in many successive 'What You Typed'
  // results filling all the matches, which looks awful.
  //
  // Instead of starting with the current matches and then adding old matches
  // until we hit our overall limit, we copy enough old matches so that each
  // provider has at least as many as before, and then use SortAndCull() to
  // clamp globally. This way, old high-relevance matches will starve new
  // low-relevance matches, under the assumption that the new matches will
  // ultimately be similar.  If the assumption holds, this prevents seeing the
  // new low-relevance match appear and then quickly get pushed off the bottom;
  // if it doesn't, then once the providers are done and we expire the old
  // matches, the new ones will all become visible, so we won't have lost
  // anything permanently.
  //
  // Note that culling tail suggestions (see |MaybeCullTailSuggestions()|)
  // relies on the behavior below of capping the total number of suggestions to
  // the higher of the number of new and old suggestions.  Without it, a
  // provider could have one old and one new suggestion, cull tail suggestions,
  // expire the old suggestion, and restore tail suggestions.  This would be
  // visually unappealing, and could occur on each keystroke.
  ProviderToMatches matches_per_provider, old_matches_per_provider;
  BuildProviderToMatchesCopy(&matches_per_provider);
  // |old_matches| is going away soon, so we can move out the matches.
  old_matches->BuildProviderToMatchesMove(&old_matches_per_provider);
  for (auto& pair : old_matches_per_provider) {
    MergeMatchesByProvider(&pair.second, matches_per_provider[pair.first]);
  }

  // Make sure previous matches adhere to `input.prevent_inline_autocomplete()`.
  // Previous matches are demoted in `MergeMatchesByProvider()` anyways, making
  // them unlikely to be default; however, without this safeguard, they may
  // still be deduped with a higher-relevance yet not-allowed-to-be-default
  // match later, resulting in a default match with autocompletion even when
  // `prevent_inline_autocomplete` is true. Some providers don't set
  // `inline_autocompletion` for matches not allowed to be default, which
  // `SetAllowedToBeDefault()` relies on; so don't invoke it for those
  // suggestions. Skipping those suggestions is fine, since
  // `SetAllowedToBeDefault()` here is only intended to make
  // `allowed_to_be_default` more conservative (true -> false, not vice versa).
  for (auto& m : matches_) {
    if (!m.from_previous)
      continue;
    if (input.prevent_inline_autocomplete() && m.allowed_to_be_default_match) {
      m.SetAllowedToBeDefault(input);
    } else {
      // Transferred matches may no longer match the new input. E.g., when the
      // user types 'gi' (and presses enter), don't inline (and navigate to)
      // 'gi[oogle.com]'.
      m.allowed_to_be_default_match = false;
    }
  }
}

void AutocompleteResult::AppendMatches(const ACMatches& matches) {
  for (const auto& match : matches) {
    std::stringstream debug_stream;
    debug_stream << "Contents: " << match.contents;
    debug_stream << ", Description: " << match.description;
    debug_stream << ", Type: " << AutocompleteMatchType::ToString(match.type);
    std::string provider_name =
        match.provider ? match.provider->GetName() : "None";
    debug_stream << ", Provider: " << provider_name;

    DCHECK_EQ(AutocompleteMatch::SanitizeString(match.contents), match.contents)
        << debug_stream.str();
    DCHECK_EQ(AutocompleteMatch::SanitizeString(match.description),
              match.description)
        << debug_stream.str();
    const bool valid_destination_url =
        match.destination_url.is_valid() || match.destination_url.is_empty();
    DCHECK(valid_destination_url) << debug_stream.str();

    matches_.push_back(match);
  }
}

void AutocompleteResult::DeduplicateMatches(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Omnibox.AutocompletionTime.UpdateResult.DeduplicateMatches");

  DeduplicateMatches(&matches_, input, template_url_service);
}

void AutocompleteResult::Sort(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service,
    std::optional<AutocompleteMatch> default_match_to_preserve) {
  if (!is_ios)
    DemoteOnDeviceSearchSuggestions();

  const auto& page_classification = input.current_page_classification();
  CompareWithDemoteByType<AutocompleteMatch> comparing_object(
      page_classification);

  // Because tail suggestions are a "last resort", we cull the tail suggestions
  // if there are any non-default, non-tail suggestions.
  if (!is_android && !is_ios)
    MaybeCullTailSuggestions(&matches_, comparing_object);

  DeduplicateMatches(input, template_url_service);

  // Sort the matches by relevance and demotions.
  std::sort(matches_.begin(), matches_.end(), comparing_object);

  // Find the best match and rotate it to the front to become the default match.
  // TODO(manukh) Ranking and preserving the default suggestion should be done
  //   by the grouping framework.
  auto top_match = FindTopMatch(input, &matches_);
  if (default_match_to_preserve &&
      (top_match == matches_.end() ||
       top_match->type != AutocompleteMatchType::URL_WHAT_YOU_TYPED)) {
    const auto default_match_fields =
        GetMatchComparisonFields(default_match_to_preserve.value());
    const auto preserved_default_match =
        std::ranges::find_if(matches_, [&](const AutocompleteMatch& match) {
          // Find a duplicate match. Don't preserve suggestions that are not
          // default-able; e.g., typing 'xy' shouldn't preserve default
          // 'xz.com/xy'.
          return default_match_fields == GetMatchComparisonFields(match) &&
                 match.allowed_to_be_default_match;
        });
    if (preserved_default_match != matches_.end())
      top_match = preserved_default_match;
  }

  RotateMatchToFront(top_match, &matches_);

  // The search provider may pre-deduplicate search suggestions. It's possible
  // for the un-deduped search suggestion that replaces a default search
  // entity suggestion to not have had `ComputeStrippedDestinationURL()`
  // invoked. Make sure to invoke it now as `AutocompleteController` relies on
  // `stripped_destination_url` to detect result changes. If
  // `stripped_destination_url` is already set, i.e. it was not a pre-deduped
  // search suggestion, `ComputeStrippedDestinationURL()` will early exit.
  if (UndedupTopSearchEntityMatch(&matches_)) {
    matches_[0].ComputeStrippedDestinationURL(input, template_url_service);
  }
}

void AutocompleteResult::SortAndCull(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service,
    OmniboxTriggeredFeatureService* triggered_feature_service,
    bool is_lens_active,
    bool can_show_contextual_suggestions,
    bool mia_enabled,
    bool is_incognito,
    std::optional<AutocompleteMatch> default_match_to_preserve) {
  SCOPED_UMA_HISTOGRAM_TIMER_MICROS(
      "Omnibox.AutocompletionTime.UpdateResult.SortAndCull");
  Sort(input, template_url_service, default_match_to_preserve);

  const auto& page_classification = input.current_page_classification();
  CompareWithDemoteByType<AutocompleteMatch> comparing_object(
      page_classification);

  bool is_zero_suggest = input.IsZeroSuggest();
  const bool use_grouping_for_non_zps =
      base::FeatureList::IsEnabled(omnibox::kGroupingFrameworkForNonZPS) &&
      !is_zero_suggest;
  bool use_grouping = is_zero_suggest || use_grouping_for_non_zps;

  MergeSuggestionGroupsMap(
      omnibox::BuildDefaultGroupsForInput(input, is_incognito));
  // Grouping requires all matches have a group ID. To keep providers 'dumb',
  // they only assign IDs when their ID isn't obvious from the match type. Most
  // matches will instead set IDs here to keep providers 'dumb' and the
  // type->group mapping consistent between providers.
  if (use_grouping) {
    std::ranges::for_each(matches_, [&](auto& match) {
      if (!match.suggestion_group_id.has_value()) {
        match.suggestion_group_id =
            AutocompleteMatch::GetDefaultGroupId(match.type);
      }
      DCHECK(match.suggestion_group_id.has_value());
    });

    // Some providers give 0 relevance matches that are meant for deduping only
    // but shouldn't be shown otherwise. Filter them out.
    std::erase_if(matches_,
                  [&](const auto& match) { return match.relevance == 0; });
  }

  // Used to determine how many search / url suggestions should appear in zps
  // if kOmniboxZpsSuggestionLimit is enabled.
  auto zps_suggestion_limit_config =
      omnibox_feature_configs::OmniboxZpsSuggestionLimit::Get();
  size_t max_search_suggestions = 8u;
  size_t max_url_suggestions = 0u;
  size_t max_suggestions = 8u;
  if (zps_suggestion_limit_config.enabled) {
    max_search_suggestions = zps_suggestion_limit_config.max_search_suggestions;
    max_url_suggestions = zps_suggestion_limit_config.max_url_suggestions;
    max_suggestions = zps_suggestion_limit_config.max_suggestions;
  }

  // If at zero suggest or `kGroupingFrameworkForNonZPS` is enabled and the
  // current input & platform are supported, delegate to the framework.
  if (is_zero_suggest) {
    PSections sections;
    if constexpr (is_android) {
      if (omnibox::IsNTPPage(page_classification)) {
        sections.push_back(std::make_unique<AndroidNTPZpsSection>(
            suggestion_groups_map_, mia_enabled));
      } else if (omnibox::IsSearchResultsPage(page_classification)) {
        sections.push_back(
            std::make_unique<AndroidSRPZpsSection>(suggestion_groups_map_));
      } else if (omnibox::IsAndroidHub(page_classification)) {
        sections.push_back(
            std::make_unique<AndroidHubZPSSection>(suggestion_groups_map_));
      } else if (omnibox::IsComposebox(page_classification)) {
        auto composebox_suggestion_limit_config =
            omnibox_feature_configs::ComposeboxSuggestionLimit::Get();
        size_t composebox_max_suggestions = 15u;
        size_t max_aim_suggestions = 15u;
        size_t max_contextual_suggestions = 15u;
        if (composebox_suggestion_limit_config.enabled) {
          composebox_max_suggestions =
              composebox_suggestion_limit_config.max_suggestions;
          max_aim_suggestions =
              composebox_suggestion_limit_config.max_aim_suggestions;
          max_contextual_suggestions =
              composebox_suggestion_limit_config.max_contextual_suggestions;
        }
        sections.push_back(std::make_unique<AndroidComposeboxZpsSection>(
            suggestion_groups_map_, composebox_max_suggestions,
            max_aim_suggestions, max_contextual_suggestions));
      } else {
        sections.push_back(
            std::make_unique<AndroidWebZpsSection>(suggestion_groups_map_));
      }
    } else if constexpr (is_desktop) {
      const size_t contextual_zps_limit =
          can_show_contextual_suggestions && !is_lens_active
              ? omnibox_feature_configs::ContextualSearch::Get()
                    .contextual_zps_limit
              : 0u;
      const size_t contextual_action_limit =
          omnibox_feature_configs::ContextualSearch::Get()
                      .show_open_lens_action &&
                  !is_lens_active
              ? 1u
              : 0u;
      if (omnibox::IsLensSearchbox(page_classification)) {
        switch (page_classification) {
          case OmniboxEventProto::CONTEXTUAL_SEARCHBOX:
          case OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX:
            sections.push_back(
                std::make_unique<DesktopLensContextualZpsSection>(
                    suggestion_groups_map_));
            break;
          case OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX:
            sections.push_back(
                std::make_unique<DesktopLensMultimodalZpsSection>(
                    suggestion_groups_map_));
            break;
          case OmniboxEventProto::LENS_SIDE_PANEL_COMPOSEBOX: {
            size_t max_aim_suggestions =
                lens::features::GetLensAimSuggestionsCount();
            // Always add contextual suggestions
            sections.push_back(std::make_unique<DesktopComposeboxZpsSection>(
                suggestion_groups_map_, max_aim_suggestions,
                max_aim_suggestions, max_aim_suggestions));
            // Add multimodal suggestions if enabled.
            if (lens::features::GetLensAimSuggestionsType() ==
                lens::features::LensAimSuggestionsType::kMultimodal) {
              sections.push_back(
                  std::make_unique<DesktopLensMultimodalZpsSection>(
                      suggestion_groups_map_, max_aim_suggestions));
            }
            break;
          }
          default:
            NOTREACHED();
        }
      } else if (omnibox::IsNTPPage(page_classification)) {
        // IPH is shown for NTP ZPS in the Omnibox only.  If it is shown, reduce
        // the limit of the normal NTP ZPS Section to make room for the IPH.
        bool has_iph_match =
            std::ranges::any_of(matches_, &AutocompleteMatch::IsIphSuggestion);
        bool add_iph_section =
            page_classification != OmniboxEventProto::NTP_REALBOX &&
            has_iph_match;
        sections.push_back(std::make_unique<DesktopNTPZpsSection>(
            suggestion_groups_map_, add_iph_section ? 7u : 8u, mia_enabled));
#if BUILDFLAG(ENABLE_EXTENSIONS)
        // Show unscoped extension suggestions on NTP except in the realbox.
        if (base::FeatureList::IsEnabled(
                extensions_features::kExperimentalOmniboxLabs)) {
          sections.push_back(
              std::make_unique<DesktopZpsUnscopedExtensionSection>(
                  suggestion_groups_map_));
        }
#endif
        if (add_iph_section) {
          sections.push_back(std::make_unique<DesktopNTPZpsIPHSection>(
              suggestion_groups_map_));
        }

        // Allow secondary zero-prefix suggestions in the NTP realbox only.
        if (page_classification == OmniboxEventProto::NTP_REALBOX) {
          sections.push_back(std::make_unique<DesktopSecondaryNTPZpsSection>(
              suggestion_groups_map_));
          // Report whether secondary zero-prefix suggestions were triggered.
          if (std::ranges::any_of(
                  suggestion_groups_map_, [](const auto& entry) {
                    return entry.second.side_type() ==
                           omnibox::GroupConfig_SideType_SECONDARY;
                  })) {
            triggered_feature_service->FeatureTriggered(
                metrics::
                    OmniboxEventProto_Feature_REMOTE_SECONDARY_ZERO_SUGGEST);
          }
        }
      } else if (omnibox::IsSearchResultsPage(page_classification)) {
        sections.push_back(std::make_unique<DesktopSRPZpsSection>(
            suggestion_groups_map_, max_suggestions + contextual_action_limit,
            max_search_suggestions, max_url_suggestions,
            contextual_action_limit));
#if BUILDFLAG(ENABLE_EXTENSIONS)
        if (base::FeatureList::IsEnabled(
                extensions_features::kExperimentalOmniboxLabs)) {
          sections.push_back(
              std::make_unique<DesktopZpsUnscopedExtensionSection>(
                  suggestion_groups_map_));
        }
#endif
      } else if (omnibox::IsComposebox(page_classification)) {
        auto composebox_suggestion_limit_config =
            omnibox_feature_configs::ComposeboxSuggestionLimit::Get();
        size_t composebox_max_suggestions = 8u;
        size_t max_aim_suggestions = 8u;
        size_t max_contextual_suggestions = 8u;
        if (composebox_suggestion_limit_config.enabled) {
          composebox_max_suggestions =
              composebox_suggestion_limit_config.max_suggestions;
          max_aim_suggestions =
              composebox_suggestion_limit_config.max_aim_suggestions;
          max_contextual_suggestions =
              composebox_suggestion_limit_config.max_contextual_suggestions;
        }
        sections.push_back(std::make_unique<DesktopComposeboxZpsSection>(
            suggestion_groups_map_, composebox_max_suggestions,
            max_aim_suggestions, max_contextual_suggestions));
      } else {
        if (contextual_zps_limit > 0u &&
            omnibox_feature_configs::ContextualSearch::Get()
                .contextual_suggestions_ablate_others_when_present &&
            std::ranges::any_of(matches_, [](const auto& match) {
              return match.IsContextualSearchSuggestion();
            })) {
          if (omnibox_feature_configs::ContextualSearch::Get()
                  .contextual_suggestions_ablate_search_only) {
            // URL suggestions.
            sections.push_back(std::make_unique<DesktopWebURLZpsSection>(
                suggestion_groups_map_, max_url_suggestions));
            // ONLY contextual search suggestions.
            sections.push_back(
                std::make_unique<DesktopWebSearchZpsContextualOnlySection>(
                    suggestion_groups_map_, contextual_action_limit,
                    contextual_zps_limit));
          } else if (omnibox_feature_configs::ContextualSearch::Get()
                         .contextual_suggestions_ablate_url_only) {
            // Regular search suggestions + contextual search suggestions.
            sections.push_back(std::make_unique<DesktopWebSearchZpsSection>(
                suggestion_groups_map_,
                max_search_suggestions + contextual_action_limit,
                contextual_action_limit, contextual_zps_limit));
          } else {
            // ONLY contextual search suggestions.
            sections.push_back(
                std::make_unique<DesktopWebSearchZpsContextualOnlySection>(
                    suggestion_groups_map_, contextual_action_limit,
                    contextual_zps_limit));
          }
        } else {
          // URL suggestions.
          sections.push_back(std::make_unique<DesktopWebURLZpsSection>(
              suggestion_groups_map_, max_url_suggestions));
          // Regular search suggestions + contextual search suggestions.
          sections.push_back(std::make_unique<DesktopWebSearchZpsSection>(
              suggestion_groups_map_,
              max_search_suggestions + contextual_action_limit,
              contextual_action_limit, contextual_zps_limit));
        }
#if BUILDFLAG(ENABLE_EXTENSIONS)
        if (base::FeatureList::IsEnabled(
                extensions_features::kExperimentalOmniboxLabs)) {
          sections.push_back(
              std::make_unique<DesktopZpsUnscopedExtensionSection>(
                  suggestion_groups_map_));
        }
#endif
      }
      if (omnibox_feature_configs::Toolbelt::Get().enabled) {
        sections.push_back(
            std::make_unique<ToolbeltSection>(suggestion_groups_map_));
      }
    } else if constexpr (is_ios) {
      if (omnibox::IsLensSearchbox(page_classification)) {
        switch (page_classification) {
          case OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX:
            sections.push_back(std::make_unique<IOSLensMultimodalZpsSection>(
                suggestion_groups_map_));
            break;
          default:
            // kLensOverlayNotFatalUntil update after launch.
            NOTREACHED(base::NotFatalUntil::M200);
        }
      } else if (omnibox::IsNTPPage(page_classification)) {
        sections.push_back(std::make_unique<IOSNTPZpsSection>(
            suggestion_groups_map_, mia_enabled));
      } else if (omnibox::IsSearchResultsPage(page_classification)) {
        sections.push_back(
            std::make_unique<IOSSRPZpsSection>(suggestion_groups_map_));
      } else if (omnibox::IsComposebox(page_classification)) {
        auto composebox_suggestion_limit_config =
            omnibox_feature_configs::ComposeboxSuggestionLimit::Get();
        size_t composebox_max_suggestions = 15u;
        size_t max_aim_suggestions = 15u;
        size_t max_contextual_suggestions = 15u;
        if (composebox_suggestion_limit_config.enabled) {
          composebox_max_suggestions =
              composebox_suggestion_limit_config.max_suggestions;
          max_aim_suggestions =
              composebox_suggestion_limit_config.max_aim_suggestions;
          max_contextual_suggestions =
              composebox_suggestion_limit_config.max_contextual_suggestions;
        }
        sections.push_back(std::make_unique<IOSComposeboxZpsSection>(
            suggestion_groups_map_, composebox_max_suggestions,
            max_aim_suggestions, max_contextual_suggestions));
      } else {
        sections.push_back(
            std::make_unique<IOSWebZpsSection>(suggestion_groups_map_));
      }
    }
    matches_ = Section::GroupMatches(std::move(sections), matches_);
  } else if (use_grouping_for_non_zps) {
    PSections sections;
    if constexpr (is_android) {
      if (omnibox::IsAndroidHub(page_classification)) {
        sections.push_back(
            std::make_unique<AndroidHubNonZPSSection>(suggestion_groups_map_));
      } else if (omnibox::IsComposebox(page_classification) &&
                 (input.lens_overlay_suggest_inputs() ||
                  input.aim_tool_mode() ==
                      omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN)) {
        sections.push_back(std::make_unique<AndroidComposeboxNonZPSSection>(
            suggestion_groups_map_));
      } else {
        bool show_only_search_suggestions =
            omnibox::IsCustomTab(page_classification);
        sections.push_back(std::make_unique<AndroidNonZPSSection>(
            show_only_search_suggestions, suggestion_groups_map_));
      }
    } else {
      sections.push_back(
          std::make_unique<DesktopNonZpsSection>(suggestion_groups_map_));
    }
    matches_ = Section::GroupMatches(std::move(sections), matches_);
  } else {
    // Limit history cluster suggestions to 1. This has to be done before
    // limiting URL matches below so that a to-be-removed history cluster
    // suggestion doesn't waste a URL slot.
    bool history_cluster_included = false;
    std::erase_if(matches_, [&](const auto& match) {
      // If not a history cluster match, don't erase it.
      if (match.type != AutocompleteMatch::Type::HISTORY_CLUSTER) {
        return false;
      }
      // If not the 1st history cluster match, do erase it.
      if (history_cluster_included) {
        return true;
      }
      // If the 1st history cluster match, don't erase it.
      history_cluster_included = true;
      return false;
    });

    // Limit URL matches.
    if (input.GetFeaturedKeywordMode() !=
        AutocompleteInput::FeaturedKeywordMode::kExact) {
      LimitNumberOfURLsShown(GetMaxMatches(is_zero_suggest), comparing_object);
    }

    // Limit total matches accounting for suggestions score <= 0, sub matches,
    // and feature configs such as OmniboxUIExperimentMaxAutocompleteMatches,
    // OmniboxMaxZeroSuggestMatches, and OmniboxDynamicMaxAutocomplete.
    const size_t num_matches =
        CalculateNumMatches(is_zero_suggest, input.GetFeaturedKeywordMode(),
                            matches_, comparing_object);

    // Group and trim suggestions to the given limit.
    matches_.resize(num_matches);

    // Group search suggestions above URL suggestions.
    if (matches_.size() > 2 && is_desktop) {
      GroupSuggestionsBySearchVsURL(std::next(matches_.begin()),
                                    matches_.end());
    }
    GroupAndDemoteMatchesInGroups();
  }

#if DCHECK_IS_ON()
  // If the user explicitly typed a scheme, the default match should have the
  // same scheme. This doesn't apply in these cases:
  //  - If the default match has no |destination_url|. An example of this is the
  //    default match after the user has tabbed into keyword search mode, but
  //    has not typed a query yet.
  //  - The default match is a Search for a query that resembles scheme (e.g.
  //    "chrome:", "chrome:123", etc.).
  //  - The user is using on-focus or on-clobber (ZeroSuggest) mode. In those
  //    modes, there is no explicit user input so these checks don't make sense.
  auto* default_match = this->default_match();
  if (default_match && default_match->destination_url.is_valid() &&
      !AutocompleteMatch::IsSearchType(default_match->type) &&
      !input.IsZeroSuggest() &&
      input.type() == metrics::OmniboxInputType::URL &&
      input.parts().scheme.is_nonempty()) {
    const std::u16string debug_info =
        u"fill_into_edit=" + default_match->fill_into_edit + u", provider=" +
        ((default_match->provider != nullptr)
             ? base::ASCIIToUTF16(default_match->provider->GetName())
             : std::u16string()) +
        u", input=" + input.text();

    const std::string& in_scheme = base::UTF16ToUTF8(input.scheme());
    const std::string& dest_scheme = default_match->destination_url.GetScheme();
    DCHECK(url_formatter::IsEquivalentScheme(in_scheme, dest_scheme))
        << debug_info;
  }
#endif
}

void AutocompleteResult::TrimOmniboxActions(bool is_zero_suggest) {
  // Platform rules:
  // Mobile:
  // - First position allow all types of OmniboxActionId (ACTION_IN_SUGGEST and
  //   ANSWER_ACTION are preferred over PEDAL)
  // - The 2nd and 3rd slot permit only PEDALs, ANSWER_ACTION, or
  //   ACTION_IN_SUGGEST (Android only).
  // - Slots 4 and beyond only permit ANSWER_ACTION or ACTION_IN_SUGGEST
  //   (Android only)
  // - TAB_SWITCH actions are not considered because they're never attached.
  //   On Android, the tab switch match is attached as ACTION_IN_SUGGEST.
  if constexpr (is_android || is_ios) {
    static constexpr size_t ACTIONS_IN_SUGGEST_CUTOFF_THRESHOLD = 1;
    static constexpr size_t PEDALS_CUTOFF_THRESHOLD = 3;
    std::vector<OmniboxActionId> include_all{OmniboxActionId::ACTION_IN_SUGGEST,
                                             OmniboxActionId::PEDAL};
    std::vector<OmniboxActionId> include_pedals_and_others;
    std::vector<OmniboxActionId> exclude_pedals;
    if constexpr (is_android) {
      include_pedals_and_others.push_back(OmniboxActionId::ACTION_IN_SUGGEST);
      exclude_pedals.push_back(OmniboxActionId::ACTION_IN_SUGGEST);
    }
    include_pedals_and_others.push_back(OmniboxActionId::PEDAL);

    for (size_t index = 0u; index < matches_.size(); ++index) {
      matches_[index].FilterOmniboxActions(
          (!is_zero_suggest && index < ACTIONS_IN_SUGGEST_CUTOFF_THRESHOLD)
              ? include_all
          : index < PEDALS_CUTOFF_THRESHOLD ? include_pedals_and_others
                                            : exclude_pedals);
      if (index < ACTIONS_IN_SUGGEST_CUTOFF_THRESHOLD) {
        matches_[index].FilterAndSortActionsInSuggest();
      }
    }
  }
}

void AutocompleteResult::SplitActionsToSuggestions() {
  const size_t size_before = size();
  if (size_before == 0) {
    return;
  }
  for (size_t i = 0; i < matches_.size(); i++) {
    if (matches_[i].IsToolbelt()) {
      // The toolbelt has pedal-like actions that should not be split out.
      continue;
    }
    for (size_t j = 0; j < matches_[i].actions.size(); j++) {
      if (matches_[i].actions[j]->ActionId() == OmniboxActionId::PEDAL) {
        *matches_.insert(matches_.begin() + i + 1,
                         matches_[i].CreateActionMatch(j));
        // Remove this action from the primary match and repeat checking at this
        // same index, which will hence be the next action.
        matches_[i].actions.erase(matches_[i].actions.begin() + j);
        j--;
      }
    }
  }
  // By design, do not change result size. But allow triggering
  // for the edge case where the pedal extends a list that still
  // does not exceed maximum.
  if (matches_[size() - 1].type != AutocompleteMatchType::PEDAL ||
      size() > GetDynamicMaxMatches()) {
    matches_.resize(size_before);
  }
}

void AutocompleteResult::GroupAndDemoteMatchesInGroups() {
  bool any_matches_in_groups = false;
  for (auto& match : *this) {
    if (!match.suggestion_group_id.has_value()) {
      continue;
    }

    const omnibox::GroupId group_id = match.suggestion_group_id.value();
    if (!base::Contains(suggestion_groups_map(), group_id)) {
      // Strip group IDs from the matches for which there is no suggestion
      // group information. These matches should instead be treated as
      // ordinary matches with no group IDs.
      match.suggestion_group_id.reset();
      continue;
    }

    any_matches_in_groups = true;

    // Record suggestion group information into the additional_info field
    // for chrome://omnibox.
    match.RecordAdditionalInfo("group id", group_id);
    match.RecordAdditionalInfo("group header",
                               GetHeaderForSuggestionGroup(group_id));
    match.RecordAdditionalInfo("group section",
                               GetSectionForSuggestionGroup(group_id));
  }

  // No need to group and demote matches in groups if none exists.
  if (!any_matches_in_groups) {
    return;
  }

  // Sort matches by their groups' section while preserving the existing order
  // within sections. Matches not in a group are ranked above matches in one.
  // 1) Suggestions without a group will be sorted first.
  // 2) Suggestions in SECTION_DEFAULT (0) and suggestions whose groups are not
  //    in `suggestion_groups_map_` are sorted 2nd.
  // 3) Remaining suggestions are sorted by section.
  std::ranges::stable_sort(
      matches_, [](int a, int b) { return a < b; },
      [&](const auto& m) {
        return m.suggestion_group_id.has_value()
                   ? GetSectionForSuggestionGroup(m.suggestion_group_id.value())
                   // -1 makes sure suggestions without a group are sorted
                   // before suggestions in the default section (0).
                   : -1;
      });
}

void AutocompleteResult::DemoteOnDeviceSearchSuggestions() {
  std::vector<AutocompleteMatch*> on_device_search_suggestions;
  int search_provider_search_suggestion_min_relevance = -1,
      on_device_search_suggestion_max_relevance = -1;
  bool search_provider_search_suggestion_exists = false;

  // Loop through all matches to check the existence of SearchProvider search
  // suggestions and OnDeviceProvider search suggestions. Also calculate the
  // maximum OnDeviceProvider search suggestion relevance and the minimum
  // SearchProvider search suggestion relevance, in preparation to adjust the
  // relevances for OnDeviceProvider search suggestions next.
  for (auto& m : matches_) {
    // The demotion will not be triggered if only trivial suggestions present,
    // which include type SEARCH_WHAT_YOU_TYPED & SEARCH_OTHER_ENGINE.
    // Note that we exclude SEARCH_OTHER_ENGINE here, simply because custom
    // search engine ("keyword search") is not enabled at Android & iOS, where
    // on device suggestion providers will be enabled. We should revisit this
    // triggering condition once keyword search is launched at Android & iOS.
    if (m.IsSearchProviderSearchSuggestion() && !m.IsTrivialAutocompletion()) {
      search_provider_search_suggestion_exists = true;
      search_provider_search_suggestion_min_relevance =
          search_provider_search_suggestion_min_relevance < 0
              ? m.relevance
              : std::min(search_provider_search_suggestion_min_relevance,
                         m.relevance);
    } else if (m.IsOnDeviceSearchSuggestion()) {
      on_device_search_suggestions.push_back(&m);
      on_device_search_suggestion_max_relevance =
          std::max(on_device_search_suggestion_max_relevance, m.relevance);
    }
  }

  // If any OnDeviceProvider search suggestion has a higher relevance than any
  // SearchProvider one, subtract the difference b/w the maximum
  // OnDeviceProvider search suggestion relevance and the minimum SearchProvider
  // search suggestion relevance from the relevances for all OnDeviceProvider
  // ones.
  if (search_provider_search_suggestion_exists &&
      !on_device_search_suggestions.empty()) {
    if (on_device_search_suggestion_max_relevance >=
        search_provider_search_suggestion_min_relevance) {
      int relevance_offset =
          (on_device_search_suggestion_max_relevance -
           search_provider_search_suggestion_min_relevance + 1);
      for (auto* m : on_device_search_suggestions)
        m->relevance = m->relevance > relevance_offset
                           ? m->relevance - relevance_offset
                           : 0;
    }
  }
}

void AutocompleteResult::AttachPedalsToMatches(
    const AutocompleteInput& input,
    const AutocompleteProviderClient& client) {
  OmniboxPedalProvider* provider = client.GetPedalProvider();
  if (!provider) {
    return;
  }

  // Used to ensure we keep only one Pedal of each kind.
  std::unordered_set<OmniboxPedal*> pedals_found;

  const size_t max_index = std::min(kMaxPedalMatchIndex, matches_.size());

  for (size_t i = 0; i < max_index && pedals_found.size() < kMaxPedalCount;
       i++) {
    AutocompleteMatch& match = matches_[i];
    // Skip matches that already have a pedal or are not suitable for actions.
    constexpr auto is_pedal = [](const auto& action) {
      return action->ActionId() == OmniboxActionId::PEDAL;
    };
    if (match.GetActionWhere(is_pedal) || !match.IsActionCompatible()) {
      continue;
    }

    OmniboxPedal* const pedal =
        provider->FindReadyPedalMatch(input, match.contents);
    if (pedal) {
      const auto result = pedals_found.insert(pedal);
      if (result.second) {
        match.actions.push_back(pedal);
      }
    }
  }
}

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
void AutocompleteResult::AttachAimAction(
    TemplateURLService* template_url_service,
    AutocompleteProviderClient* client) {
  if (!OmniboxFieldTrial::IsDeterministicAimActionInTypedStateEnabled(client)) {
    return;
  }

  for (AutocompleteMatch& match : matches_) {
    if (!match.actions.empty()) {
      continue;
    }
    if (match.allowed_to_be_default_match &&
        AutocompleteMatch::IsSearchType(match.type) &&
        match.contents.length() >=
            static_cast<size_t>(
                OmniboxFieldTrial::kMinimumTypedCharactersToInvokeAimShortcut
                    .Get())) {
      omnibox::SuggestTemplateInfo::TemplateAction template_action;
      template_action.set_action_type(
          omnibox::SuggestTemplateInfo_TemplateAction_ActionType_CHROME_AIM);
      template_action.set_action_uri(
          GetUrlForAim(template_url_service, GetAimActionEntrypointID(),
                       /*query_start_time=*/base::Time::Now(), match.contents)
              .spec());
      match.actions.emplace_back(base::MakeRefCounted<OmniboxActionInSuggest>(
          std::move(template_action), std::nullopt));
      // Only attach to the first eligible match.
      return;
    }
  }
}
#endif

void AutocompleteResult::AttachContextualSearchFulfillmentActionToMatches() {
  for (AutocompleteMatch& match : matches_) {
    if (match.IsContextualSearchSuggestion() && !match.HasLensSearchAction()) {
      match.takeover_action =
          base::MakeRefCounted<ContextualSearchFulfillmentAction>(
              match.destination_url, match.type,
              match.subtypes.contains(omnibox::SUBTYPE_ZERO_PREFIX));
    }
  }
}

void AutocompleteResult::AttachContextualSearchOpenLensActionToMatches() {
  for (AutocompleteMatch& match : matches_) {
    if (match.IsContextualSearchSuggestion() && match.HasLensSearchAction()) {
      match.takeover_action =
          base::MakeRefCounted<ContextualSearchOpenLensAction>();
    }
  }
}

void AutocompleteResult::ConvertOpenTabMatches(
    AutocompleteProviderClient* client,
    const AutocompleteInput* input) {
  base::TimeTicks start_time = base::TimeTicks::Now();

  // URL matching on Android is expensive, because it triggers a volume of JNI
  // calls. We improve this situation by batching the lookup.
  TabMatcher::GURLToTabInfoMap batch_lookup_map;
  for (auto& match : matches_) {
    // If already converted this match, don't re-search through open tabs and
    // possibly re-change the description.
    // Note: explicitly check for value rather than deferring to implicit
    // boolean conversion of std::optional.
    if (match.has_tab_match.has_value() ||
        match.type == AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER) {
      continue;
    }
    batch_lookup_map.insert({match.destination_url, {}});
  }

  if (!batch_lookup_map.empty()) {
    client->GetTabMatcher().FindMatchingTabs(&batch_lookup_map, input);

    for (auto& match : matches_) {
      if (match.has_tab_match.has_value() ||
          match.type == AutocompleteMatchType::HISTORY_EMBEDDINGS_ANSWER) {
        continue;
      }

      auto tab_info = batch_lookup_map.find(match.destination_url);
      // DCHECK ok as loop is exited if tab_info at .end().
      DCHECK(tab_info != batch_lookup_map.end());
      if (tab_info == batch_lookup_map.end()) {
        continue;
      }

      match.has_tab_match = tab_info->second.has_matching_tab;
      // Do not attach the action for iOS since they have separate UI treatment
      // for tab matches (no button row as on desktop and realbox).
      if (!is_ios && match.has_tab_match.value()) {
        // The default action for suggestions from the open tab provider in
        // keyword mode is to switch to the open tab so no button is necessary.
        if (!match.from_keyword ||
            match.provider->type() != AutocompleteProvider::TYPE_OPEN_TAB) {
          if constexpr (is_android) {
            // On Android, attach the action as ActionInSuggest that will be
            // interpreted as either action button or chip per the form factor.
            omnibox::SuggestTemplateInfo::TemplateAction template_action;
            template_action.set_action_type(
                omnibox::
                    SuggestTemplateInfo_TemplateAction_ActionType_CHROME_TAB_SWITCH);
            template_action.set_action_uri(match.destination_url.spec());
            auto action_in_suggest =
                base::MakeRefCounted<OmniboxActionInSuggest>(
                    std::move(template_action), std::nullopt);
#if BUILDFLAG(IS_ANDROID)
            action_in_suggest->tab_id = tab_info->second.android_tab_id;
#endif
            match.actions.push_back(action_in_suggest);
          } else {
            match.actions.push_back(
                base::MakeRefCounted<TabSwitchAction>(match.destination_url));
          }
        }
      }
    }
  }

  base::TimeDelta time_delta = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES("Omnibox.TabMatchTime", time_delta,
                                          base::Microseconds(1),
                                          base::Milliseconds(5), 50);
}

bool AutocompleteResult::HasCopiedMatches() const {
  for (const auto& i : *this) {
    if (i.from_previous)
      return true;
  }
  return false;
}

size_t AutocompleteResult::size() const {
  return matches_.size();
}

bool AutocompleteResult::empty() const {
  return matches_.empty();
}

AutocompleteResult::const_iterator AutocompleteResult::begin() const {
  return matches_.begin();
}

AutocompleteResult::iterator AutocompleteResult::begin() {
  return matches_.begin();
}

AutocompleteResult::const_iterator AutocompleteResult::end() const {
  return matches_.end();
}

AutocompleteResult::iterator AutocompleteResult::end() {
  return matches_.end();
}

const AutocompleteMatch& AutocompleteResult::match_at(size_t index) const {
  DCHECK_LT(index, matches_.size());
  return matches_[index];
}

AutocompleteMatch* AutocompleteResult::match_at(size_t index) {
  DCHECK_LT(index, matches_.size());
  return &matches_[index];
}

const AutocompleteMatch* AutocompleteResult::default_match() const {
  if (begin() != end() && begin()->allowed_to_be_default_match)
    return &(*begin());

  return nullptr;
}

// static
ACMatches::const_iterator AutocompleteResult::FindTopMatch(
    const AutocompleteInput& input,
    const ACMatches& matches) {
  return FindTopMatch(input, const_cast<ACMatches*>(&matches));
}

// static
ACMatches::iterator AutocompleteResult::FindTopMatch(
    const AutocompleteInput& input,
    ACMatches* matches) {
  // The matches may be sorted by type-demoted relevance. We want to choose the
  // highest-relevance, allowed-to-be-default match while ignoring type demotion
  // in order to explicitly find the highest relevance match rather than just
  // accepting the first allowed-to-be-default match in the list.
  // The goal of this behavior is to ensure that in situations where the user
  // expects to see a commonly visited URL as the default match, the URL is not
  // suppressed by type demotion.
  // However, we don't care about this URL behavior when the user is using the
  // fakebox/realbox, which is intended to work more like a search-only box.
  // Unless the user's input is a URL in which case we still want to ensure they
  // can get a URL as the default match.
  omnibox::CheckObsoletePageClass(input.current_page_classification());

  if (input.current_page_classification() != OmniboxEventProto::NTP_REALBOX ||
      input.type() == metrics::OmniboxInputType::URL) {
    auto best = matches->end();
    for (auto it = matches->begin(); it != matches->end(); ++it) {
      if (it->allowed_to_be_default_match &&
          (best == matches->end() ||
           AutocompleteMatch::MoreRelevant(*it, *best))) {
        best = it;
      }
    }
    return best;
  }
  return std::ranges::find_if(*matches,
                              &AutocompleteMatch::allowed_to_be_default_match);
}

// static
bool AutocompleteResult::UndedupTopSearchEntityMatch(ACMatches* matches) {
  if (matches->empty())
    return false;

  auto top_match = matches->begin();
  if (top_match->type != ACMatchType::SEARCH_SUGGEST_ENTITY)
    return false;

  // We define an iterator to capture the non-entity duplicate match (if any)
  // so that we can later use it with duplicate_matches.erase().
  auto non_entity_it = top_match->duplicate_matches.end();

  // Search the duplicates for an equivalent non-entity search suggestion.
  for (auto it = top_match->duplicate_matches.begin();
       it != top_match->duplicate_matches.end(); ++it) {
    // Reject any ineligible duplicates.
    if (it->type == ACMatchType::SEARCH_SUGGEST_ENTITY ||
        !AutocompleteMatch::IsSearchType(it->type) ||
        !it->allowed_to_be_default_match) {
      continue;
    }

    // Capture the first eligible non-entity duplicate we find, but continue the
    // search for a potential server-provided duplicate, which is considered to
    // be an even better candidate for the reasons outlined below.
    if (non_entity_it == top_match->duplicate_matches.end()) {
      non_entity_it = it;
    }

    // When an entity suggestion (SEARCH_SUGGEST_ENTITY) is received from
    // google.com, we also receive a non-entity version of the same suggestion
    // which (a) gets placed in the |duplicate_matches| list of the entity
    // suggestion (as part of the deduplication process) and (b) has the same
    // |deletion_url| as the entity suggestion.
    // When the user attempts to remove the SEARCH_SUGGEST_ENTITY suggestion
    // from the omnibox, the suggestion removal code will fire off network
    // requests to the suggestion's own |deletion_url| as well as to any
    // deletion_url's present on matches in the associated |duplicate_matches|
    // list, which in this case would result in redundant network calls to the
    // same URL.
    // By prioritizing the "undeduping" (i.e. moving a duplicate match out of
    // the |duplicate_matches| list) and promotion of the non-entity
    // SEARCH_SUGGEST (or any other "specialized search") duplicate as the
    // top match, we are deliberately separating the two matches that have the
    // same |deletion_url|, thereby eliminating any redundant network calls
    // upon suggestion removal.
    if (it->type == ACMatchType::SEARCH_SUGGEST ||
        AutocompleteMatch::IsSpecializedSearchType(it->type)) {
      non_entity_it = it;
      break;
    }
  }

  if (non_entity_it != top_match->duplicate_matches.end()) {
    // Move out the non-entity match, then erase it from the list of duplicates.
    // We do this first, because the insertion operation invalidates all
    // iterators, including |top_match|.
    AutocompleteMatch non_entity_match_copy{std::move(*non_entity_it)};
    top_match->duplicate_matches.erase(non_entity_it);

    // When we spawn our non-entity match copy, we still want to preserve any
    // entity ID that was provided by the server for logging purposes, even if
    // we don't display it.
    if (non_entity_match_copy.entity_id.empty()) {
      non_entity_match_copy.entity_id = top_match->entity_id;
    }

    // Unless the entity match has Actions in Suggest, promote the non-entity
    // match to the top. Otherwise keep the entity match at the top followed by
    // the non-entity match.
    bool top_match_has_actions =
        !!top_match->GetActionWhere([](const auto& action) {
          return action->ActionId() == OmniboxActionId::ACTION_IN_SUGGEST;
        });

    if (top_match_has_actions) {
      matches->insert(std::next(matches->begin()),
                      std::move(non_entity_match_copy));
    } else {
      matches->insert(matches->begin(), std::move(non_entity_match_copy));
    }
    // Immediately return as all our iterators are invalid after the insertion.
    return true;
  }

  return false;
}

// static
size_t AutocompleteResult::CalculateNumMatches(
    bool is_zero_suggest,
    AutocompleteInput::FeaturedKeywordMode featured_keyword_mode,
    const ACMatches& matches,
    const CompareWithDemoteByType<AutocompleteMatch>& comparing_object) {
  // Use alternative CalculateNumMatchesPerUrlCount if applicable.
  if (!is_zero_suggest &&
      featured_keyword_mode != AutocompleteInput::FeaturedKeywordMode::kExact &&
      base::FeatureList::IsEnabled(omnibox::kDynamicMaxAutocomplete)) {
    return CalculateNumMatchesPerUrlCount(matches, comparing_object);
  }

  // In the process of trimming, drop all matches with a demoted relevance
  // score of 0.
  size_t max_matches_by_policy =
      GetMaxMatches(is_zero_suggest, featured_keyword_mode);
  size_t num_matches = 0;
  while (num_matches < matches.size() &&
         comparing_object.GetDemotedRelevance(matches[num_matches]) > 0) {
    // Don't increment if at loose limit.
    if (num_matches >= max_matches_by_policy)
      break;
    ++num_matches;
  }
  return num_matches;
}

// static
size_t AutocompleteResult::CalculateNumMatchesPerUrlCount(
    const ACMatches& matches,
    const CompareWithDemoteByType<AutocompleteMatch>& comparing_object) {
  size_t base_limit = GetMaxMatches();
  size_t increased_limit = GetDynamicMaxMatches();
  size_t url_cutoff = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDynamicMaxAutocomplete,
      OmniboxFieldTrial::kDynamicMaxAutocompleteUrlCutoffParam, 0);
  DCHECK(increased_limit >= base_limit);

  size_t num_matches = 0;
  size_t num_url_matches = 0;
  size_t num_unscoped_extension_matches = 0;
  for (const auto& match : matches) {
    // Matches scored less than 0 won't be shown anyways, so we can break early.
    if (comparing_object.GetDemotedRelevance(matches[num_matches]) <= 0)
      break;
    // Skip unscoped extension provider matches. This match limit will be
    // adjusted to include these matches.
    if (match.provider && match.provider->type() ==
                              AutocompleteProvider::TYPE_UNSCOPED_EXTENSION) {
      num_unscoped_extension_matches++;
      continue;
    }
    if (!AutocompleteMatch::IsSearchType(match.type))
      num_url_matches++;
    size_t limit = num_url_matches <= url_cutoff ? increased_limit : base_limit;
    if (num_matches >= limit)
      break;
    num_matches++;
  }

  return num_matches + num_unscoped_extension_matches;
}

void AutocompleteResult::Reset() {
  session_ = {};
  ClearMatches();
}

void AutocompleteResult::ClearMatches() {
  matches_.clear();
  suggestion_groups_map_.clear();
  smart_compose_inline_hint_.clear();
#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
#endif
}

void AutocompleteResult::SwapMatchesWith(AutocompleteResult* other) {
  matches_.swap(other->matches_);
  suggestion_groups_map_.swap(other->suggestion_groups_map_);
  smart_compose_inline_hint_.swap(other->smart_compose_inline_hint_);

#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
  other->DestroyJavaObject();
#endif
}

void AutocompleteResult::CopyMatchesFrom(const AutocompleteResult& other) {
  if (this == &other)
    return;

  matches_ = other.matches_;
  suggestion_groups_map_ = other.suggestion_groups_map_;
  smart_compose_inline_hint_ = other.smart_compose_inline_hint_;

#if BUILDFLAG(IS_ANDROID)
  DestroyJavaObject();
#endif
}

#if DCHECK_IS_ON()
void AutocompleteResult::Validate() const {
  for (const auto& i : *this)
    i.Validate();
}
#endif  // DCHECK_IS_ON()

// static
GURL AutocompleteResult::ComputeAlternateNavUrl(
    const AutocompleteInput& input,
    const AutocompleteMatch& match,
    AutocompleteProviderClient* provider_client) {
  auto redirector_policy =
      omnibox::GetInterceptionChecksBehavior(provider_client->GetLocalState());

  bool policy_allows_alternate_navs =
      (redirector_policy == omnibox::IntranetRedirectorBehavior::
                                DISABLE_INTERCEPTION_CHECKS_ENABLE_INFOBARS ||
       redirector_policy == omnibox::IntranetRedirectorBehavior::
                                ENABLE_INTERCEPTION_CHECKS_AND_INFOBARS);
  TRACE_EVENT_INSTANT("omnibox", "AutocompleteResult::ComputeAlternateNavURL",
                      "input", input, "match", match,
                      "policy_allows_alternate_navs",
                      policy_allows_alternate_navs);
  if (!policy_allows_alternate_navs)
    return GURL();

  return ((input.type() == metrics::OmniboxInputType::UNKNOWN) &&
          (AutocompleteMatch::IsSearchType(match.type)) &&
          !ui::PageTransitionCoreTypeIs(match.transition,
                                        ui::PAGE_TRANSITION_KEYWORD) &&
          (input.canonicalized_url() != match.destination_url))
             ? input.canonicalized_url()
             : GURL();
}

// static
void AutocompleteResult::DeduplicateMatches(
    ACMatches* matches,
    const AutocompleteInput& input,
    TemplateURLService* template_url_service) {
  for (auto& match : *matches) {
    match.ComputeStrippedDestinationURL(input, template_url_service);
  }

  // Group matches by stripped URL and `AutocompleteMatchDedupeType`.
  std::unordered_map<AutocompleteResult::MatchDedupComparator,
                     std::vector<ACMatches::iterator>,
                     ACMatchKeyHash<std::string, AutocompleteMatchDedupeType>>
      url_to_matches;
  for (auto i = matches->begin(); i != matches->end(); ++i) {
    url_to_matches[GetMatchComparisonFields(*i)].push_back(i);
  }

  // For each group of duplicate matches, choose the one that's considered best.
  for (auto& group : url_to_matches) {
    const auto& key = group.first;

    // The vector of matches whose URL are equivalent.
    std::vector<ACMatches::iterator>& duplicate_matches = group.second;
    if (std::get<0>(key).empty() || duplicate_matches.size() == 1) {
      continue;
    }

    // Sort the matches best to worst, according to the deduplication criteria.
    std::sort(duplicate_matches.begin(), duplicate_matches.end(),
              &AutocompleteMatch::BetterDuplicateByIterator);
    AutocompleteMatch& best_match = **duplicate_matches.begin();

    // Process all the duplicate matches (from second-best to worst).
    std::vector<AutocompleteMatch> duplicates_of_duplicates;
    for (auto i = std::next(duplicate_matches.begin());
         i != duplicate_matches.end(); ++i) {
      AutocompleteMatch& duplicate_match = **i;

      // Each duplicate match may also have its own duplicates. Move those to
      // a temporary list, which will be eventually added to the end of
      // |best_match.duplicate_matches|. Clear out the original list too.
      std::move(duplicate_match.duplicate_matches.begin(),
                duplicate_match.duplicate_matches.end(),
                std::back_inserter(duplicates_of_duplicates));
      duplicate_match.duplicate_matches.clear();

      best_match.UpgradeMatchWithPropertiesFrom(duplicate_match);

      // This should be a copy, not a move, since we don't erase duplicate
      // matches from the source list until the very end.
      DCHECK(duplicate_match.duplicate_matches.empty());  // Should be cleared.
      best_match.duplicate_matches.push_back(duplicate_match);
    }
    std::move(duplicates_of_duplicates.begin(), duplicates_of_duplicates.end(),
              std::back_inserter(best_match.duplicate_matches));
  }

  // Erase duplicate matches.
  std::erase_if(*matches, [&url_to_matches](const AutocompleteMatch& match) {
    auto match_comparison_fields = GetMatchComparisonFields(match);
    return !match.stripped_destination_url.is_empty() &&
           &(*url_to_matches[match_comparison_fields].front()) != &match;
  });
}

std::u16string AutocompleteResult::GetCommonPrefix() {
  std::u16string common_prefix;

  for (const auto& match : matches_) {
    if (match.type == ACMatchType::SEARCH_SUGGEST_TAIL) {
      int common_length;
      // TODO (manukh): `GetAdditionalInfoForDebugging()` shouldn't be used for
      //   non-debugging purposes.
      base::StringToInt(match.GetAdditionalInfoForDebugging(
                            kACMatchPropertyContentsStartIndex),
                        &common_length);
      common_prefix = base::UTF8ToUTF16(match.GetAdditionalInfoForDebugging(
                                            kACMatchPropertySuggestionText))
                          .substr(0, common_length);
      break;
    }
  }
  return common_prefix;
}

size_t AutocompleteResult::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(matches_);
}

std::vector<AutocompleteResult::MatchDedupComparator>
AutocompleteResult::GetMatchDedupComparators() const {
  std::vector<AutocompleteResult::MatchDedupComparator> comparators;
  for (const auto& match : *this)
    comparators.push_back(GetMatchComparisonFields(match));
  return comparators;
}

std::u16string AutocompleteResult::GetHeaderForSuggestionGroup(
    omnibox::GroupId suggestion_group_id) const {
  auto it = suggestion_groups_map().find(suggestion_group_id);
  if (it == suggestion_groups_map().end()) {
    return u"";
  }
  return base::UTF8ToUTF16(it->second.header_text());
}

omnibox::GroupSection AutocompleteResult::GetSectionForSuggestionGroup(
    omnibox::GroupId suggestion_group_id) const {
  auto it = suggestion_groups_map().find(suggestion_group_id);
  if (it == suggestion_groups_map().end()) {
    return omnibox::SECTION_DEFAULT;
  }

  return it->second.section();
}

omnibox::GroupConfig_SideType AutocompleteResult::GetSideTypeForSuggestionGroup(
    omnibox::GroupId suggestion_group_id) const {
  auto it = suggestion_groups_map().find(suggestion_group_id);
  if (it == suggestion_groups_map().end()) {
    return omnibox::GroupConfig_SideType_DEFAULT_PRIMARY;
  }

  return it->second.side_type();
}

omnibox::GroupConfig_RenderType
AutocompleteResult::GetRenderTypeForSuggestionGroup(
    omnibox::GroupId suggestion_group_id) const {
  auto it = suggestion_groups_map().find(suggestion_group_id);
  if (it == suggestion_groups_map().end()) {
    return omnibox::GroupConfig_RenderType_DEFAULT_VERTICAL;
  }

  return it->second.render_type();
}

void AutocompleteResult::MergeSuggestionGroupsMap(
    const omnibox::GroupConfigMap& suggestion_groups_map) {
  for (const auto& entry : suggestion_groups_map) {
    suggestion_groups_map_[entry.first].MergeFrom(entry.second);
  }
}

// static
bool AutocompleteResult::HasMatchByDestination(const AutocompleteMatch& match,
                                               const ACMatches& matches) {
  for (const auto& m : matches) {
    if (m.destination_url == match.destination_url)
      return true;
  }
  return false;
}

// static
void AutocompleteResult::MaybeCullTailSuggestions(
    ACMatches* matches,
    const CompareWithDemoteByType<AutocompleteMatch>& comparing_object) {
  std::function<bool(const AutocompleteMatch&)> is_tail =
      [](const AutocompleteMatch& match) {
        return match.type == ACMatchType::SEARCH_SUGGEST_TAIL;
      };
  std::function<bool(const AutocompleteMatch&)> is_history_cluster =
      [&](const AutocompleteMatch& match) {
        return match.type == ACMatchType::HISTORY_CLUSTER;
      };
  // 'normal' refers to a suggestion that is neither a tail nor history cluster.
  bool default_normal = false;
  bool other_normals = false;
  bool any_normals = false;
  bool default_tail = false;
  bool any_tails = false;
  bool any_history_clusters = false;
  for (const auto& match : *matches) {
    if (comparing_object.GetDemotedRelevance(match) == 0)
      continue;
    if (is_tail(match)) {
      any_tails = true;
      if (!default_tail && match.allowed_to_be_default_match)
        default_tail = true;
    } else if (is_history_cluster(match)) {
      any_history_clusters = true;
    } else {
      any_normals = true;
      if (!default_normal && match.allowed_to_be_default_match)
        default_normal = true;
      else
        other_normals = true;
    }
  }

  // If there are only non-tail or only tail suggestions, then cull none.
  if (!any_normals || !any_tails)
    return;

  // Cull non-tail suggestions when the default is a tail suggestion.
  if (!default_normal && default_tail) {
    std::erase_if(*matches, std::not_fn(is_tail));
    return;
  }

  // Cull tail suggestions when there is a non-tail, non-default suggestion.
  if (other_normals) {
    std::erase_if(*matches, is_tail);
    return;
  }

  // If showing tail suggestions, hide history cluster suggestions.
  if (any_history_clusters)
    std::erase_if(*matches, is_history_cluster);

  // If showing tail suggestions with a default non-tail, make sure the tail
  // suggestions are not defaulted.
  if (default_tail) {
    DCHECK(default_normal);
    for (auto& match : *matches) {
      if (is_tail(match))
        match.allowed_to_be_default_match = false;
    }
  }
}

void AutocompleteResult::BuildProviderToMatchesCopy(
    ProviderToMatches* provider_to_matches) const {
  for (const auto& match : *this)
    (*provider_to_matches)[match.provider].push_back(match);
}

void AutocompleteResult::BuildProviderToMatchesMove(
    ProviderToMatches* provider_to_matches) {
  for (auto& match : *this)
    (*provider_to_matches)[match.provider].push_back(std::move(match));
}

void AutocompleteResult::MergeMatchesByProvider(ACMatches* old_matches,
                                                const ACMatches& new_matches) {
  if (new_matches.size() >= old_matches->size())
    return;

  // Prevent old matches from this provider from outranking new ones and
  // becoming the default match by capping old matches' scores to be less than
  // the highest-scoring allowed-to-be-default match from this provider.
  auto i = std::ranges::find_if(
      new_matches, &AutocompleteMatch::allowed_to_be_default_match);

  // If the provider doesn't have any matches that are allowed-to-be-default,
  // cap scores below the global allowed-to-be-default match.
  // AutocompleteResult maintains the invariant that the first item in
  // |matches_| is always such a match.
  if (i == new_matches.end())
    i = matches_.begin();

  const int max_relevance = i->relevance - 1;

  // Because the goal is a visibly-stable popup, rather than one that preserves
  // the highest-relevance matches, we copy in the lowest-relevance matches
  // first. This means that within each provider's "group" of matches, any
  // synchronous matches (which tend to have the highest scores) will
  // "overwrite" the initial matches from that provider's previous results,
  // minimally disturbing the rest of the matches.
  size_t delta = old_matches->size() - new_matches.size();
  for (const AutocompleteMatch& old_match : base::Reversed(*old_matches)) {
    if (delta == 0) {
      break;
    }

    if (!HasMatchByDestination(old_match, new_matches)) {
      matches_.push_back(std::move(old_match));
      matches_.back().relevance =
          std::min(max_relevance, matches_.back().relevance);
      matches_.back().from_previous = true;
      delta--;
    }
  }
}

AutocompleteResult::MatchDedupComparator
AutocompleteResult::GetMatchComparisonFields(const AutocompleteMatch& match) {
  AutocompleteMatchDedupeType type;
  if (match.provider != nullptr &&
      match.provider->type() == AutocompleteProvider::TYPE_VERBATIM_MATCH) {
    type = AutocompleteMatchDedupeType::kVerbatimProvider;
  } else if (match.type == ACMatchType::CALCULATOR) {
    type = AutocompleteMatchDedupeType::kCalculator;
  } else if (match.IsSearchAimSuggestion() &&
             omnibox_feature_configs::AiMode::Get()
                 .do_not_dedupe_aim_suggestions) {
    type = AutocompleteMatchDedupeType::kAiMode;
  } else {
    type = AutocompleteMatchDedupeType::kNormal;
  }

  return std::make_tuple(match.stripped_destination_url.spec(), type);
}

void AutocompleteResult::LimitNumberOfURLsShown(
    size_t max_matches,
    const CompareWithDemoteByType<AutocompleteMatch>& comparing_object) {
  size_t search_count =
      std::ranges::count_if(matches_, [&](const AutocompleteMatch& m) {
        return AutocompleteMatch::IsSearchType(m.type) &&
               // Don't count if would be removed.
               comparing_object.GetDemotedRelevance(m) > 0;
      });
  // Display more than `max_url_matches_` if there are no non-URL suggestions
  // to replace them. Avoid signed math.

  size_t max_url_count = max_url_matches_;
  if (max_matches > search_count && max_matches - search_count > max_url_count)
    max_url_count = max_matches - search_count;
  size_t url_count = 0;
  // Erase URL suggestions past the count of allowed ones, or anything past
  // maximum.
  std::erase_if(matches_,
                [&url_count, max_url_count](const AutocompleteMatch& m) {
                  return !AutocompleteMatch::IsSearchType(m.type) &&
                         ++url_count > max_url_count;
                });
}

// static
void AutocompleteResult::GroupSuggestionsBySearchVsURL(iterator begin,
                                                       iterator end) {
  while (begin != end &&
         AutocompleteMatch::ShouldBeSkippedForGroupBySearchVsUrl(begin->type)) {
    ++begin;
  }

  if (begin == end)
    return;

  std::ranges::stable_sort(begin, end, {},
                           [](const auto& m) { return m.GetSortingOrder(); });
}

std::ostream& operator<<(std::ostream& os, const AutocompleteResult& result) {
  os << "AutocompleteResult {" << std::endl;
  for (size_t i = 0; i < result.matches_.size(); i++) {
    const AutocompleteMatch& match = result.matches_[i];
    os << "  - " << i << ": `" << match.contents << "`"
       << (match.allowed_to_be_default_match ? '*' : ' ');
    if (match.suggestion_group_id) {
      os << " group=" << match.suggestion_group_id.value();
    }
    if (!match.subtypes.empty()) {
      os << " subtypes=";
      for (auto subtype : match.subtypes) {
        os << subtype << ',';
      }
    }
    os << std::endl;
  }
  os << "}" << std::endl;
  return os;
}
