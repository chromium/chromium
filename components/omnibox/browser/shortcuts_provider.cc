// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_provider.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_scoring_signals_annotator.h"
#include "components/omnibox/browser/history_cluster_provider.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_triggered_feature_service.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "third_party/metrics_proto/omnibox_scoring_signals.pb.h"
#include "third_party/omnibox_proto/groups.pb.h"

#if !BUILDFLAG(IS_IOS)
#include "components/history_clusters/core/config.h"  // nogncheck
#endif  // !BUILDFLAG(IS_IOS)

constexpr bool kIsDesktop = !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS);

namespace {

using ShortcutMatch = ShortcutsProvider::ShortcutMatch;
using ScoringSignals = ::metrics::OmniboxScoringSignals;

class DestinationURLEqualsURL {
 public:
  explicit DestinationURLEqualsURL(const GURL& url) : url_(url) {}
  bool operator()(const AutocompleteMatch& match) const {
    return match.destination_url == url_;
  }

 private:
  const GURL url_;
};

// Helpers for extracting aggregated factors from a vector of shortcuts.
const ShortcutsDatabase::Shortcut* ShortestShortcutText(
    std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return *base::ranges::min_element(shortcuts, {}, [](const auto* shortcut) {
    return shortcut->text.length();
  });
}

const ShortcutsDatabase::Shortcut* MostRecentShortcut(
    std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return *base::ranges::max_element(shortcuts, {}, [](const auto* shortcut) {
    return shortcut->last_access_time;
  });
}

int SumNumberOfHits(std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return std::accumulate(shortcuts.begin(), shortcuts.end(), 0,
                         [](int sum, const auto* shortcut) {
                           return sum + shortcut->number_of_hits;
                         });
}

const ShortcutsDatabase::Shortcut* ShortestShortcutContent(
    std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return *base::ranges::min_element(shortcuts, {}, [](const auto* shortcut) {
    return shortcut->match_core.contents.length();
  });
}

// Helper for `CreateScoredShortcutMatch()` to score shortcuts.
int CalculateScoreFromFactors(size_t typed_length,
                              size_t shortcut_text_length,
                              const base::Time& last_access_time,
                              int number_of_hits,
                              int max_relevance) {
  DCHECK_GT(typed_length, 0u);
  DCHECK_LE(typed_length, shortcut_text_length);
  // The initial score is based on how much of the shortcut the user has typed.
  // Due to appending 3 chars when updating shortcuts, and expanding the last
  // word when updating or creating shortcuts, the shortcut text can be longer
  // than the user's previous inputs (see
  // `ShortcutsBackend::AddOrUpdateShortcut()`). As an approximation, ignore 10
  // chars in the shortcut text. Shortcuts are often deduped with higher scoring
  // history suggestions anyway.
  const size_t adjustment = 10;
  const size_t adjusted_text_length =
      std::max(shortcut_text_length, typed_length + adjustment) - adjustment;
  // Using the square root of the typed fraction boosts the base score rapidly
  // as characters are typed, compared with simply using the typed fraction
  // directly. This makes sense since the first characters typed are much more
  // important for determining how likely it is a user wants a particular
  // shortcut than are the remaining continued characters.
  const double typed_fraction =
      sqrt(static_cast<double>(typed_length) / adjusted_text_length);

  // Decay score by half each week. Clamp to 0 in case time jumps backwards
  // (e.g. due to DST).
  const double halftime_numerator =
      std::max((base::Time::Now() - last_access_time) / base::Days(7), 0.);
  // Reduce the decay factor for more used shortcuts. Once used, decay at full
  // speed; otherwise, decay `n` times slower, where n increases by 0.2 for each
  // additional hit, up to a maximum of 5.
  const double halftime_denominator = std::min(.8 + number_of_hits * .2, 5.);
  const double halftime_decay =
      pow(.5, halftime_numerator / halftime_denominator);

  return base::ClampRound(typed_fraction * halftime_decay * max_relevance);
}

// Populate scoring signals from the shortcut match to ACMatch.
void PopulateScoringSignals(const ShortcutMatch& shortcut_match,
                            AutocompleteMatch* match) {
  match->scoring_signals = std::make_optional<ScoringSignals>();
  match->scoring_signals->set_shortcut_visit_count(
      shortcut_match.aggregate_number_of_hits);
  match->scoring_signals->set_shortest_shortcut_len(
      shortcut_match.shortest_text_length);
  match->scoring_signals->set_elapsed_time_last_shortcut_visit_sec(
      (base::Time::Now() - shortcut_match.most_recent_access_time).InSeconds());
  match->scoring_signals->set_length_of_url(
      match->destination_url.spec().length());

  // Populate history signals in case the shortcut isn't in the history
  // in-memory index or doesn't have a history entry (e.g. bookmark shortcuts
  // with expired history entries or built-in shortcuts).
  match->scoring_signals->set_typed_count(
      shortcut_match.aggregate_number_of_hits);
  match->scoring_signals->set_visit_count(
      shortcut_match.aggregate_number_of_hits);
  match->scoring_signals->set_elapsed_time_last_visit_secs(
      match->scoring_signals->elapsed_time_last_shortcut_visit_sec());
}

}  // namespace

