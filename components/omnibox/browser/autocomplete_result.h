// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_RESULT_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_RESULT_H_

#include <stddef.h>

#include <map>
#include <optional>
#include <vector>

#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#endif

class AutocompleteInput;
class AutocompleteProvider;
class AutocompleteProviderClient;
class OmniboxTriggeredFeatureService;
class PrefService;
class TemplateURLService;

// All matches from all providers for a particular query.  This also tracks
// what the default match should be if the user doesn't manually select another
// match.
class AutocompleteResult {
 public:
  typedef ACMatches::const_iterator const_iterator;
  typedef ACMatches::iterator iterator;
  using MatchDedupComparator = ACMatchKey<std::string, bool, bool>;

  // Max number of matches we'll show from the various providers. This limit
  // may be different for zero suggest and non zero suggest. Does not take into
  // account the boost conditionally provided by the
  // omnibox::kDynamicMaxAutocomplete feature.
  static size_t GetMaxMatches(bool is_zero_suggest = false);
  // Defaults to GetMaxMatches if omnibox::kDynamicMaxAutocomplete is disabled;
  // otherwise returns the boosted dynamic limit.
  static size_t GetDynamicMaxMatches();

  AutocompleteResult();
  ~AutocompleteResult();
  AutocompleteResult(const AutocompleteResult&) = delete;
  AutocompleteResult& operator=(const AutocompleteResult&) = delete;

#if BUILDFLAG(IS_ANDROID)
  // Returns a corresponding Java object, creating it if necessary.
  // NOTE: Android specific methods are defined in autocomplete_match_android.cc
  base::android::ScopedJavaLocalRef<jobject> GetOrCreateJavaObject(
      JNIEnv* env) const;

  // Notify the Java object that its native counterpart is about to be
  // destroyed.
  void DestroyJavaObject() const;

  // Construct an array of AutocompleteMatch objects arranged in the exact same
  // order as |matches_|.
  base::android::ScopedJavaLocalRef<jobjectArray> BuildJavaMatches(
      JNIEnv* env) const;

  // Group suggestions in specified range by search vs url.
  // The range used is [first_index, last_index), which contains all the
  // elements between first_index and last_index, including the element pointed
  // by first_index, but not the element pointed by last_index.
  void GroupSuggestionsBySearchVsURL(JNIEnv* env,
                                     int first_index,
                                     int last_index);

  // Compares the set of AutocompleteMatch references held by Java with the
  // AutocompleteMatch objects held by this instance of the AutocompleteResult
  // and returns true if the two sets are same.
  // The |match_index|, when different than -1 (|kNoMatchIndex|), specifies the
  // index of a match of particular interest; this index helps identify cases
  // where an action is planned on suggestion at an index that falls outside of
  // bounds of valid AutocompleteResult indices, where every other aspect of the
  // AutocompleteResult is correct.
  bool VerifyCoherency(JNIEnv* env,
                       const base::android::JavaParamRef<jlongArray>& matches,
                       jint match_index,
                       jint verification_point);
#endif

  // Moves matches from |old_matches| to provide a consistent result set.
  // |old_matches| is mutated during this, and should not be used afterwards.
  void TransferOldMatches(const AutocompleteInput& input,
                          AutocompleteResult* old_matches);

  // Adds a new set of matches to the result set.  Does not re-sort.
  void AppendMatches(const ACMatches& matches);

  // Modifies |matches| such that any duplicate matches are coalesced into
  // representative "best" matches. The erased matches are moved into the
  // |duplicate_matches| members of their representative matches.
  void DeduplicateMatches(const AutocompleteInput& input,
                          TemplateURLService* template_url_service);

  // See `Sort()`. `SortAndCull()` also groups and culls the suggestions.
  // TODO(manukh): `Sort()` without grouping and culling is only needed
  //   temporarily for ML ranking without changing the search v URL balance.
  //   After we remove that restriction, they should be re-merged into 1
  //   function, because calling `Sort()` without calling `SortAndCull()`
  //   afterwards is probably invalid.
  void SortAndCull(const AutocompleteInput& input,
                   TemplateURLService* template_url_service,
                   OmniboxTriggeredFeatureService* triggered_feature_service,
                   std::optional<AutocompleteMatch> default_match_to_preserve =
                       std::nullopt);

