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

  // Max number of matches we'll show from the various providers.
  static size_t GetMaxMatches();

  AutocompleteResult();
  ~AutocompleteResult();

  // Copies matches from |old_matches| to provide a consistant result set. See
  // comments in code for specifics. Will clear |old_matches| if this result is
  // empty().
  void CopyOldMatches(const AutocompleteInput& input,
                      AutocompleteResult* old_matches,
                      TemplateURLService* template_url_service);

  // Adds a new set of matches to the result set.  Does not re-sort.  Calls
  // PossiblySwapContentsAndDescriptionForURLSuggestion(input)" on all added
  // matches; see comments there for more information.
  void AppendMatches(const AutocompleteInput& input,
                     const ACMatches& matches);

  // Removes duplicates, puts the list in sorted order and culls to leave only
  // the best GetMaxMatches() matches. Sets the default match to the best match
  // and updates the alternate nav URL. On desktop, it filters the matches to be
  // either all tail suggestions (except for the first match) or no tail
  // suggestions.
  void SortAndCull(const AutocompleteInput& input,
                   TemplateURLService* template_url_service);

  // Creates and adds any dedicated Pedal matches triggered by existing match.
  void AppendDedicatedPedalMatches(AutocompleteProviderClient* client,
                                   const AutocompleteInput& input);

  // Sets |pedal| in matches that have Pedal-triggering text.
  void ConvertInSuggestionPedalMatches(AutocompleteProviderClient* client);

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
  static ACMatches::const_iterator FindTopMatch(const ACMatches& matches);
  static ACMatches::iterator FindTopMatch(ACMatches* matches);

  const GURL& alternate_nav_url() const { return alternate_nav_url_; }

  // Clears the matches for this result set.
  void Reset();

  void Swap(AutocompleteResult* other);

  // operator=() by another name.
  void CopyFrom(const AutocompleteResult& rhs);

#if DCHECK_IS_ON()
  // Does a data integrity check on this result.
  void Validate() const;
#endif  // DCHECK_IS_ON()

  // Compute the "alternate navigation URL" for a given match. This is obtained
  // by interpreting the user input directly as a URL. See comments on
  // |alternate_nav_url_|.
  static GURL ComputeAlternateNavUrl(const AutocompleteInput& input,
                                     const AutocompleteMatch& match);

  // Sort |matches| by destination, taking into account demotions based on
  // |page_classification| when resolving ties about which of several
  // duplicates to keep.  The matches are also deduplicated. Duplicate matches
  // are stored in the |duplicate_matches| vector of the corresponding
  // AutocompleteMatch.
  static void SortAndDedupMatches(
      metrics::OmniboxEventProto::PageClassification page_classification,
      ACMatches* matches);

  // Prepend missing tail suggestion prefixes in results, if present.
  void InlineTailPrefixes();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

 private:
  friend class AutocompleteProviderTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest, ConvertsOpenTabsCorrectly);

  typedef std::map<AutocompleteProvider*, ACMatches> ProviderToMatches;

#if defined(OS_ANDROID)
  // iterator::difference_type is not defined in the STL that we compile with on
  // Android.
  typedef int matches_difference_type;
#else
  typedef ACMatches::iterator::difference_type matches_difference_type;
#endif

  // Returns true if |matches| contains a match with the same destination as
  // |match|.
  static bool HasMatchByDestination(const AutocompleteMatch& match,
                                    const ACMatches& matches);

  // If there are both tail and non-tail suggestions (ignoring one default
  // match), remove the tail suggestions.  If the only default matches are tail
  // suggestions, remove the non-tail suggestions.
  static void MaybeCullTailSuggestions(ACMatches* matches);

  // Populates |provider_to_matches| from |matches_|.
  void BuildProviderToMatches(ProviderToMatches* provider_to_matches) const;

  // Copies matches into this result. |old_matches| gives the matches from the
  // last result, and |new_matches| the results from this result.
  void MergeMatchesByProvider(
      metrics::OmniboxEventProto::PageClassification page_classification,
      const ACMatches& old_matches,
      const ACMatches& new_matches);

  // This pulls the relevant fields out of a match for comparison with other
  // matches for the purpose of deduping. It uses the stripped URL, so that we
  // collapse similar URLs if necessary, and whether the match is a calculator
  // suggestion, because we don't want to dedupe them against URLs that simply
  // happen to go to the same destination.
  static std::pair<GURL, bool> GetMatchComparisonFields(
      const AutocompleteMatch& match);

  ACMatches matches_;

  const_iterator default_match_;

  // The "alternate navigation URL", if any, for this result set.  This is a URL
  // to try offering as a navigational option in case the user navigated to the
  // URL of the default match but intended something else.  For example, if the
  // user's local intranet contains site "foo", and the user types "foo", we
  // default to searching for "foo" when the user may have meant to navigate
  // there.  In cases like this, the default match will point to the "search for
  // 'foo'" result, and this will contain "http://foo/".
  GURL alternate_nav_url_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResult);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_RESULT_H_