const int ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance = 1199;

ShortcutsProvider::ShortcutMatch::ShortcutMatch(
    int relevance,
    int aggregate_number_of_hits,
    base::Time most_recent_access_time,
    size_t shortest_text_length,
    const GURL& stripped_destination_url,
    const ShortcutsDatabase::Shortcut* shortcut)
    : relevance(relevance),
      aggregate_number_of_hits(aggregate_number_of_hits),
      most_recent_access_time(most_recent_access_time),
      shortest_text_length(shortest_text_length),
      stripped_destination_url(stripped_destination_url),
      shortcut(shortcut),
      contents(shortcut->match_core.contents),
      type(shortcut->match_core.type) {
  DCHECK_GE(aggregate_number_of_hits, shortcut->number_of_hits);
}

ShortcutsProvider::ShortcutMatch::ShortcutMatch(const ShortcutMatch& other) =
    default;

ShortcutMatch& ShortcutsProvider::ShortcutMatch::operator=(
    const ShortcutMatch& other) = default;

ShortcutsProvider::ShortcutsProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_SHORTCUTS),
      client_(client),
      backend_(client_->GetShortcutsBackend()) {
  if (backend_) {
    backend_->AddObserver(this);
    if (backend_->initialized()) {
      initialized_ = true;
    }
  }
}

void ShortcutsProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ShortcutsProvider::Start");
  matches_.clear();

  if (input.IsZeroSuggest() ||
      input.type() == metrics::OmniboxInputType::EMPTY ||
      input.text().empty() || !initialized_) {
    return;
  }
  DoAutocomplete(input,
                 OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled());
}

void ShortcutsProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Copy the URL since deleting from |matches_| will invalidate |match|.
  GURL url(match.destination_url);
  DCHECK(url.is_valid());

  // When a user deletes a match, they probably mean for the URL to disappear
  // out of history entirely. So nuke all shortcuts that map to this URL.
  if (backend_) {  // Can be NULL in Incognito.
    backend_->DeleteShortcutsWithURL(url);
  }

  std::erase_if(matches_, DestinationURLEqualsURL(url));
  // NOTE: |match| is now dead!

  // Delete the match from the history DB. This will eventually result in a
  // second call to DeleteShortcutsWithURL(), which is harmless.
  history::HistoryService* const history_service = client_->GetHistoryService();
  DCHECK(history_service);
  history_service->DeleteURLs({url});
}

ShortcutsProvider::~ShortcutsProvider() {
  if (backend_) {
    backend_->RemoveObserver(this);
  }
}

void ShortcutsProvider::OnShortcutsLoaded() {
  initialized_ = true;
}