  // Removes duplicates, puts the list in sorted order. Sets the default match
  // to the best match and updates the alternate nav URL.
  //
  // `default_match_to_preserve` can be used to prevent the default match from
  // being surprisingly swapped out during the asynchronous pass. If it has a
  // value, this method searches the results for that match, and promotes it to
  // the top. But we don't add back that match if it doesn't already exist.
  //
  // On desktop, it filters the matches to be either all tail suggestions
  // (except for the first match) or no tail suggestions.
  //
  // TODO(manukh): `Sort()` is useful for determining the default suggestion
  //   accurately. It includes more code than absolutely necessary for
  //   determining the default suggestion to help avoid accidentally leaving the
  //   results in an invalid state. But it really shouldn't be called except if
  //   `SortAndCull()` is guaranteed to be called soon after. The 2 should be
  //   re-merged into 1 function once this is no longer needed.
  void Sort(const AutocompleteInput& input,
            TemplateURLService* template_url_service,
            std::optional<AutocompleteMatch> default_match_to_preserve);

  // Ensures that matches belonging to suggestion groups, i.e., those with a
  // suggestion_group_id value and a corresponding suggestion group info, are
  // grouped together at the bottom of result set based on the order in which
  // the groups should appear in the result set. This is done for two reasons:
  //
  // 1) Certain groups of remote zero-prefix matches need to appear under a
  // header as specified in omnibox::GroupConfig. GroupConfigs are
  // uniquely identified by the group IDs in |suggestion_groups_map_|. It is
  // also possible for zero-prefix matches to mix and match while belonging to
  // the same groups (e.g., bad server data or mixing of local and remote
  // suggestions from different providers). Hence, after mixing, deduping, and
  // sorting the matches, we group the ones with the same group ID and demote
  // them to the bottom of the result set based on a predetermined order. This
  // ensures matches without group IDs or omnibox::GroupConfig to appear at
  // the top of the result set, and two, there are no interleaving of groups or
  // headers;
  //
  // 2) Certain groups of non-zero-prefix matches, such as those produced by the
  // HistoryClusterProvider, must appear at the bottom of the result set.
  // Specifying a group ID (and a corresponding suggestion group info) for those
  // matches ensures that would happen.
  //
  // Called after matches are deduped and sorted and before they are culled.
  void GroupAndDemoteMatchesInGroups();

  // Filter and remove OmniboxActions according to Platform-specific rules.
  void TrimOmniboxActions(bool is_zero_suggest);

  // Split some `actions` on matches out to become their own matches.
  void SplitActionsToSuggestions();

  // Sets |action| in matches that have Pedal-triggering text.
  void AttachPedalsToMatches(const AutocompleteInput& input,
                             const AutocompleteProviderClient& client);

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

  // Returns the default match if it exists, or nullptr otherwise.
  const AutocompleteMatch* default_match() const;

  // Returns the first match in |matches| which might be chosen as default.
  // If the page is not the fake box, the scores are not demoted by type.
  static ACMatches::const_iterator FindTopMatch(const AutocompleteInput& input,
                                                const ACMatches& matches);
  static ACMatches::iterator FindTopMatch(const AutocompleteInput& input,
                                          ACMatches* matches);

  // If the top match is a Search Entity, and it was deduplicated with a
  // non-entity match, splits off the non-entity match from the list of
  // duplicates and returns true. Otherwise returns false.
  // The non-entity duplicate is promoted to the top, unless the entity match
  // has Action in Suggest where it remains at the top.
  static bool UndedupTopSearchEntityMatch(ACMatches* matches);

  // Just a helper function to encapsulate the logic of deciding how many
  // matches to keep, with respect to configured maximums, URL limits,
  // and relevancies.
  static size_t CalculateNumMatches(
      bool is_zero_suggest,
      const ACMatches& matches,
      const CompareWithDemoteByType<AutocompleteMatch>& comparing_object);
  // Determines how many matches to keep depending on how many URLs would be
  // shown. CalculateNumMatches defers to CalculateNumMatchesPerUrlCount if the
  // kDynamicMaxAutocomplete feature is enabled.
  static size_t CalculateNumMatchesPerUrlCount(
      const ACMatches& matches,
      const CompareWithDemoteByType<AutocompleteMatch>& comparing_object);

  const omnibox::GroupConfigMap& suggestion_groups_map() const {
    return suggestion_groups_map_;
  }

  bool zero_prefix_enabled_in_session() const {
    return session_.zero_prefix_enabled_;
  }

  void set_zero_prefix_enabled_in_session(bool enabled) {
    session_.zero_prefix_enabled_ = enabled;
  }

  size_t num_zero_prefix_suggestions_shown_in_session() const {
    return session_.num_zero_prefix_suggestions_shown_;
  }

