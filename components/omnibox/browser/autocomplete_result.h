// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_RESULT_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_RESULT_H_

#include <stddef.h>

#include <map>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/match_compare.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

class AutocompleteInput;
class AutocompleteProvider;
class AutocompleteProviderClient;
class TemplateURLService;

// All matches from all providers for a particular query.  This also tracks
// what the default match should be if the user doesn't manually select another
// match.
class AutocompleteResult {
 public:
  typedef ACMatches::const_iterator const_iterator;
  typedef ACMatches::iterator iterator;
  using MatchDedupComparator = std::pair<GURL, bool>;

  // Max number of matches we'll show from the various providers.
  static size_t GetMaxMatches(bool is_zero_suggest = false);

  AutocompleteResult();
  ~AutocompleteResult();

  // Moves matches from |old_matches| to provide a consistent result set.
  // |old_matches| is mutated during this, and should not be used afterwards.
  void TransferOldMatches(const AutocompleteInput& input,
                          AutocompleteResult* old_matches,
                          TemplateURLService* template_url_service);

  // Adds a new set of matches to the result set.  Does not re-sort.  Calls
  // PossiblySwapContentsAndDescriptionForURLSuggestion(input)" on all added
  // matches; see comments there for more information.
  void AppendMatches(const AutocompleteInput& input,
                     const ACMatches& matches);

  // Removes duplicates, puts the list in sorted order and culls to leave only
  // the best GetMaxMatches() matches. Sets the default match to the best match
  // and updates the alternate nav URL.
  //
  // |preserve_default_match| can be used to prevent the default match from
  // being surprisingly swapped out during the asynchronous pass. If it has a
  // value, this method searches the results for that match, and promotes it to
  // the top. But we don't add back that match if it doesn't already exist.
  //
  // On desktop, it filters the matches to be either all tail suggestions
  // (except for the first match) or no tail suggestions.
  void SortAndCull(const AutocompleteInput& input,
                   TemplateURLService* template_url_service,
                   const AutocompleteMatch* preserve_default_match = nullptr);

  // Creates and adds any dedicated Pedal matches triggered by existing matches.
  // This should be the only place where new Pedal suggestions are introduced
  // because it doesn't dedupe; it just carefully avoids adding duplicates.
  void AppendDedicatedPedalMatches(AutocompleteProviderClient* client,
                                   const AutocompleteInput& input);

  // Sets |has_tab_match| in matches whose URL matches an open tab's URL.
  // Also, fixes up the description if not using another UI element to
  // annotate (e.g. tab switch button). |input| can be null; if provided,
  // the match can be more precise (e.g. scheme presence).
  void ConvertOpenTabMatches(AutocompleteProviderClient* client,
                             const AutocompleteInput* input);

  // Returns true if at least one match was copied from the last result.
  bool HasCopiedMatches() const;

  // Vector-style accessors/operators.
  size_t size() const;
  bool empty() const;
  const_iterator begin() const;
  iterator begin();
  const_iterator end() const;
  iterator end();

  // Returns the match at the given index.
  const AutocompleteMatch& match_at(size_t index) const;
  AutocompleteMatch* match_at(size_t index);

  // Get the default match for the query (not necessarily the first).  Returns
  // end() if there is no default match.
  const_iterator default_match() const { return default_match_; }

  // Returns true if the top match is a verbatim search or URL match (see
  // IsVerbatimType() in autocomplete_match.h), and the next match is not also
  // some kind of verbatim match.
  bool TopMatchIsStandaloneVerbatimMatch() const;

  // Returns the first match in |matches| which might be chosen as default.
  // If |kOmniboxPreserveDefaultMatchScore| is enabled and the page is not
  // the fake box, the scores are not demoted by type.
  static ACMatches::const_iterator FindTopMatch(const AutocompleteInput& input,
                                                const ACMatches& matches);
  static ACMatches::iterator FindTopMatch(const AutocompleteInput& input,
                                          ACMatches* matches);

  // If the top match is a Search Entity, and it was deduplicated with a
  // non-entity match, split off the non-entity match from the list of
  // duplicates and promote it to the top.
  static void DiscourageTopMatchFromBeingSearchEntity(ACMatches* matches);

  // Just a helper function to encapsulate the logic of deciding how many
  // matches to keep, with respect to configured maximums, URL limits,
  // and relevancies.
  static size_t CalculateNumMatches(
      bool input_from_omnibox_focus,
      const ACMatches& matches,
      const CompareWithDemoteByType<AutocompleteMatch>& comparing_object);

  // Clears the matches for this result set.
  void Reset();

  void Swap(AutocompleteResult* other);

