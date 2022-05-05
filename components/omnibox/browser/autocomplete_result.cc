// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_set>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/omnibox_pedal_provider.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/base_search_provider.h"
#include "components/omnibox/browser/intranet_redirector_state.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/l10n/l10n_util.h"

using metrics::OmniboxEventProto;

typedef AutocompleteMatchType ACMatchType;

namespace {

// Rotates |it| to be in the front of |matches|.
// |it| must be a valid iterator of |matches| or equal to |matches->end()|.
void RotateMatchToFront(ACMatches::iterator it, ACMatches* matches) {
  if (it == matches->end())
    return;

  auto next = std::next(it);
  std::rotate(matches->begin(), it, next);
}

#if BUILDFLAG(IS_IOS)
// Maximum number of pedals to show.
// On iOS, the UI for pedals gets too visually cluttered with too many pedals.
constexpr size_t kMaxPedalCount = 1;
// Maximum index of a match in a result for which the pedal should be displayed.
// On iOS, the UI for pedals gets too visually cluttered with too many pedals.
constexpr size_t kMaxPedalMatchIndex = 3;
#else
constexpr size_t kMaxPedalCount = std::numeric_limits<size_t>::max();
constexpr size_t kMaxPedalMatchIndex = std::numeric_limits<size_t>::max();
#endif

}  // namespace

struct MatchGURLHash {
  // The |bool| is whether the match is a calculator suggestion. We want them
  // compare differently against other matches with the same URL.
  size_t operator()(const std::pair<GURL, bool>& p) const {
    return std::hash<std::string>()(p.first.spec()) + p.second;
  }
};

// static
size_t AutocompleteResult::GetMaxMatches(bool is_zero_suggest) {
#if BUILDFLAG(IS_ANDROID)
  constexpr size_t kDefaultMaxAutocompleteMatches = 8;
  constexpr size_t kDefaultMaxZeroSuggestMatches = 15;
#elif BUILDFLAG(IS_IOS)
  constexpr size_t kDefaultMaxAutocompleteMatches = 6;
  constexpr size_t kDefaultMaxZeroSuggestMatches = 6;
#else
  constexpr size_t kDefaultMaxAutocompleteMatches = 8;
  constexpr size_t kDefaultMaxZeroSuggestMatches = 8;
#endif
  static_assert(kMaxAutocompletePositionValue > kDefaultMaxAutocompleteMatches,
                "kMaxAutocompletePositionValue must be larger than the largest "
                "possible autocomplete result size.");
  static_assert(kMaxAutocompletePositionValue > kDefaultMaxZeroSuggestMatches,
                "kMaxAutocompletePositionValue must be larger than the largest "
                "possible zero suggest autocomplete result size.");
  static_assert(kDefaultMaxAutocompleteMatches != 0,
                "Default number of suggestions must be non-zero");
  static_assert(kDefaultMaxZeroSuggestMatches != 0,
                "Default number of zero-prefix suggestions must be non-zero");

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

  //  Otherwise, i.e. if no zero suggest specific limit has been specified or
  //  the input is not from omnibox focus, return the general max matches limit.
  size_t field_trial_value = base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam,
      kDefaultMaxAutocompleteMatches);
  DCHECK(kMaxAutocompletePositionValue > field_trial_value);
  return field_trial_value;
}

// static
size_t AutocompleteResult::GetDynamicMaxMatches() {
#if BUILDFLAG(IS_ANDROID)
  constexpr const int kDynamicMaxMatchesLimit = 15;
#else
  constexpr const int kDynamicMaxMatchesLimit = 10;
#endif
  if (!base::FeatureList::IsEnabled(omnibox::kDynamicMaxAutocomplete))
    return AutocompleteResult::GetMaxMatches();
  return base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kDynamicMaxAutocomplete,
      OmniboxFieldTrial::kDynamicMaxAutocompleteIncreasedLimitParam,
      kDynamicMaxMatchesLimit);
}

AutocompleteResult::AutocompleteResult() {
  // Reserve enough space for the maximum number of matches we'll show in either
  // on-focus or prefix-suggest mode.
  matches_.reserve(std::max(GetMaxMatches(), GetMaxMatches(true)));
}

AutocompleteResult::~AutocompleteResult() = default;