  void set_num_zero_prefix_suggestions_shown_in_session(size_t number) {
    session_.num_zero_prefix_suggestions_shown_ = number;
  }

  // Clears this result set - i.e., `matches_` and `suggestion_groups_map_`.
  void ClearMatches();

  // Clears this result set and the session data - i.e., `matches_`,
  // `suggestion_groups_map_` and `session_`. Called when
  // AutocompleteController::Stop() with `clear_result=true` is called.
  void Reset();

#if DCHECK_IS_ON()
  // Does a data integrity check on this result.
  void Validate() const;
#endif  // DCHECK_IS_ON()

  // Returns a URL to offer the user as an alternative navigation when they
  // open |match| after typing in |input|.
  static GURL ComputeAlternateNavUrl(
      const AutocompleteInput& input,
      const AutocompleteMatch& match,
      AutocompleteProviderClient* provider_client);

  // Gets common prefix from SEARCH_SUGGEST_TAIL matches
  std::u16string GetCommonPrefix();

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  // Get a list of comparators used for deduping for the matches in this result.
  // This is only used for logging.
  std::vector<MatchDedupComparator> GetMatchDedupComparators() const;

  // Returns the header string associated with |suggestion_group_id|.
  // Returns an empty string if |suggestion_group_id| is not found in
  // |suggestion_groups_map_|.
  std::u16string GetHeaderForSuggestionGroup(
      omnibox::GroupId suggestion_group_id) const;

  // Returns whether or not |suggestion_group_id| should be collapsed in the UI.
  // This method takes into account both the user's stored prefs as well as
  // the server-provided visibility hint for |suggestion_group_id|.
  // Returns false if |suggestion_group_id| is not found in
  // |suggestion_groups_map_| or if the suggestion group does not contain the
  // original server provided group ID.
  bool IsSuggestionGroupHidden(const PrefService* prefs,
                               omnibox::GroupId suggestion_group_id) const;

  // Sets the UI collapsed/expanded state of the |suggestion_group_id| in the
  // user's stored prefs based on the value of |hidden|.
  // Returns early if |suggestion_group_id| is not found in
  // |suggestion_groups_map_| or if the suggestion group does not contains the
  // original server provided group ID.
  void SetSuggestionGroupHidden(PrefService* prefs,
                                omnibox::GroupId suggestion_group_id,
                                bool hidden) const;

  // Returns the section associated with |suggestion_group_id|.
  // Returns omnibox::SECTION_DEFAULT if |suggestion_group_id| is not found in
  // |suggestion_groups_map_|.
  omnibox::GroupSection GetSectionForSuggestionGroup(
      omnibox::GroupId suggestion_group_id) const;

  // Returns the side type associated with `suggestion_group_id`.
  // Returns omnibox::DEFAULT_PRIMARY if `suggestion_group_id` is not found in
  // `suggestion_groups_map_`.
  omnibox::GroupConfig_SideType GetSideTypeForSuggestionGroup(
      omnibox::GroupId suggestion_group_id) const;

  // Returns the render type associated with `suggestion_group_id`.
  // Returns omnibox::DEFAULT_VERTICAL if `suggestion_group_id` is not found in
  // `suggestion_groups_map_`.
  omnibox::GroupConfig_RenderType GetRenderTypeForSuggestionGroup(
      omnibox::GroupId suggestion_group_id) const;

  // Updates |suggestion_groups_map_| with the suggestion groups information
  // from |suggeston_groups_map|. Followed by GroupAndDemoteMatchesInGroups()
  // which sorts the matches based on the order in which their groups should
  // appear while preserving the existing order of matches within the same
  // group.
  void MergeSuggestionGroupsMap(
      const omnibox::GroupConfigMap& suggeston_groups_map);

  // Erase matches where `predicate` returns true. In other words, this
  // preserves only those matches for which `predicate` returns false.
  template <typename UnaryPredicate>
  size_t EraseMatchesWhere(UnaryPredicate predicate) {
    return std::erase_if(matches_, predicate);
  }

  // This method implements a stateful stable partition. Matches which are
  // search types, and their submatches regardless of type, are shifted
  // earlier in the range, while non-search types and their submatches
  // are shifted later. For grouping purposes, the starter pack suggestions
  // (while technically navigation suggestions) are grouped before search types.
  static void GroupSuggestionsBySearchVsURL(iterator begin, iterator end);

  // This value should be comfortably larger than any max-autocomplete-matches
  // under consideration.
  static constexpr size_t kMaxAutocompletePositionValue = 30;