void ShortcutsProvider::DoAutocomplete(const AutocompleteInput& input,
                                       bool populate_scoring_signals) {
  if (!backend_) {
    return;
  }
  // Get the URLs from the shortcuts database with keys that partially or
  // completely match the input string.
  std::u16string lower_input(base::i18n::ToLower(input.text()));
  DCHECK(!lower_input.empty());

  int max_relevance = kShortcutsProviderDefaultMaxRelevance;
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  const std::u16string fixed_up_input(FixupUserInput(input).second);

  // Get the shortcuts from the database with keys that partially or completely
  // match the input string.
  std::vector<ShortcutMatch> shortcut_matches;
  // Track history cluster shortcuts separately, so they don't consume
  // `provider_max_matches_`.
  std::vector<ShortcutMatch> history_cluster_shortcut_matches;

  // Group the matching shortcuts by stripped `destination_url`, score them
  // together, and create a single `ShortcutMatch`.
  std::map<GURL, std::vector<const ShortcutsDatabase::Shortcut*>>
      shortcuts_by_url;
  for (auto it = FindFirstMatch(lower_input, backend_.get());
       it != backend_->shortcuts_map().end() &&
       base::StartsWith(it->first, lower_input, base::CompareCase::SENSITIVE);
       ++it) {
    const ShortcutsDatabase::Shortcut& shortcut = it->second;

    const GURL stripped_destination_url(AutocompleteMatch::GURLToStrippedGURL(
        shortcut.match_core.destination_url, input, template_url_service,
        shortcut.match_core.keyword,
        /*keep_search_intent_params=*/false, /*normalize_search_terms=*/
        base::FeatureList::IsEnabled(omnibox::kNormalizeSearchSuggestions)));
    shortcuts_by_url[stripped_destination_url].push_back(&shortcut);
  }

  if (!input.omit_asynchronous_matches()) {
    // Inputs like 'http' or 'chrome' might have a more than 100 shortcuts,
    // which precludes `UMA_HISTOGRAM_EXACT_LINEAR`.
    UMA_HISTOGRAM_COUNTS_1000(
        "Omnibox.Shortcuts.NumberOfUniqueShortcutsIterated",
        shortcuts_by_url.size());
  }

  for (const auto& [url, shortcuts] : shortcuts_by_url) {
    if (!input.omit_asynchronous_matches()) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "Omnibox.Shortcuts.NumberOfDuplicatesPerShortcutIterated",
          shortcuts.size(), 11);
    }

    ShortcutMatch shortcut_match = CreateScoredShortcutMatch(
        lower_input.length(), url, shortcuts, max_relevance);

    // Don't return shortcuts with zero relevance.
    if (shortcut_match.relevance == 0)
      continue;

    if (kIsDesktop &&
        AutocompleteMatch::IsFeaturedSearchType(shortcut_match.type)) {
      // Let FeaturedSearchProvider win for feature search shortcuts (e.g.
      // starter pack and feature site search created by policy); they should
      // not allow default or inline autocomplete for the keyword mode refresh.
      continue;
    }

    if (shortcut_match.shortcut->match_core.type ==
        AutocompleteMatch::Type::HISTORY_CLUSTER) {
      history_cluster_shortcut_matches.push_back(shortcut_match);
    } else {
      shortcut_matches.push_back(shortcut_match);
    }
  }

  if (!shortcut_matches.empty() &&
      omnibox_feature_configs::ShortcutBoosting::Get().enabled) {
    // The initial value of `max_relevance` doesn't matter, as long as its >=
    // all `shortcut_matches` relevances.
    if (omnibox_feature_configs::ShortcutBoosting::Get().enabled)
      max_relevance = INT_MAX;
    // Promote the shortcut with most hits to compete for the default slot.
    // Won't necessarily be the highest scoring shortcut, as scoring also
    // depends on visit times and input length. Therefore, has to be done before
    // the partial sort before to ensure the match isn't erased. The match may
    // be not-allowed-to-be-default, in which case, it'll be competing for top
    // slot in the URL grouped suggestions. This won't affect the scores of
    // other shortcuts, as they're already scored less than
    // `kShortcutsProviderDefaultMaxRelevance`.
    const auto best_match = base::ranges::max_element(
        shortcut_matches, {}, [](const auto& shortcut_match) {
          return shortcut_match.aggregate_number_of_hits;
        });
    int boost_score =
        AutocompleteMatch::IsSearchType(best_match->type)
            ? omnibox_feature_configs::ShortcutBoosting::Get().search_score
            : omnibox_feature_configs::ShortcutBoosting::Get().url_score;
    if (boost_score > best_match->relevance) {
      client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
          metrics::OmniboxEventProto_Feature_SHORTCUT_BOOST);
      if (!omnibox_feature_configs::ShortcutBoosting::Get().counterfactual) {
        best_match->relevance = boost_score;
      }
    }
  }

  // Find best matches.
  std::partial_sort(
      shortcut_matches.begin(),
      shortcut_matches.begin() +
          std::min(provider_max_matches_, shortcut_matches.size()),
      shortcut_matches.end(),
      [](const ShortcutMatch& elem1, const ShortcutMatch& elem2) {
        // Ensure a stable sort by sorting equal-relevance matches
        // alphabetically.
        return elem1.relevance == elem2.relevance
                   ? elem1.contents < elem2.contents
                   : elem1.relevance > elem2.relevance;
      });
  bool ignore_provider_limit =
      OmniboxFieldTrial::IsMlUrlScoringUnlimitedNumCandidatesEnabled();
  if (!ignore_provider_limit &&
      shortcut_matches.size() > provider_max_matches_) {
    shortcut_matches.erase(shortcut_matches.begin() + provider_max_matches_,
                           shortcut_matches.end());
  }

  // Create and initialize autocomplete matches from shortcut matches.
  matches_.reserve(shortcut_matches.size() +
                   history_cluster_shortcut_matches.size());
  base::ranges::transform(
      shortcut_matches, std::back_inserter(matches_),
      [&](const auto& shortcut_match) {
        // Guarantee that all relevance scores are decreasing (but do not assign
        // any scores below 1). Only do this for non-history cluster shortcuts.
        max_relevance = std::min(max_relevance, shortcut_match.relevance);
        int relevance = max_relevance;
        if (max_relevance > 1)
          --max_relevance;
        auto match = ShortcutMatchToACMatch(shortcut_match, relevance, input,
                                            fixed_up_input, lower_input);
        if (populate_scoring_signals && match.IsMlSignalLoggingEligible()) {
          PopulateScoringSignals(shortcut_match, &match);
        }
        return match;
      });

  ResizeMatches(provider_max_matches_, ignore_provider_limit);
  base::ranges::transform(
      history_cluster_shortcut_matches, std::back_inserter(matches_),
      [&](const auto& shortcut_match) {
        auto match =
            ShortcutMatchToACMatch(shortcut_match, shortcut_match.relevance,
                                   input, fixed_up_input, lower_input);
    // Guard this as `HistoryClusterProvider` doesn't exist on iOS.
    // Though this code will never run on iOS regardless.
#if !BUILDFLAG(IS_IOS)
        // `lower_input` is only what the user typed, e.g. "new y" instead of
        // "new york". Use `match.description`, which is the whole string.
        // This is a bit hacky, but accurately reflects how
        // `HistoryClusterProvider` constructed the original match.
        std::string matching_string = base::UTF16ToUTF8(match.description);
        // Shortcut-generated HC matches have empty `ClusterKeywordData()`s,
        // because it wasn't generated via an entity match in the first place.
        HistoryClusterProvider::CompleteHistoryClustersMatch(
            matching_string, history::ClusterKeywordData(), &match);
#endif  // !BUILDFLAG(IS_IOS)

        return match;
      });
}

