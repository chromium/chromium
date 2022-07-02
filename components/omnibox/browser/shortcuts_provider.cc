// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_provider.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_i18n.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"

namespace {

class DestinationURLEqualsURL {
 public:
  explicit DestinationURLEqualsURL(const GURL& url) : url_(url) {}
  bool operator()(const AutocompleteMatch& match) const {
    return match.destination_url == url_;
  }

 private:
  const GURL url_;
};

// ShortcutMatch holds sufficient information about a single match from the
// shortcut database to allow for destination deduping and relevance sorting.
// After those stages the top matches are converted to the more heavyweight
// AutocompleteMatch struct.  Avoiding constructing the larger struct for
// every such match can save significant time when there are many shortcut
// matches to process.
struct ShortcutMatch {
  ShortcutMatch(int relevance,
                const GURL& stripped_destination_url,
                const ShortcutsDatabase::Shortcut* shortcut)
      : relevance(relevance),
        stripped_destination_url(stripped_destination_url),
        shortcut(shortcut),
        contents(shortcut->match_core.contents),
        type(shortcut->match_core.type) {}

  int relevance;
  // To satisfy |CompareWithDemoteByType<>::operator()|.
  size_t subrelevance = 0;
  GURL stripped_destination_url;
  raw_ptr<const ShortcutsDatabase::Shortcut> shortcut;
  std::u16string contents;
  AutocompleteMatch::Type type;

  AutocompleteMatch::Type GetDemotionType() const { return type; }
};

// Sorts |matches| by destination, taking into account demotions based on
// |page_classification| when resolving ties about which of several
// duplicates to keep.  The matches are also deduplicated.
void SortAndDedupMatches(
    metrics::OmniboxEventProto::PageClassification page_classification,
    std::vector<ShortcutMatch>* matches) {
  // Sort matches such that duplicate matches are consecutive.
  std::sort(matches->begin(), matches->end(),
            DestinationSort<ShortcutMatch>(page_classification));

  // Erase duplicate matches. Duplicate matches are those with
  // stripped_destination_url fields equal and non empty.
  matches->erase(
      std::unique(matches->begin(), matches->end(),
                  [](const ShortcutMatch& elem1, const ShortcutMatch& elem2) {
                    return !elem1.stripped_destination_url.is_empty() &&
                           (elem1.stripped_destination_url ==
                            elem2.stripped_destination_url);
                  }),
      matches->end());
}

// Helpers for extracting aggregated factors from a vector of shortcuts.
const ShortcutsDatabase::Shortcut* ShortestShortcutText(
    std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return *base::ranges::min_element(
      shortcuts, [](auto len1, auto len2) { return len1 < len2; },
      [](const auto* shortcut) { return shortcut->text.length(); });
}

const ShortcutsDatabase::Shortcut* MostRecentShortcut(
    std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return *base::ranges::max_element(
      shortcuts,
      [](const auto& time1, const auto& time2) { return time1 < time2; },
      [](const auto* shortcut) { return shortcut->last_access_time; });
}

int SumNumberOfHits(std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return std::accumulate(shortcuts.begin(), shortcuts.end(), 0,
                         [](int sum, const auto* shortcut) {
                           return sum + shortcut->number_of_hits;
                         });
}

const ShortcutsDatabase::Shortcut* ShortestShortcutContent(
    std::vector<const ShortcutsDatabase::Shortcut*> shortcuts) {
  return *base::ranges::min_element(
      shortcuts, [](auto len1, auto len2) { return len1 < len2; },
      [](const auto* shortcut) {
        return shortcut->match_core.contents.length();
      });
}

// Helper for `CalculateScore()` and `CalculateAggregateScore()` to score
// shortcuts based on their individual or aggregate factors.
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
  // `ShortcutsBackend::AddOrUpdateShortcut()`). As an approximation, ignore the
  // 10 chars in the shortcut text, though this can overestimate or
  // underestimate the actual previous inputs. Shortcuts are often deduped with
  // higher scoring history suggestions anyway.
  const size_t adjustment =
      OmniboxFieldTrial::IsShortcutExpandingEnabled() ? 10 : 3;
  const size_t adjusted_text_length =
      std::max(shortcut_text_length, typed_length + adjustment) - adjustment;
  const double typed_fraction =
      static_cast<double>(typed_length) / adjusted_text_length;