 private:
  friend class AutocompleteController;
  friend class AutocompleteResultForTesting;
  friend class AutocompleteProviderTest;
  friend class HistoryURLProviderTest;
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest, Desktop_TwoColumnRealbox);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest, Android_TrimOmniboxActions);
  FRIEND_TEST_ALL_PREFIXES(AutocompleteResultTest, SwapMatches);

  typedef std::map<AutocompleteProvider*, ACMatches> ProviderToMatches;

#if BUILDFLAG(IS_ANDROID)
  // iterator::difference_type is not defined in the STL that we compile with on
  // Android.
  typedef int matches_difference_type;
#else
  typedef ACMatches::iterator::difference_type matches_difference_type;
#endif

  struct SessionData {
    void Reset();

    // Whether zero-prefix suggestions could have been shown in the session.
    bool zero_prefix_enabled_ = false;

    // The number of zero-prefix suggestions shown in the session.
    size_t num_zero_prefix_suggestions_shown_ = 0u;
  };

  // Swaps this result set - i.e., `matches_` and `suggestion_groups_map_` -
  // with `other`. Called in AutocompleteController and tests only.
  void SwapMatchesWith(AutocompleteResult* other);

  // Copies the result set - i.e., `matches_` and `suggestion_groups_map_` -
  // from `other`. Called in AutocompleteController and tests only.
  void CopyMatchesFrom(const AutocompleteResult& other);

  // Modifies |matches| such that any duplicate matches are coalesced into
  // representative "best" matches. The erased matches are moved into the
  // |duplicate_matches| members of their representative matches.
  static void DeduplicateMatches(ACMatches* matches,
                                 const AutocompleteInput& input,
                                 TemplateURLService* template_url_service);

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
  void MergeMatchesByProvider(ACMatches* old_matches,
                              const ACMatches& new_matches);

  // Returns a tuple encompassing all attributes relevant to determining whether a match should
  // be deduplicated with another match.
  static MatchDedupComparator GetMatchComparisonFields(
      const AutocompleteMatch& match);

  // This method reduces the number of navigation suggestions to that of
  // |max_url_matches| but will allow more if there are no other types to
  // replace them.
  void LimitNumberOfURLsShown(
      size_t max_matches,
      size_t max_url_count,
      const CompareWithDemoteByType<AutocompleteMatch>& comparing_object);

  // If we have SearchProvider search suggestions, demote OnDeviceProvider
  // search suggestions, since, which in general have lower quality than
  // SearchProvider search suggestions. The demotion can happen in two ways,
  // controlled by Finch (1. decrease-relevances or 2. remove-suggestions):
  // 1. Decrease the on device search suggestion relevances that they will
  //    always be shown after SearchProvider search suggestions.
  // 2. Set the relevances of OnDeviceProvider search suggestions to 0, such
  //    that they will be removed from result list later.
  void DemoteOnDeviceSearchSuggestions();

  // The current result set. Cleared on `ClearMatches()` or `Reset()`.
  ACMatches matches_;

  // The map of suggestion group IDs to suggestion group information for the
  // current result set. Cleared along with `matches_` on `ClearMatches()` or
  // `Reset()`.
  omnibox::GroupConfigMap suggestion_groups_map_;

  // The session data irrespective of the current result set. Cleared on
  // `Reset()`.
  // TODO(crbug.com/40218651): This is a bandaid solution for storing the
  // session data. It relies on `ClearMatches()`, `SwapMatchesWith()`, and
  // `CopyMatchesFrom()` to only modify the current result set and not the
  // session data; and for `Reset()` to be called once during the autocomplete
  // session. Ideally, this should be replaced with a more general solution such
  // as changing the OmniboxTriggeredFeatureService so that it defines an
  // autocomplete session to start when the omnibox is focused and to end when
  // the popup closes.
  SessionData session_;

#if BUILDFLAG(IS_ANDROID)
  // Corresponding Java object.
  // This object should be ignored when AutocompleteResult is copied or moved.
  // This object should never be accessed directly. To acquire a reference to
  // java object, call the GetOrCreateJavaObject().
  // Note that this object is lazily constructed to avoid creating Java matches
  // for throw away AutocompleteMatch objects, eg. during Classify() or
  // QualifyPartialUrlQuery() calls.
  // See AutocompleteControllerAndroid for more details.
  mutable base::android::ScopedJavaGlobalRef<jobject> java_result_;
#endif
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_RESULT_H_
