// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_result.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <string>
#include <unordered_set>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/omnibox_pedal_provider.h"
#include "components/omnibox/browser/omnibox_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "ui/base/l10n/l10n_util.h"

struct MatchGURLHash {
  // The |bool| is whether the match is a calculator suggestion. We want them
  // compare differently against other matches with the same URL.
  size_t operator()(const std::pair<GURL, bool>& p) const {
    return std::hash<std::string>()(p.first.spec()) + p.second;
  }
};

// A match attribute when a default match's score has been boosted
// with a higher scoring non-default match.
static const char kScoreBoostedFrom[] = "score_boosted_from";

// static
size_t AutocompleteResult::GetMaxMatches() {
  constexpr size_t kDefaultMaxAutocompleteMatches = 6;

  return base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam,
      kDefaultMaxAutocompleteMatches);
}

AutocompleteResult::AutocompleteResult() {
  // Reserve space for the max number of matches we'll show.
  matches_.reserve(GetMaxMatches());

  // It's probably safe to do this in the initializer list, but there's little
  // penalty to doing it here and it ensures our object is fully constructed
  // before calling member functions.
  default_match_ = end();
}

AutocompleteResult::~AutocompleteResult() {}

void AutocompleteResult::CopyOldMatches(
    const AutocompleteInput& input,
    AutocompleteResult* old_matches,
    TemplateURLService* template_url_service) {
  if (old_matches->empty())
    return;

  if (empty()) {
    // If we've got no matches we can copy everything from the last result.
    Swap(old_matches);
    for (auto i(begin()); i != end(); ++i)
      i->from_previous = true;
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
  BuildProviderToMatches(&matches_per_provider);
  old_matches->BuildProviderToMatches(&old_matches_per_provider);
  for (ProviderToMatches::const_iterator i(old_matches_per_provider.begin());
       i != old_matches_per_provider.end(); ++i) {
    MergeMatchesByProvider(input.current_page_classification(),
                           i->second, matches_per_provider[i->first]);
  }

  SortAndCull(input, template_url_service);
}

void AutocompleteResult::AppendMatches(const AutocompleteInput& input,
                                       const ACMatches& matches) {
  for (const auto& i : matches) {
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i.contents), i.contents);
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i.description),
              i.description);
    matches_.push_back(i);
    if (!AutocompleteMatch::IsSearchType(i.type) &&
        i.type != AutocompleteMatchType::DOCUMENT_SUGGESTION) {
      const OmniboxFieldTrial::EmphasizeTitlesCondition condition(
          OmniboxFieldTrial::GetEmphasizeTitlesConditionForInput(input));
      bool emphasize = false;
      switch (condition) {
        case OmniboxFieldTrial::EMPHASIZE_WHEN_NONEMPTY:
          emphasize = !i.description.empty();
          break;
        case OmniboxFieldTrial::EMPHASIZE_WHEN_TITLE_MATCHES:
          emphasize = !i.description.empty() &&
              AutocompleteMatch::HasMatchStyle(i.description_class);
          break;
        case OmniboxFieldTrial::EMPHASIZE_WHEN_ONLY_TITLE_MATCHES:
          emphasize = !i.description.empty() &&
              AutocompleteMatch::HasMatchStyle(i.description_class) &&
              !AutocompleteMatch::HasMatchStyle(i.contents_class);
          break;
        case OmniboxFieldTrial::EMPHASIZE_NEVER:
          break;
        default:
          NOTREACHED();
      }
      matches_.back().swap_contents_and_description = emphasize;
    }
  }
  default_match_ = end();
  alternate_nav_url_ = GURL();
}