  // Using the square root of the typed fraction boosts the base score rapidly
  // as characters are typed, compared with simply using the typed fraction
  // directly. This makes sense since the first characters typed are much more
  // important for determining how likely it is a user wants a particular
  // shortcut than are the remaining continued characters.
  const double base_score = max_relevance * sqrt(typed_fraction);

  // Then we decay this by half each week.
  const double kLn2 = 0.6931471805599453;
  base::TimeDelta time_passed = base::Time::Now() - last_access_time;
  // Clamp to 0 in case time jumps backwards (e.g. due to DST).
  double decay_exponent = std::max(0.0, kLn2 * time_passed / base::Days(7));

  // We modulate the decay factor based on how many times the shortcut has been
  // used. Newly created shortcuts decay at full speed; otherwise, decaying by
  // half takes |n| times as much time, where n increases by
  // (1.0 / each 5 additional hits), up to a maximum of 5x as long.
  const double kMaxDecaySpeedDivisor = 5.0;
  const double kNumUsesPerDecaySpeedDivisorIncrement = 5.0;
  const double decay_divisor =
      std::min(kMaxDecaySpeedDivisor,
               (number_of_hits + kNumUsesPerDecaySpeedDivisorIncrement - 1) /
                   kNumUsesPerDecaySpeedDivisorIncrement);

  return base::ClampRound(base_score / exp(decay_exponent / decay_divisor));
}

}  // namespace

const int ShortcutsProvider::kShortcutsProviderDefaultMaxRelevance = 1199;

ShortcutsProvider::ShortcutsProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_SHORTCUTS),
      client_(client),
      initialized_(false) {
  scoped_refptr<ShortcutsBackend> backend = client_->GetShortcutsBackend();
  if (backend) {
    backend->AddObserver(this);
    if (backend->initialized())
      initialized_ = true;
  }
}

void ShortcutsProvider::Start(const AutocompleteInput& input,
                              bool minimal_changes) {
  TRACE_EVENT0("omnibox", "ShortcutsProvider::Start");
  matches_.clear();

  if (input.focus_type() == OmniboxFocusType::DEFAULT &&
      input.type() != metrics::OmniboxInputType::EMPTY &&
      !input.text().empty() && initialized_) {
    GetMatches(input);
  }
}

void ShortcutsProvider::DeleteMatch(const AutocompleteMatch& match) {
  // Copy the URL since deleting from |matches_| will invalidate |match|.
  GURL url(match.destination_url);
  DCHECK(url.is_valid());

  // When a user deletes a match, they probably mean for the URL to disappear
  // out of history entirely. So nuke all shortcuts that map to this URL.
  scoped_refptr<ShortcutsBackend> backend =
      client_->GetShortcutsBackendIfExists();
  if (backend)  // Can be NULL in Incognito.
    backend->DeleteShortcutsWithURL(url);

  base::EraseIf(matches_, DestinationURLEqualsURL(url));
  // NOTE: |match| is now dead!

  // Delete the match from the history DB. This will eventually result in a
  // second call to DeleteShortcutsWithURL(), which is harmless.
  history::HistoryService* const history_service = client_->GetHistoryService();
  DCHECK(history_service);
  history_service->DeleteURLs({url});
}

ShortcutsProvider::~ShortcutsProvider() {
  scoped_refptr<ShortcutsBackend> backend =
      client_->GetShortcutsBackendIfExists();
  if (backend)
    backend->RemoveObserver(this);
}

void ShortcutsProvider::OnShortcutsLoaded() {
  initialized_ = true;
}