ShortcutMatch ShortcutsProvider::CreateScoredShortcutMatch(
    size_t input_length,
    const GURL& stripped_destination_url,
    const std::vector<const ShortcutsDatabase::Shortcut*>& shortcuts,
    int max_relevance) {
  DCHECK_GT(shortcuts.size(), 0u);

  const int number_of_hits = SumNumberOfHits(shortcuts);
  const int number_of_hits_threshold =
      AutocompleteMatch::IsSearchType(shortcuts[0]->match_core.type)
          ? omnibox_feature_configs::ShortcutBoosting::Get()
                .non_top_hit_search_threshold
          : omnibox_feature_configs::ShortcutBoosting::Get()
                .non_top_hit_threshold;

  int boost_score = 0;
  if (number_of_hits_threshold && number_of_hits >= number_of_hits_threshold) {
    boost_score =
        AutocompleteMatch::IsSearchType(shortcuts[0]->match_core.type)
            ? omnibox_feature_configs::ShortcutBoosting::Get().search_score
            : omnibox_feature_configs::ShortcutBoosting::Get().url_score;

    if (boost_score) {
      client_->GetOmniboxTriggeredFeatureService()->FeatureTriggered(
          metrics::OmniboxEventProto_Feature_SHORTCUT_BOOST);
    }

    if (omnibox_feature_configs::ShortcutBoosting::Get().counterfactual)
      boost_score = 0;
  }

  int relevance = boost_score + number_of_hits;

  // These scoring factors are only useful if boosting is inapplicable or for ML
  // signal logging. Skip computing them otherwise to better measure performance
  // impact of the 2 features.
  size_t shortest_text_length = 0;
  base::Time last_access_time = {};
  if (!boost_score ||
      OmniboxFieldTrial::IsPopulatingUrlScoringSignalsEnabled()) {
    // International characters can change length depending on case. Use the
    // lower case shortcut text length, since the `input_length` is also the
    // lower case length.
    shortest_text_length =
        base::i18n::ToLower(ShortestShortcutText(shortcuts)->text).length();
    last_access_time = MostRecentShortcut(shortcuts)->last_access_time;

    if (!boost_score) {
      relevance = CalculateScoreFromFactors(input_length, shortest_text_length,
                                            last_access_time, number_of_hits,
                                            max_relevance);
    }
  }

  // Pick the shortcut with the shortest content. Picking the shortest
  // shortcut text would probably also work, but could result in more
  // text changes as the user types their input for shortcut texts that
  // are prefixes of each other.
  const ShortcutsDatabase::Shortcut* shortcut =
      ShortestShortcutContent(shortcuts);

  return ShortcutMatch{relevance,
                       number_of_hits,
                       last_access_time,
                       shortest_text_length,
                       stripped_destination_url,
                       shortcut};
}