void AutocompleteResult::SortAndCull(
    const AutocompleteInput& input,
    TemplateURLService* template_url_service) {
  for (auto i(matches_.begin()); i != matches_.end(); ++i)
    i->ComputeStrippedDestinationURL(input, template_url_service);

#if !(defined(OS_ANDROID) || defined(OS_IOS))
  // Wipe tail suggestions if not exclusive (minus default match).
  MaybeCullTailSuggestions(&matches_);
#endif
  SortAndDedupMatches(input.current_page_classification(), &matches_);

  // Sort and trim to the most relevant GetMaxMatches() matches.
  CompareWithDemoteByType<AutocompleteMatch> comparing_object(
      input.current_page_classification());
  std::sort(matches_.begin(), matches_.end(), comparing_object);
  // Top match is not allowed to be the default match.  Find the most
  // relevant legal match and shift it to the front.
  auto it = FindTopMatch(&matches_);
  if (it != matches_.end())
    std::rotate(matches_.begin(), it, it + 1);
  // In the process of trimming, drop all matches with a demoted relevance
  // score of 0.
  const size_t max_num_matches = std::min(GetMaxMatches(), matches_.size());
  size_t num_matches;
  for (num_matches = 0u; (num_matches < max_num_matches) &&
       (comparing_object.GetDemotedRelevance(*match_at(num_matches)) > 0);
       ++num_matches) {}
  matches_.resize(num_matches);

  default_match_ = matches_.begin();

  if (default_match_ != matches_.end()) {
    const base::string16 debug_info =
        base::ASCIIToUTF16("fill_into_edit=") + default_match_->fill_into_edit +
        base::ASCIIToUTF16(", provider=") +
        ((default_match_->provider != nullptr)
             ? base::ASCIIToUTF16(default_match_->provider->GetName())
             : base::string16()) +
        base::ASCIIToUTF16(", input=") + input.text();

    // We should only get here with an empty omnibox for automatic suggestions
    // on focus on the NTP; in these cases hitting enter should do nothing, so
    // there should be no default match.  Otherwise, we're doing automatic
    // suggestions for the currently visible URL (and hitting enter should
    // reload it), or the user is typing; in either of these cases, there should
    // be a default match.
    DCHECK_NE(input.text().empty(), default_match_->allowed_to_be_default_match)
        << debug_info;

    // For navigable default matches, make sure the destination type is what the
    // user would expect given the input.
    if (default_match_->allowed_to_be_default_match &&
        default_match_->destination_url.is_valid()) {
      if (AutocompleteMatch::IsSearchType(default_match_->type)) {
        // We shouldn't get query matches for URL inputs.
        DCHECK_NE(metrics::OmniboxInputType::URL, input.type()) << debug_info;
      } else {
        // If the user explicitly typed a scheme, the default match should
        // have the same scheme.
        if ((input.type() == metrics::OmniboxInputType::URL) &&
            input.parts().scheme.is_nonempty()) {
          const std::string& in_scheme = base::UTF16ToUTF8(input.scheme());
          const std::string& dest_scheme =
              default_match_->destination_url.scheme();
          DCHECK(url_formatter::IsEquivalentScheme(in_scheme, dest_scheme))
              << debug_info;
        }
      }
    }
  }

  // Set the alternate nav URL.
  alternate_nav_url_ = (default_match_ == matches_.end()) ?
      GURL() : ComputeAlternateNavUrl(input, *default_match_);
}

void AutocompleteResult::AppendDedicatedPedalMatches(
    AutocompleteProviderClient* client,
    const AutocompleteInput& input) {
  ACMatches pedal_suggestions;
  const OmniboxPedalProvider* provider = client->GetPedalProvider();
  for (const auto& match : matches_) {
    if (match.pedal)
      continue;
    OmniboxPedal* pedal = provider->FindPedalMatch(match.contents);
    if (pedal) {
      AutocompleteMatch suggestion = match;
      suggestion.relevance--;
      suggestion.pedal = pedal;
      suggestion.ApplyPedal();
      pedal_suggestions.push_back(suggestion);
    }
  }
  if (!pedal_suggestions.empty()) {
    AppendMatches(input, pedal_suggestions);
  }
}