  // operator=() by another name.
  void CopyFrom(const AutocompleteResult& rhs);

#if DCHECK_IS_ON()
  // Does a data integrity check on this result.
  void Validate() const;
#endif  // DCHECK_IS_ON()

  // Returns a URL to offer the user as an alternative navigation when they
  // open |match| after typing in |input|.
  static GURL ComputeAlternateNavUrl(const AutocompleteInput& input,
                                     const AutocompleteMatch& match);

  // Prepend missing tail suggestion prefixes in results, if present.
  void InlineTailPrefixes();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Get a list of comparators used for deduping for the matches in this result.
  std::vector<MatchDedupComparator> GetMatchDedupComparators() const;

  // Logs metrics for when |new_result| replaces |old_result| asynchronously.
  // |old_result| a list of the comparators for the old matches.
  static void LogAsynchronousUpdateMetrics(
      const std::vector<MatchDedupComparator>& old_result,
      const AutocompleteResult& new_result);

 private:
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest, ConvertsOpenTabsCorrectly);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest,
                           PedalSuggestionsRemainUnique);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest,
                           TestGroupSuggestionsBySearchVsURL);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest,
                           DemoteOnDeviceSearchSuggestions);
  friend class HistoryURLProviderTest;

  typedef std::map<AutocompleteProvider*, ACMatches> ProviderToMatches;

#if defined(OS_ANDROID)
  // iterator::difference_type is not defined in the STL that we compile with on
  // Android.
  typedef int matches_difference_type;
#else
  typedef ACMatches::iterator::difference_type matches_difference_type;
#endif

  // Modifies |matches| such that any duplicate matches are coalesced into
  // representative "best" matches. The erased matches are moved into the
  // |duplicate_matches| members of their representative matches.
  static void DeduplicateMatches(
      metrics::OmniboxEventProto::PageClassification page_classification,
      ACMatches* matches);

  // Returns true if |matches| contains a match with the same destination as
  // |match|.
  static bool HasMatchByDestination(const AutocompleteMatch& match,
                                    const ACMatches& matches);

  // If there are both tail and non-tail suggestions (ignoring one default
  // match), remove the tail suggestions.  If the only default matches are tail
  // suggestions, remove the non-tail suggestions.
  static void MaybeCullTailSuggestions(
      ACMatches* matches,
      const CompareWithDemoteByType<AutocompleteMatch>& comparing_object);

  // Populates |provider_to_matches| from |matches_|. This AutocompleteResult
  // should not be used after the 'move' version.
  void BuildProviderToMatchesCopy(ProviderToMatches* provider_to_matches) const;
  void BuildProviderToMatchesMove(ProviderToMatches* provider_to_matches);

  // Moves matches into this result. |old_matches| gives the matches from the
  // last result, and |new_matches| the results from this result. |old_matches|
  // should not be used afterwards.
  void MergeMatchesByProvider(
      metrics::OmniboxEventProto::PageClassification page_classification,
      ACMatches* old_matches,
      const ACMatches& new_matches);

  // This pulls the relevant fields out of a match for comparison with other
  // matches for the purpose of deduping. It uses the stripped URL, so that we
  // collapse similar URLs if necessary, and whether the match is a calculator
  // suggestion, because we don't want to dedupe them against URLs that simply
  // happen to go to the same destination.
  static MatchDedupComparator GetMatchComparisonFields(
      const AutocompleteMatch& match);

  // This method reduces the number of navigation suggestions to that of
  // |max_url_matches| but will allow more if there are no other types to
  // replace them.
  void LimitNumberOfURLsShown(
      size_t max_url_count,
      const CompareWithDemoteByType<AutocompleteMatch>& comparing_object);

  // This method implements a stateful stable partition. Matches which are
  // search types, and their submatches regardless of type, are shifted
  // earlier in the range, while non-search types and their submatches
  // are shifted later.
  static void GroupSuggestionsBySearchVsURL(iterator begin, iterator end);

  // If we have SearchProvider search suggestions, demote OnDeviceProvider
  // search suggestions, since, which in general have lower quality than
  // SearchProvider search suggestions. The demotion can happen in two ways,
  // controlled by Finch (1. decrease-relevances or 2. remove-suggestions):
  // 1. Decrease the on device search suggestion relevances that they will
  //    always be shown after SearchProvider search suggestions.
  // 2. Set the relevances of OnDeviceProvider search suggestions to 0, such
  //    that they will be removed from result list later.
  void DemoteOnDeviceSearchSuggestions();

  ACMatches matches_;

  const_iterator default_match_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResult);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_RESULT_H_