AutocompleteMatch ShortcutsProvider::ShortcutMatchToACMatch(
    const ShortcutMatch& shortcut_match,
    int relevance,
    const AutocompleteInput& input,
    const std::u16string& fixed_up_input_text,
    const std::u16string lower_input) {
  DCHECK(!input.text().empty());
  AutocompleteMatch match;
  match.provider = this;
  match.relevance = relevance;

  // https://crbug.com/1154982#c36 - When deleting history is disabled by
  // policy, also disable deleting Shortcuts matches, because it's confusing
  // when the X appears on the de-duplicated History and Shortcuts matches.
  match.deletable = client_->AllowDeletingBrowserHistory();

  const ShortcutsDatabase::Shortcut& shortcut = *shortcut_match.shortcut;
  match.fill_into_edit = shortcut.match_core.fill_into_edit;
  match.destination_url = shortcut.match_core.destination_url;
  DCHECK(match.destination_url.is_valid());
  match.stripped_destination_url = shortcut_match.stripped_destination_url;
  DCHECK(match.stripped_destination_url.is_valid());
  match.document_type = shortcut.match_core.document_type;
  match.contents = shortcut.match_core.contents;
  match.contents_class = AutocompleteMatch::ClassificationsFromString(
      shortcut.match_core.contents_class);
  match.description = shortcut.match_core.description;
  match.description_class = AutocompleteMatch::ClassificationsFromString(
      shortcut.match_core.description_class);
  match.transition = shortcut.match_core.transition;
  match.type = shortcut.match_core.type;
  match.keyword = shortcut.match_core.keyword;
  match.shortcut_boosted = relevance > kShortcutsProviderDefaultMaxRelevance;
  match.RecordAdditionalInfo("number of hits",
                             shortcut_match.aggregate_number_of_hits);
  match.RecordAdditionalInfo("last access time",
                             shortcut_match.most_recent_access_time);
  match.RecordAdditionalInfo(
      "shortest shortcut text length",
      static_cast<int>(shortcut_match.shortest_text_length));
  match.RecordAdditionalInfo("original input text", shortcut.text);
  if (match.shortcut_boosted)
    match.RecordAdditionalInfo("shortcut boosted", "true");

  // Set |inline_autocompletion| and |allowed_to_be_default_match| if possible.
  // If the input is in keyword mode, navigation matches cannot be the default
  // match, and search query matches can only be the default match if their
  // keywords matches the input's keyword, as otherwise, default,
  // different-keyword matches may result in leaving keyword mode. Additionally,
  // if the match is a search query, check whether the user text is a prefix of
  // the query. If the match is a navigation, we assume the fill_into_edit looks
  // something like a URL, so we use URLPrefix::GetInlineAutocompleteOffset() to
  // try and strip off any prefixes that the user might not think would change
  // the meaning, but would otherwise prevent inline autocompletion. This
  // allows, for example, the input of "foo.c" to autocomplete to "foo.com" for
  // a fill_into_edit of "http://foo.com".
  const bool is_search_type = AutocompleteMatch::IsSearchType(match.type);
  const bool keyword_matches =
      base::StartsWith(base::UTF16ToUTF8(input.text()),
                       base::StrCat({base::UTF16ToUTF8(match.keyword), " "}),
                       base::CompareCase::INSENSITIVE_ASCII);
  if (is_search_type) {
    const TemplateURL* template_url =
        client_->GetTemplateURLService()->GetDefaultSearchProvider();
    match.from_keyword =
        // Either the default search provider is disabled,
        !template_url ||
        // or the match is not from the default search provider,
        match.keyword != template_url->keyword() ||
        // or keyword mode was invoked explicitly and the keyword in the input
        // is also of the default search provider.
        (input.prefer_keyword() && keyword_matches);
    match.search_terms_args =
        std::make_unique<TemplateURLRef::SearchTermsArgs>(match.contents);
  }

  const bool match_has_explicit_keyword =
      !match
           .GetSubstitutingExplicitlyInvokedKeyword(
               client_->GetTemplateURLService())
           .empty();

  // If the input is in keyword mode, don't inline a match without or with a
  // different keyword. Otherwise, if the input is not in keyword mode, don't
  // inline a match with a keyword.
  if (input.prefer_keyword()
          ? is_search_type && keyword_matches && match_has_explicit_keyword
          : !match_has_explicit_keyword) {
    if (is_search_type) {
      if (match.fill_into_edit.size() >= input.text().size() &&
          std::equal(match.fill_into_edit.begin(),
                     match.fill_into_edit.begin() + input.text().size(),
                     input.text().begin(),
                     SimpleCaseInsensitiveCompareUCS2())) {
        match.inline_autocompletion =
            match.fill_into_edit.substr(input.text().length());
        match.allowed_to_be_default_match =
            !input.prevent_inline_autocomplete() ||
            match.inline_autocompletion.empty();
      }
#if !BUILDFLAG(IS_IOS)
    } else if (match.type != AutocompleteMatch::Type::HISTORY_CLUSTER) {
      // Don't default history cluster suggestions.
#else
    } else {
#endif
      // Try rich autocompletion first. For document suggestions, hide the
      // URL from `additional_text` and don't try to inline the metadata (e.g.
      // 'Google Docs' or '1/1/2023').
      bool autocompleted =
          match.type == AutocompleteMatch::Type::DOCUMENT_SUGGESTION
              ? match.TryRichAutocompletion(
                    u"", ShortcutsBackend::GetSwappedContents(match), input,
                    shortcut.text)
              : match.TryRichAutocompletion(
                    ShortcutsBackend::GetSwappedContents(match),
                    ShortcutsBackend::GetSwappedDescription(match), input,
                    shortcut.text);
      if (!autocompleted) {
        const size_t inline_autocomplete_offset =
            URLPrefix::GetInlineAutocompleteOffset(
                input.text(), fixed_up_input_text, true, match.fill_into_edit);
        if (inline_autocomplete_offset != std::u16string::npos) {
          match.inline_autocompletion =
              match.fill_into_edit.substr(inline_autocomplete_offset);
          match.SetAllowedToBeDefault(input);
        }
      }
    }
  }

  // Try to mark pieces of the contents and description as matches if they
  // appear in |input.text()|.
  if (!lower_input.empty()) {
    match.contents_class = ClassifyAllMatchesInString(
        lower_input, match.contents, is_search_type, match.contents_class);
    match.description_class = ClassifyAllMatchesInString(
        lower_input, match.description,
        /*text_is_search_query=*/false, match.description_class);
  }
  return match;
}

ShortcutsBackend::ShortcutMap::const_iterator ShortcutsProvider::FindFirstMatch(
    const std::u16string& keyword,
    ShortcutsBackend* backend) {
  DCHECK(backend);
  auto it = backend->shortcuts_map().lower_bound(keyword);
  // Lower bound not necessarily matches the keyword, check for item pointed by
  // the lower bound iterator to at least start with keyword.
  return ((it == backend->shortcuts_map().end()) ||
          base::StartsWith(it->first, keyword, base::CompareCase::SENSITIVE))
             ? it
             : backend->shortcuts_map().end();
}