void ShortcutsProvider::GetMatches(const AutocompleteInput& input) {
  scoped_refptr<ShortcutsBackend> backend =
      client_->GetShortcutsBackendIfExists();
  if (!backend)
    return;
  // Get the URLs from the shortcuts database with keys that partially or
  // completely match the search term.
  std::u16string term_string(base::i18n::ToLower(input.text()));
  DCHECK(!term_string.empty());

  int max_relevance;
  if (!OmniboxFieldTrial::ShortcutsScoringMaxRelevance(
          input.current_page_classification(), &max_relevance))
    max_relevance = kShortcutsProviderDefaultMaxRelevance;
  TemplateURLService* template_url_service = client_->GetTemplateURLService();
  const std::u16string fixed_up_input(FixupUserInput(input).second);

  // Get the shortcuts from the database with keys that partially or completely
  // match the search term.
  std::vector<ShortcutMatch> shortcut_matches;
  if (base::FeatureList::IsEnabled(omnibox::kAggregateShortcuts)) {
    // If `kAggregateShortcuts` is enabled, group the matching shortcuts by
    // stripped `destination_url`, score them together, and create a single
    // `ShortcutMatch`.
    std::map<GURL, std::vector<const ShortcutsDatabase::Shortcut*>>
        shortcuts_by_url;
    for (auto it = FindFirstMatch(term_string, backend.get());
         it != backend->shortcuts_map().end() &&
         base::StartsWith(it->first, term_string, base::CompareCase::SENSITIVE);
         ++it) {
      const ShortcutsDatabase::Shortcut& shortcut = it->second;
      const GURL stripped_destination_url(AutocompleteMatch::GURLToStrippedGURL(
          shortcut.match_core.destination_url, input, template_url_service,
          shortcut.match_core.keyword));
      shortcuts_by_url[stripped_destination_url].push_back(&shortcut);
    }
    for (auto const& it : shortcuts_by_url) {
      int relevance =
          CalculateAggregateScore(term_string, it.second, max_relevance);
      // Don't return shortcuts with zero relevance.
      if (relevance) {
        // When `kAggregateShortcuts` is disabled, the highest scored shortcut
        // is picked, followed by shortest content if equally scored. Since
        // we're scoring them in aggregate, there are no individual scores to
        // consider, so we just pick the shortest content. Picking the shortest
        // shortcut text would probably also work, but could result in more
        // text changes as the user types their input for shortcut texts that
        // are prefixes of each other.
        const ShortcutsDatabase::Shortcut* shortcut =
            ShortestShortcutContent(it.second);
        shortcut_matches.emplace_back(relevance, it.first, shortcut);
      }
    }

  } else {
    // If `kAggregateShortcuts` is disabled, score each matching shortcut
    // individually and create 1 `ShortcutMatch` for each. Dedupe them
    // afterwards.
    for (auto it = FindFirstMatch(term_string, backend.get());
         it != backend->shortcuts_map().end() &&
         base::StartsWith(it->first, term_string, base::CompareCase::SENSITIVE);
         ++it) {
      // Don't return shortcuts with zero relevance.
      int relevance = CalculateScore(term_string, it->second, max_relevance);
      if (relevance) {
        const ShortcutsDatabase::Shortcut& shortcut = it->second;
        GURL stripped_destination_url(AutocompleteMatch::GURLToStrippedGURL(
            shortcut.match_core.destination_url, input, template_url_service,
            shortcut.match_core.keyword));
        shortcut_matches.emplace_back(relevance, stripped_destination_url,
                                      &it->second);
      }
    }
    // Remove duplicates.  This is important because it's common to have
    // multiple shortcuts pointing to the same URL, e.g., ma, mai, and mail all
    // pointing to mail.google.com, so typing "m" will return them all.  If we
    // then simply clamp to provider_max_matches_ and let the
    // SortAndDedupMatches take care of collapsing the duplicates, we'll
    // effectively only be returning one match, instead of several
    // possibilities.
    //
    // Note that while removing duplicates, we don't populate a match's
    // |duplicate_matches| field--duplicates don't need to be preserved in the
    // matches because they are only used for deletions, and this provider
    // deletes matches based on the URL.
    SortAndDedupMatches(input.current_page_classification(), &shortcut_matches);
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
  if (shortcut_matches.size() > provider_max_matches_) {
    shortcut_matches.erase(shortcut_matches.begin() + provider_max_matches_,
                           shortcut_matches.end());
  }
  // Create and initialize autocomplete matches from shortcut matches.
  // Also guarantee that all relevance scores are decreasing (but do not assign
  // any scores below 1).
  matches_.reserve(shortcut_matches.size());
  for (ShortcutMatch& match : shortcut_matches) {
    max_relevance = std::min(max_relevance, match.relevance);
    matches_.push_back(ShortcutToACMatch(*match.shortcut, max_relevance, input,
                                         fixed_up_input, term_string));
    if (max_relevance > 1)
      --max_relevance;
  }
}

AutocompleteMatch ShortcutsProvider::ShortcutToACMatch(
    const ShortcutsDatabase::Shortcut& shortcut,
    int relevance,
    const AutocompleteInput& input,
    const std::u16string& fixed_up_input_text,
    const std::u16string term_string) {
  DCHECK(!input.text().empty());
  AutocompleteMatch match;
  match.provider = this;
  match.relevance = relevance;

  // https://crbug.com/1154982#c36 - When deleting history is disabled by
  // policy, also disable deleting Shortcuts matches, because it's confusing
  // when the X appears on the de-duplicated History and Shortcuts matches.
  match.deletable = client_->AllowDeletingBrowserHistory();

  match.fill_into_edit = shortcut.match_core.fill_into_edit;
  match.destination_url = shortcut.match_core.destination_url;
  DCHECK(match.destination_url.is_valid());
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
  match.RecordAdditionalInfo("number of hits", shortcut.number_of_hits);
  match.RecordAdditionalInfo("last access time", shortcut.last_access_time);
  match.RecordAdditionalInfo("original input text", shortcut.text);

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

  DCHECK(is_search_type != match.keyword.empty());

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
    } else {
      // Try rich autocompletion first. For document suggestions,
      // `match.contents` is the title, while `description` is something like
      // 'Google Docs' and shouldn't be autocompleted. For all other nav
      // suggestions, `contents` is the URL and `description` is the title.
      bool autocompleted =
          match.type == AutocompleteMatch::Type::DOCUMENT_SUGGESTION
              ? match.TryRichAutocompletion(u"", match.contents, input,
                                            shortcut.text)
              : match.TryRichAutocompletion(match.contents, match.description,
                                            input, shortcut.text);

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
  if (!term_string.empty()) {
    match.contents_class = ClassifyAllMatchesInString(
        term_string, match.contents, is_search_type, match.contents_class);
    match.description_class = ClassifyAllMatchesInString(
        term_string, match.description,
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

int ShortcutsProvider::CalculateScore(
    const std::u16string& terms,
    const ShortcutsDatabase::Shortcut& shortcut,
    int max_relevance) {
  return CalculateScoreFromFactors(terms.length(), shortcut.text.length(),
                                   shortcut.last_access_time,
                                   shortcut.number_of_hits, max_relevance);
}

int ShortcutsProvider::CalculateAggregateScore(
    const std::u16string& terms,
    const std::vector<const ShortcutsDatabase::Shortcut*>& shortcuts,
    int max_relevance) {
  DCHECK_GT(shortcuts.size(), 0u);
  const size_t shortest_text_length =
      ShortestShortcutText(shortcuts)->text.length();
  const base::Time& last_access_time =
      MostRecentShortcut(shortcuts)->last_access_time;
  const int number_of_hits = SumNumberOfHits(shortcuts);
  return CalculateScoreFromFactors(terms.length(), shortest_text_length,
                                   last_access_time, number_of_hits,
                                   max_relevance);
}