void AutocompleteResult::TransferOldMatches(
    const AutocompleteInput& input,
    AutocompleteResult* old_matches,
    TemplateURLService* template_url_service) {
  if (old_matches->empty())
    return;

  // Exclude specialized suggestion types from being transferred to prevent
  // user-visible artifacts.
  old_matches->matches_.erase(
      std::remove_if(
          old_matches->begin(), old_matches->end(),
          [](const auto& match) {
            return match.type == AutocompleteMatchType::TILE_NAVSUGGEST ||
                   match.type == AutocompleteMatchType::TILE_SUGGESTION;
          }),
      old_matches->matches_.end());

  if (empty()) {
    // If we've got no matches we can copy everything from the last result.
    Swap(old_matches);
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

  // Make sure previous matches adhere to |input.prevent_inline_autocomplete()|.
  // Previous matches are demoted in |MergeMatchesByProvider()| anyways, making
  // them unlikely to be default; however, without this safeguard, they may
  // still be deduped with a higher-relevance yet not-allowed-to-be-default
  // match later, resulting in a default match with autocompletion when
  // |prevent_inline_autocomplete| is false.
  for (auto& m : matches_) {
    if (input.prevent_inline_autocomplete() && m.from_previous)
      m.SetAllowedToBeDefault(input);
  }

  SortAndCull(input, template_url_service);
}

void AutocompleteResult::AppendMatches(const AutocompleteInput& input,
                                       const ACMatches& matches) {
  for (const auto& match : matches) {
    DCHECK_EQ(AutocompleteMatch::SanitizeString(match.contents),
              match.contents);
    DCHECK_EQ(AutocompleteMatch::SanitizeString(match.description),
              match.description);
    matches_.push_back(match);
    if (!match.description.empty() &&
        !AutocompleteMatch::IsSearchType(match.type) &&
        match.type != ACMatchType::DOCUMENT_SUGGESTION) {
      matches_.back().swap_contents_and_description = true;
    }
  }
}

void AutocompleteResult::SortAndCull(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service,
    const AutocompleteMatch* preserve_default_match) {
  for (auto& match : matches_)
    match.ComputeStrippedDestinationURL(input, template_url_service);

  DemoteOnDeviceSearchSuggestions();

  CompareWithDemoteByType<AutocompleteMatch> comparing_object(
      input.current_page_classification());

#if !(BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS))
  // Because tail suggestions are a "last resort", we cull the tail suggestions
  // if there are any non-default, non-tail suggestions.
  MaybeCullTailSuggestions(&matches_, comparing_object);
#endif

  DeduplicateMatches(&matches_);

  // Sort the matches by relevance and demotions.
  std::sort(matches_.begin(), matches_.end(), comparing_object);

  // Find the best match and rotate it to the front to become the default match.
  {
    ACMatches::iterator top_match = matches_.end();

    // If we are trying to keep a default match from a previous pass stable,
    // search the current results for it, and if found, make it the top match.
    if (preserve_default_match) {
      std::pair<GURL, bool> default_match_fields =
          GetMatchComparisonFields(*preserve_default_match);

      top_match = std::find_if(
          matches_.begin(), matches_.end(), [&](const AutocompleteMatch& m) {
            // Find a match that is a duplicate AND has the same fill_into_edit.
            return default_match_fields == GetMatchComparisonFields(m) &&
                   preserve_default_match->fill_into_edit == m.fill_into_edit;
          });
    }

    // Otherwise, if there's no default match from a previous pass to preserve,
    // find the top match based on our normal undemoted scoring method.
    if (top_match == matches_.end())
      top_match = FindTopMatch(input, &matches_);

    RotateMatchToFront(top_match, &matches_);

    DiscourageTopMatchFromBeingSearchEntity(&matches_);
  }

  // Limit URL matches per OmniboxMaxURLMatches.
  size_t max_url_count = 0;
  bool is_zero_suggest = input.focus_type() != OmniboxFocusType::DEFAULT;
  if (OmniboxFieldTrial::IsMaxURLMatchesFeatureEnabled() &&
      (max_url_count = OmniboxFieldTrial::GetMaxURLMatches()) != 0)
    LimitNumberOfURLsShown(GetMaxMatches(is_zero_suggest), max_url_count,
                           comparing_object);

  // Limit total matches accounting for suggestions score <= 0, sub matches, and
  // feature configs such as OmniboxUIExperimentMaxAutocompleteMatches,
  // OmniboxMaxZeroSuggestMatches, and OmniboxDynamicMaxAutocomplete.
  const size_t num_matches =
      CalculateNumMatches(is_zero_suggest, matches_, comparing_object);

  if (base::FeatureList::IsEnabled(omnibox::kRetainSuggestionsWithHeaders)) {
    size_t num_regular_suggestions = 0;
    base::EraseIf(matches_,
                  [&num_regular_suggestions, num_matches](const auto& match) {
                    // Trim suggestions without headers to the specified limit.
                    if (!match.suggestion_group_id.has_value()) {
                      num_regular_suggestions++;
                      return num_regular_suggestions > num_matches;
                    }
                    // Do not trim suggestions with headers.
                    return false;
                  });
  } else {
    matches_.resize(num_matches);
  }

  // Group search suggestions above URL suggestions.
#if BUILDFLAG(IS_ANDROID)
  if (matches_.size() > 2 &&
      !base::FeatureList::IsEnabled(omnibox::kAdaptiveSuggestionsCount)) {
#else
  if (matches_.size() > 2) {
#endif
    GroupSuggestionsBySearchVsURL(std::next(matches_.begin()), matches_.end());
  }

  // Grouping and Demoting Matches with Headers needs to be done only after
  // matches are grouped by Search and URL type to ensure that URLs don't sink
  // to the bottom of the suggestions list, and surface below the Matches with
  // headers.
  GroupAndDemoteMatchesWithHeaders();

  // If we have a default match, run some sanity checks. Skip these checks if
  // the default match has no |destination_url|. An example of this is the
  // default match after the user has tabbed into keyword search mode, but has
  // not typed a query yet.
  auto* default_match = this->default_match();
  if (default_match && default_match->destination_url.is_valid()) {
    const std::u16string debug_info =
        u"fill_into_edit=" + default_match->fill_into_edit + u", provider=" +
        ((default_match->provider != nullptr)
             ? base::ASCIIToUTF16(default_match->provider->GetName())
             : std::u16string()) +
        u", input=" + input.text();

    if (AutocompleteMatch::IsSearchType(default_match->type)) {
      // We shouldn't get query matches for URL inputs.
      DCHECK_NE(metrics::OmniboxInputType::URL, input.type()) << debug_info;
    } else {
      // If the user explicitly typed a scheme, the default match should
      // have the same scheme.
      if ((input.type() == metrics::OmniboxInputType::URL) &&
          input.parts().scheme.is_nonempty()) {
        const std::string& in_scheme = base::UTF16ToUTF8(input.scheme());
        const std::string& dest_scheme =
            default_match->destination_url.scheme();
        DCHECK(url_formatter::IsEquivalentScheme(in_scheme, dest_scheme))
            << debug_info;
      }
    }
  }
}

void AutocompleteResult::GroupAndDemoteMatchesWithHeaders() {
  constexpr int kNoHeaderSuggestionGroupId = -1;

  // Create a map from suggestion group ID to the index it first appears.
  // Reserve the first spot for matches without headers.
  std::map<int, int> group_id_index_map = {{kNoHeaderSuggestionGroupId, 0}};
  for (auto it = matches_.begin(); it != matches_.end(); ++it) {
    if (it->suggestion_group_id.has_value()) {
      // Record group IDs and header strings, if available, into the
      // additional_info field for chrome://omnibox.
      int group_id = it->suggestion_group_id.value();
      it->RecordAdditionalInfo("suggestion_group_id", group_id);
      const std::u16string header = GetHeaderForGroupId(group_id);
      if (!header.empty()) {
        it->RecordAdditionalInfo("header string", header);
      } else {
        // Strip group IDs for which there is no header string from the matches.
        // Otherwise, these matches may be shown at the bottom with an empty
        // header row. They should instead be treated as ordinary matches with
        // no group ID.
        it->suggestion_group_id.reset();
      }
    }

    int group_id = it->suggestion_group_id.value_or(kNoHeaderSuggestionGroupId);
    // Use the 1-based index of the match to record the first appearance of its
    // group ID since 0 is reserved for matches without headers. We are
    // interested in the relative values of these indices only and their
    // absolute values hardly matter.
    int index = std::distance(matches_.begin(), it) + 1;
    // map::insert doesn't insert the value if the map already contains the key.
    group_id_index_map.insert(std::pair<int, int>(group_id, index));
  }

  // No need to group and demote matches with headers if none exists.
  if (group_id_index_map.size() == 1)
    return;

  // Sort the matches based on the order in which their group IDs first appear
  // while preserving the existing order of matches with the same group ID.
  std::stable_sort(
      matches_.begin(), matches_.end(),
      [&group_id_index_map, kNoHeaderSuggestionGroupId](const auto& a,
                                                        const auto& b) {
        const int a_group_id =
            a.suggestion_group_id.value_or(kNoHeaderSuggestionGroupId);
        const int b_group_id =
            b.suggestion_group_id.value_or(kNoHeaderSuggestionGroupId);
        return group_id_index_map[a_group_id] < group_id_index_map[b_group_id];
      });
}

void AutocompleteResult::DemoteOnDeviceSearchSuggestions() {
  const std::string mode = OmniboxFieldTrial::OnDeviceHeadSuggestDemoteMode();
  if (mode != "decrease-relevances" && mode != "remove-suggestions")
    return;

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
      if (mode == "decrease-relevances") {
        search_provider_search_suggestion_min_relevance =
            search_provider_search_suggestion_min_relevance < 0
                ? m.relevance
                : std::min(search_provider_search_suggestion_min_relevance,
                           m.relevance);
      }
    } else if (m.IsOnDeviceSearchSuggestion()) {
      on_device_search_suggestions.push_back(&m);
      if (mode == "decrease-relevances") {
        on_device_search_suggestion_max_relevance =
            std::max(on_device_search_suggestion_max_relevance, m.relevance);
      }
    }
  }

  // If SearchProvider search suggestions present, adjust the relevances for
  // OnDeviceProvider search suggestions, determined by mode:
  // 1. decrease-relevances: if any OnDeviceProvider search suggestion has a
  //    higher relevance than any SearchProvider one, subtract the difference
  //    b/w the maximum OnDeviceProvider search suggestion relevance and the
  //    minimum SearchProvider search suggestion relevance from the relevances
  //    for all OnDeviceProvider ones.
  // 2. remove-suggestions: set the relevances to 0 for all OnDeviceProvider
  //    search suggestions.
  if (search_provider_search_suggestion_exists &&
      !on_device_search_suggestions.empty()) {
    if (mode == "decrease-relevances" &&
        on_device_search_suggestion_max_relevance >=
            search_provider_search_suggestion_min_relevance) {
      int relevance_offset =
          (on_device_search_suggestion_max_relevance -
           search_provider_search_suggestion_min_relevance + 1);
      for (auto* m : on_device_search_suggestions)
        m->relevance = m->relevance > relevance_offset
                           ? m->relevance - relevance_offset
                           : 0;
    } else if (mode == "remove-suggestions") {
      for (auto* m : on_device_search_suggestions)
        m->relevance = 0;
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

  provider->set_field_trial_triggered(false);

  const size_t max_index = std::min(kMaxPedalMatchIndex, matches_.size());

  for (size_t i = 0; i < max_index && pedals_found.size() < kMaxPedalCount;
       i++) {
    AutocompleteMatch& match = matches_[i];
    // Skip matches that have already detected their Pedal, and avoid attaching
    // to matches with types that don't mix well with Pedals (e.g. entities).
    if (match.action ||
        !AutocompleteMatch::IsActionCompatibleType(match.type)) {
      continue;
    }

    OmniboxPedal* const pedal =
        provider->FindReadyPedalMatch(input, match.contents);
    if (pedal) {
      const auto result = pedals_found.insert(pedal);
      if (result.second)
        match.action = pedal;
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
    // boolean conversion of absl::optional.
    if (match.has_tab_match.has_value())
      continue;
    batch_lookup_map.insert({match.destination_url, {}});
  }

  if (!batch_lookup_map.empty()) {
    client->GetTabMatcher().FindMatchingTabs(&batch_lookup_map, input);

    for (auto& match : matches_) {
      if (match.has_tab_match.has_value())
        continue;

      auto tab_info = batch_lookup_map.find(match.destination_url);
      DCHECK(tab_info != batch_lookup_map.end());
      if (tab_info == batch_lookup_map.end())
        continue;

      match.has_tab_match = tab_info->second.has_matching_tab;
#if BUILDFLAG(IS_ANDROID)
      match.UpdateMatchingJavaTab(tab_info->second.android_tab);
#endif
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
  if ((input.current_page_classification() !=
           OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS &&
       input.current_page_classification() != OmniboxEventProto::NTP_REALBOX) ||
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
  } else {
    return std::find_if(matches->begin(), matches->end(), [](const auto& m) {
      return m.allowed_to_be_default_match;
    });
  }
}

// static
void AutocompleteResult::DiscourageTopMatchFromBeingSearchEntity(
    ACMatches* matches) {
  if (matches->empty())
    return;

  auto top_match = matches->begin();
  if (top_match->type != ACMatchType::SEARCH_SUGGEST_ENTITY)
    return;

  // Search the duplicates for an equivalent non-entity search suggestion.
  for (auto it = top_match->duplicate_matches.begin();
       it != top_match->duplicate_matches.end(); ++it) {
    // Reject any ineligible duplicates.
    if (it->type == ACMatchType::SEARCH_SUGGEST_ENTITY ||
        !AutocompleteMatch::IsSearchType(it->type) ||
        !it->allowed_to_be_default_match) {
      continue;
    }

    // Copy the non-entity match, then erase it from the list of duplicates.
    // We do this first, because the insertion operation invalidates all
    // iterators, including |top_match|.
    AutocompleteMatch non_entity_match_copy = *it;
    top_match->duplicate_matches.erase(it);

    // Promote the non-entity match to the top, then immediately return, since
    // all our iterators are invalid after the insertion.
    matches->insert(matches->begin(), std::move(non_entity_match_copy));
    return;
  }
}

// static
size_t AutocompleteResult::CalculateNumMatches(
    bool is_zero_suggest,
    const ACMatches& matches,
    const CompareWithDemoteByType<AutocompleteMatch>& comparing_object) {
  // Use alternative CalculateNumMatchesPerUrlCount if applicable.
  if (!is_zero_suggest &&
      base::FeatureList::IsEnabled(omnibox::kDynamicMaxAutocomplete))
    return CalculateNumMatchesPerUrlCount(matches, comparing_object);
  // In the process of trimming, drop all matches with a demoted relevance
  // score of 0.
  size_t max_matches_by_policy = GetMaxMatches(is_zero_suggest);
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
  for (const auto& match : matches) {
    // Matches scored less than 0 won't be shown anyways, so we can break early.
    if (comparing_object.GetDemotedRelevance(matches[num_matches]) <= 0)
      break;
    if (!AutocompleteMatch::IsSearchType(match.type))
      num_url_matches++;
    size_t limit = num_url_matches <= url_cutoff ? increased_limit : base_limit;
    if (num_matches >= limit)
      break;
    num_matches++;
  }
  return num_matches;
}

void AutocompleteResult::Reset() {
  matches_.clear();
  headers_map_.clear();
  hidden_group_ids_.clear();
#if BUILDFLAG(IS_ANDROID)
  java_result_.Reset();
#endif
}

void AutocompleteResult::Swap(AutocompleteResult* other) {
  matches_.swap(other->matches_);
#if BUILDFLAG(IS_ANDROID)
  java_result_.Reset();
  other->java_result_.Reset();
#endif
}

void AutocompleteResult::CopyFrom(const AutocompleteResult& other) {
  if (this == &other)
    return;

  matches_ = other.matches_;
#if BUILDFLAG(IS_ANDROID)
  java_result_.Reset();
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
void AutocompleteResult::DeduplicateMatches(ACMatches* matches) {
  // Group matches by stripped URL and whether it's a calculator suggestion.
  std::unordered_map<std::pair<GURL, bool>, std::vector<ACMatches::iterator>,
                     MatchGURLHash>
      url_to_matches;
  for (auto i = matches->begin(); i != matches->end(); ++i) {
    std::pair<GURL, bool> p = GetMatchComparisonFields(*i);
    url_to_matches[p].push_back(i);
  }

  // For each group of duplicate matches, choose the one that's considered best.
  for (auto& group : url_to_matches) {
    const auto& key = group.first;
    const GURL& gurl = key.first;
    // The vector of matches whose URL are equivalent.
    std::vector<ACMatches::iterator>& duplicate_matches = group.second;
    if (gurl.is_empty() || duplicate_matches.size() == 1)
      continue;

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
  base::EraseIf(*matches, [&url_to_matches](const AutocompleteMatch& m) {
    std::pair<GURL, bool> p = GetMatchComparisonFields(m);
    return !m.stripped_destination_url.is_empty() &&
           &(*url_to_matches[p].front()) != &m;
  });
}

std::u16string AutocompleteResult::GetCommonPrefix() {
  std::u16string common_prefix;

  for (const auto& match : matches_) {
    if (match.type == ACMatchType::SEARCH_SUGGEST_TAIL) {
      int common_length;
      base::StringToInt(
          match.GetAdditionalInfo(kACMatchPropertyContentsStartIndex),
          &common_length);
      common_prefix = base::UTF8ToUTF16(match.GetAdditionalInfo(
                                            kACMatchPropertySuggestionText))
                          .substr(0, common_length);
      break;
    }
  }
  return common_prefix;
}

void AutocompleteResult::SetTailSuggestCommonPrefixes() {
  std::u16string common_prefix = GetCommonPrefix();

  if (!common_prefix.empty()) {
    for (auto& match : matches_)
      match.SetTailSuggestCommonPrefix(common_prefix);
  }
}

void AutocompleteResult::SetTailSuggestContentPrefixes() {
  std::u16string common_prefix = GetCommonPrefix();

  if (!common_prefix.empty()) {
    for (auto& match : matches_) {
      match.SetTailSuggestContentPrefix(common_prefix);
    }
  }
}

size_t AutocompleteResult::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(matches_);
}

std::vector<AutocompleteResult::MatchDedupComparator>
AutocompleteResult::GetMatchDedupComparators() const {
  std::vector<MatchDedupComparator> comparators;
  for (const auto& match : *this)
    comparators.push_back(AutocompleteResult::GetMatchComparisonFields(match));
  return comparators;
}

std::u16string AutocompleteResult::GetHeaderForGroupId(
    int suggestion_group_id) const {
  const auto& it = headers_map_.find(suggestion_group_id);
  if (it != headers_map_.end())
    return it->second;
  return std::u16string();
}

bool AutocompleteResult::IsSuggestionGroupIdHidden(
    PrefService* prefs,
    int suggestion_group_id) const {
  omnibox::SuggestionGroupVisibility user_preference =
      omnibox::GetUserPreferenceForSuggestionGroupVisibility(
          prefs, suggestion_group_id);

  if (user_preference == omnibox::SuggestionGroupVisibility::HIDDEN)
    return true;
  if (user_preference == omnibox::SuggestionGroupVisibility::SHOWN)
    return false;

  DCHECK_EQ(user_preference, omnibox::SuggestionGroupVisibility::DEFAULT);
  return base::Contains(hidden_group_ids_, suggestion_group_id);
}

void AutocompleteResult::MergeHeadersMap(
    const SearchSuggestionParser::HeadersMap& headers_map) {
  headers_map_.insert(headers_map.begin(), headers_map.end());
}

void AutocompleteResult::MergeHiddenGroupIds(
    const std::vector<int>& hidden_group_ids) {
  hidden_group_ids_.insert(hidden_group_ids.begin(), hidden_group_ids.end());
}
// static
void AutocompleteResult::LogUpdateMetrics(
    const std::vector<MatchDedupComparator>& old_result,
    const AutocompleteResult& new_result,
    bool in_start) {
  bool any_match_changed = false;

  for (size_t i = 0; i < old_result.size(); ++i) {
    // Log a change for changed or removed matches. Don't log for
    // matches appended to the bottom since that's less disruptive.
    if (i >= new_result.size() ||
        old_result[i] != GetMatchComparisonFields(new_result.match_at(i))) {
      if (in_start) {
        UMA_HISTOGRAM_EXACT_LINEAR(
            "Omnibox.CrossInputMatchStability.MatchChange", i,
            kMaxAutocompletePositionValue);
      } else {
        UMA_HISTOGRAM_EXACT_LINEAR("Omnibox.MatchStability.AsyncMatchChange2",
                                   i, kMaxAutocompletePositionValue);
      }
      any_match_changed = true;
    }
  }

  if (in_start) {
    UMA_HISTOGRAM_BOOLEAN(
        "Omnibox.CrossInputMatchStability.MatchChangedInAnyPosition",
        any_match_changed);
  } else {
    UMA_HISTOGRAM_BOOLEAN(
        "Omnibox.MatchStability.AsyncMatchChangedInAnyPosition",
        any_match_changed);
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
  // This function implements the following logic:
  // ('E' == 'There exists', '!E' == 'There does not exist')
  // 1) !E default non-tail and E tail default? remove non-tails
  // 2) !E any tails at all? do nothing
  // 3) E default non-tail and other non-tails? remove tails
  // 4) E default non-tail and no other non-tails? mark tails as non-default
  // 5) E non-default non-tails? remove non-tails
  std::function<bool(const AutocompleteMatch&)> is_tail =
      [](const AutocompleteMatch& match) {
        return match.type == ACMatchType::SEARCH_SUGGEST_TAIL;
      };
  auto default_non_tail = matches->end();
  auto default_tail = matches->end();
  bool other_non_tails = false, any_tails = false;
  for (auto i = matches->begin(); i != matches->end(); ++i) {
    if (comparing_object.GetDemotedRelevance(*i) == 0)
      continue;
    if (!is_tail(*i)) {
      // We allow one default non-tail match. For non-default matches,
      // don't consider if we'd remove them later.
      if (default_non_tail == matches->end() && i->allowed_to_be_default_match)
        default_non_tail = i;
      else
        other_non_tails = true;
    } else {
      any_tails = true;
      if (default_tail == matches->end() && i->allowed_to_be_default_match)
        default_tail = i;
    }
  }
  // If the only default matches are tail suggestions, let them remain and
  // instead remove the non-tail suggestions.  This is necessary because we do
  // not want to display tail suggestions mixed with other suggestions in the
  // dropdown below the first item (the default match).  In this case, we
  // cannot remove the tail suggestions because we'll be left without a legal
  // default match--the non-tail ones much go.  This situation though is
  // unlikely, as we normally would expect the search-what-you-typed suggestion
  // as a default match (and that's a non-tail suggestion).
  // 1) above.
  if (default_tail != matches->end() && default_non_tail == matches->end()) {
    base::EraseIf(*matches, std::not_fn(is_tail));
    return;
  }
  // 2) above.
  if (!any_tails)
    return;
  // If both tail and non-tail matches, remove tail. Note that this can
  // remove the highest rated suggestions.
  if (default_non_tail != matches->end()) {
    // 3) above.
    if (other_non_tails) {
      base::EraseIf(*matches, is_tail);
    } else {
      // 4) above.
      // We want the non-tail default match to be placed first. Mark tail
      // suggestions as not a legal default match, so that the default match
      // will be moved up explicitly.
      for (auto& match : *matches) {
        if (is_tail(match))
          match.allowed_to_be_default_match = false;
      }
    }
  } else if (other_non_tails && default_tail == matches->end()) {
    // 5) above.
    // If there are no defaults at all, but non-tail suggestions exist, remove
    // the tail suggestions.
    base::EraseIf(*matches, is_tail);
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
  auto i = std::find_if(
      new_matches.begin(), new_matches.end(),
      [](const AutocompleteMatch& m) { return m.allowed_to_be_default_match; });

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

std::pair<GURL, bool> AutocompleteResult::GetMatchComparisonFields(
    const AutocompleteMatch& match) {
  return std::make_pair(match.stripped_destination_url,
                        match.type == ACMatchType::CALCULATOR);
}

void AutocompleteResult::LimitNumberOfURLsShown(
    size_t max_matches,
    size_t max_url_count,
    const CompareWithDemoteByType<AutocompleteMatch>& comparing_object) {
  size_t search_count = std::count_if(
      matches_.begin(), matches_.end(), [&](const AutocompleteMatch& m) {
        return AutocompleteMatch::IsSearchType(m.type) &&
               // Don't count if would be removed.
               comparing_object.GetDemotedRelevance(m) > 0;
      });
  // Display more than GetMaxURLMatches() if there are no non-URL suggestions
  // to replace them. Avoid signed math.
  if (max_matches > search_count && max_matches - search_count > max_url_count)
    max_url_count = max_matches - search_count;
  size_t url_count = 0;
  // Erase URL suggestions past the count of allowed ones, or anything past
  // maximum.
  matches_.erase(
      std::remove_if(matches_.begin(), matches_.end(),
                     [&url_count, max_url_count](const AutocompleteMatch& m) {
                       if (!AutocompleteMatch::IsSearchType(m.type) &&
                           ++url_count > max_url_count)
                         return true;
                       return false;
                     }),
      matches_.end());
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

  std::stable_partition(begin, end, [](const AutocompleteMatch& match) {
    return AutocompleteMatch::IsSearchType(match.type);
  });
}