void AutocompleteResult::ConvertInSuggestionPedalMatches(
    AutocompleteProviderClient* client) {
  const OmniboxPedalProvider* provider = client->GetPedalProvider();
  // Used to ensure we keep only one Pedal of each kind.
  std::unordered_set<OmniboxPedal*> pedals_found;
  for (auto& match : matches_) {
    // Skip matches that will not show Pedal because they already
    // have a tab match or associated keyword.  Also skip matches
    // that have already detected their Pedal.
    if (match.has_tab_match || match.associated_keyword || match.pedal)
      continue;

    OmniboxPedal* const pedal = provider->FindPedalMatch(match.contents);
    if (pedal) {
      const auto result = pedals_found.insert(pedal);
      if (result.second)
        match.pedal = pedal;
    }
  }
}

void AutocompleteResult::ConvertOpenTabMatches(
    AutocompleteProviderClient* client,
    const AutocompleteInput* input) {
  for (auto& match : matches_) {
    // If already converted this match, don't re-search through open tabs and
    // possibly re-change the description.
    if (match.has_tab_match)
      continue;
    // If URL is in a tab, remember that.
    if (client->IsTabOpenWithURL(match.destination_url, input))
      match.has_tab_match = true;
  }
}

bool AutocompleteResult::HasCopiedMatches() const {
  for (auto i(begin()); i != end(); ++i) {
    if (i->from_previous)
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

// Returns the match at the given index.
const AutocompleteMatch& AutocompleteResult::match_at(size_t index) const {
  DCHECK_LT(index, matches_.size());
  return matches_[index];
}

AutocompleteMatch* AutocompleteResult::match_at(size_t index) {
  DCHECK_LT(index, matches_.size());
  return &matches_[index];
}

bool AutocompleteResult::TopMatchIsStandaloneVerbatimMatch() const {
  if (empty() || !match_at(0).IsVerbatimType())
    return false;

  // Skip any copied matches, under the assumption that they'll be expired and
  // disappear.  We don't want this disappearance to cause the visibility of the
  // top match to change.
  for (auto i(begin() + 1); i != end(); ++i) {
    if (!i->from_previous)
      return !i->IsVerbatimType();
  }
  return true;
}

// static
ACMatches::const_iterator AutocompleteResult::FindTopMatch(
    const ACMatches& matches) {
  auto it = matches.begin();
  while ((it != matches.end()) && !it->allowed_to_be_default_match)
    ++it;
  return it;
}

// static
ACMatches::iterator AutocompleteResult::FindTopMatch(ACMatches* matches) {
  auto it = matches->begin();
  while ((it != matches->end()) && !it->allowed_to_be_default_match)
    ++it;
  return it;
}

void AutocompleteResult::Reset() {
  matches_.clear();
  default_match_ = end();
}

void AutocompleteResult::Swap(AutocompleteResult* other) {
  const size_t default_match_offset = default_match_ - begin();
  const size_t other_default_match_offset =
      other->default_match_ - other->begin();
  matches_.swap(other->matches_);
  default_match_ = begin() + other_default_match_offset;
  other->default_match_ = other->begin() + default_match_offset;
  alternate_nav_url_.Swap(&(other->alternate_nav_url_));
}

void AutocompleteResult::CopyFrom(const AutocompleteResult& rhs) {
  if (this == &rhs)
    return;

  matches_ = rhs.matches_;
  // Careful!  You can't just copy iterators from another container, you have to
  // reconstruct them.
  default_match_ = (rhs.default_match_ == rhs.end())
                       ? end()
                       : (begin() + (rhs.default_match_ - rhs.begin()));

  alternate_nav_url_ = rhs.alternate_nav_url_;
}

#if DCHECK_IS_ON()
void AutocompleteResult::Validate() const {
  for (auto i(begin()); i != end(); ++i)
    i->Validate();
}
#endif  // DCHECK_IS_ON()

// static
GURL AutocompleteResult::ComputeAlternateNavUrl(
    const AutocompleteInput& input,
    const AutocompleteMatch& match) {
  return ((input.type() == metrics::OmniboxInputType::UNKNOWN) &&
          (AutocompleteMatch::IsSearchType(match.type)) &&
          !ui::PageTransitionCoreTypeIs(match.transition,
                                        ui::PAGE_TRANSITION_KEYWORD) &&
          (input.canonicalized_url() != match.destination_url))
             ? input.canonicalized_url()
             : GURL();
}

void AutocompleteResult::SortAndDedupMatches(
    metrics::OmniboxEventProto::PageClassification page_classification,
    ACMatches* matches) {
  // Group matches by stripped URL and whether it's a calculator suggestion.
  std::unordered_map<std::pair<GURL, bool>, std::list<ACMatches::iterator>,
                     MatchGURLHash>
      url_to_matches;
  for (auto i = matches->begin(); i != matches->end(); ++i) {
    std::pair<GURL, bool> p = GetMatchComparisonFields(*i);
    url_to_matches[p].push_back(i);
  }
  CompareWithDemoteByType<AutocompleteMatch> compare_demote_by_type(
      page_classification);
  // Find best default, and non-default, match in each group.
  for (auto& group : url_to_matches) {
    const auto& p = group.first;
    const GURL& gurl = p.first;
    // The list of matches whose URL are equivalent.
    auto& duplicate_matches = group.second;
    if (gurl.is_empty() || duplicate_matches.size() == 1)
      continue;

    auto best_match = duplicate_matches.end();
    auto best_default = duplicate_matches.end();
    for (auto i = duplicate_matches.begin(); i != duplicate_matches.end();
         ++i) {
      if ((*i)->allowed_to_be_default_match) {
        if (best_default == duplicate_matches.end() ||
            // This object implements greater than.
            compare_demote_by_type(**i, **best_default)) {
          best_default = i;
        }
      }
      if (best_match == duplicate_matches.end() ||
          compare_demote_by_type(**i, **best_match)) {
        best_match = i;
      }
    }
    if (best_match != best_default && best_default != duplicate_matches.end()) {
      (*best_default)
          ->RecordAdditionalInfo(kScoreBoostedFrom, (*best_default)->relevance);
      (*best_default)->relevance = (*best_match)->relevance;
      best_match = best_default;
    }
    // Rotate best first if necessary, so we know to keep it.
    if (best_match != duplicate_matches.begin()) {
      duplicate_matches.splice(duplicate_matches.begin(), duplicate_matches,
                               best_match);
    }
    for (auto i = std::next(best_match); i != duplicate_matches.end(); ++i) {
      // For each duplicate match, append its duplicates to that of the best
      // match, then append it, before we erase it.
      (*best_match)->duplicate_matches.insert(
          (*best_match)->duplicate_matches.end(),
          (*i)->duplicate_matches.begin(),
          (*i)->duplicate_matches.end());
      (*best_match)->duplicate_matches.push_back(**i);
    }
  }
  // Erase duplicate matches.
  matches->erase(
      std::remove_if(
          matches->begin(), matches->end(),
          [&url_to_matches](const AutocompleteMatch& m) {
            std::pair<GURL, bool> p = GetMatchComparisonFields(m);
            return !m.stripped_destination_url.is_empty() &&
                   &(*url_to_matches[p].front()) != &m;
          }),
      matches->end());
}

void AutocompleteResult::InlineTailPrefixes() {
  base::string16 common_prefix;

  for (const auto& match : matches_) {
    if (match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL) {
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
  if (common_prefix.size()) {
    for (auto& match : matches_)
      match.InlineTailPrefix(common_prefix);
  }
}

size_t AutocompleteResult::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(matches_);
  res += base::trace_event::EstimateMemoryUsage(alternate_nav_url_);

  return res;
}

// static
bool AutocompleteResult::HasMatchByDestination(const AutocompleteMatch& match,
                                               const ACMatches& matches) {
  for (auto i(matches.begin()); i != matches.end(); ++i) {
    if (i->destination_url == match.destination_url)
      return true;
  }
  return false;
}

// static
void AutocompleteResult::MaybeCullTailSuggestions(ACMatches* matches) {
  std::function<bool(const AutocompleteMatch&)> is_tail =
      [](const AutocompleteMatch& match) {
        return match.type == AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
      };
  auto non_tail_default = std::find_if(
      matches->begin(), matches->end(), [&](const AutocompleteMatch& match) {
        return match.allowed_to_be_default_match && !is_tail(match);
      });
  // If the only default matches are tail suggestions, let them remain and
  // instead remove the non-tail suggestions.  This is necessary because we do
  // not want to display tail suggestions mixed with other suggestions in the
  // dropdown below the first item (the default match).  In this case, we
  // cannot remove the tail suggestions because we'll be left without a legal
  // default match--the non-tail ones much go.  This situation though is
  // unlikely, as we normally would expect the search-what-you-typed suggestion
  // as a default match (and that's a non-tail suggestion).
  if (non_tail_default == matches->end()) {
    base::EraseIf(*matches, std::not1(is_tail));
    return;
  }
  // Determine if there are both tail and non-tail matches, excluding the
  // non-tail default match.
  bool any_tail = false, any_non_tail = false;
  for (auto i = matches->begin();
       i != matches->end() && !(any_tail && any_non_tail); ++i) {
    // We allow one default non-tail match.
    if (i != non_tail_default) {
      if (is_tail(*i))
        any_tail = true;
      else
        any_non_tail = true;
    }
  }
  // If both tail and non-tail matches, remove tail. Note that this can
  // remove the highest rated suggestions.
  if (any_tail) {
    if (any_non_tail) {
      base::EraseIf(*matches, is_tail);
    } else {
      // We want the non-tail default match to be first. Mark tail suggestions
      // as not a legal default match, so that the default match will be moved
      // up explicitly.
      for (auto& match : *matches) {
        if (is_tail(match))
          match.allowed_to_be_default_match = false;
      }
    }
  }
}

void AutocompleteResult::BuildProviderToMatches(
    ProviderToMatches* provider_to_matches) const {
  for (auto i(begin()); i != end(); ++i)
    (*provider_to_matches)[i->provider].push_back(*i);
}

void AutocompleteResult::MergeMatchesByProvider(
    metrics::OmniboxEventProto::PageClassification page_classification,
    const ACMatches& old_matches,
    const ACMatches& new_matches) {
  if (new_matches.size() >= old_matches.size())
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

  DCHECK(i->allowed_to_be_default_match);
  const int max_relevance = i->relevance - 1;

  // Because the goal is a visibly-stable popup, rather than one that preserves
  // the highest-relevance matches, we copy in the lowest-relevance matches
  // first. This means that within each provider's "group" of matches, any
  // synchronous matches (which tend to have the highest scores) will
  // "overwrite" the initial matches from that provider's previous results,
  // minimally disturbing the rest of the matches.
  size_t delta = old_matches.size() - new_matches.size();
  for (auto i(old_matches.rbegin()); i != old_matches.rend() && delta > 0;
       ++i) {
    if (!HasMatchByDestination(*i, new_matches)) {
      AutocompleteMatch match = *i;
      match.relevance = std::min(max_relevance, match.relevance);
      match.from_previous = true;
      matches_.push_back(match);
      delta--;
    }
  }
}

std::pair<GURL, bool> AutocompleteResult::GetMatchComparisonFields(
    const AutocompleteMatch& match) {
  return std::make_pair(match.stripped_destination_url,
                        match.type == AutocompleteMatchType::CALCULATOR);
}
