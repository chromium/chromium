// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace base {
class TimeDelta;
struct Feature;
}

// The set of parameters customizing the HUP scoring.
struct HUPScoringParams {
  // A set of parameters describing how to cap a given count score.  First,
  // we apply a half-life based decay of the given count and then find the
  // maximum relevance score based on the decay factor or counts specified
  // in the corresponding bucket list. See comment on |buckets_| for details.
  class ScoreBuckets {
   public:
    // Stores the max relevance at each count/decay factor threshold.
    typedef std::pair<double, int> CountMaxRelevance;

    ScoreBuckets();
    ScoreBuckets(const ScoreBuckets& other);
    ~ScoreBuckets();

    // Computes a half-life time decay given the |elapsed_time|.
    double HalfLifeTimeDecay(const base::TimeDelta& elapsed_time) const;

    int relevance_cap() const { return relevance_cap_; }
    void set_relevance_cap(int relevance_cap) {
      relevance_cap_ = relevance_cap;
    }

    int half_life_days() const { return half_life_days_; }
    void set_half_life_days(int half_life_days) {
      half_life_days_ = half_life_days;
    }

    bool use_decay_factor() const { return use_decay_factor_; }
    void set_use_decay_factor(bool use_decay_factor) {
      use_decay_factor_ = use_decay_factor;
    }

    std::vector<CountMaxRelevance>& buckets() { return buckets_; }
    const std::vector<CountMaxRelevance>& buckets() const { return buckets_; }

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;

   private:
    // History matches with relevance score greater or equal to |relevance_cap_|
    // are not affected by this experiment.
    // Set to -1, if there is no relevance cap in place and all matches are
    // subject to demotion.
    int relevance_cap_;

    // Half life time for a decayed count as measured since the last visit.
    // Set to -1 if not used.
    int half_life_days_;

    // The relevance score caps at successively decreasing threshold values.
    // The thresholds are either decayed counts or decay factors, depending on
    // the value of |use_decay_factor_|.
    //
    // Consider this example specifying the decayed counts:
    //   [(1, 1000), (0.5, 500), (0, 100)]
    // If decayed count is 2 (which is >= 1), the corresponding match's maximum
    // relevance will be capped at 1000.  In case of 0.5, the score is capped
    // at 500.  Anything below 0.5 is capped at 100.
    //
    // This list is sorted by the pair's first element in descending order.
    std::vector<CountMaxRelevance> buckets_;

    // True when the bucket thresholds are decay factors rather than counts.
    bool use_decay_factor_;
  };

  HUPScoringParams() {}

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  ScoreBuckets typed_count_buckets;

  // Used only when the typed count is 0.
  ScoreBuckets visited_count_buckets;
};

namespace OmniboxFieldTrial {
// A mapping that contains multipliers indicating that matches of the
// specified type should have their relevance score multiplied by the
// given number.  Omitted types are assumed to have multipliers of 1.0.
typedef std::map<AutocompleteMatchType::Type, float> DemotionMultipliers;

// A vector that maps from the number of matching pages to the document
// specificity score used in HistoryQuick provider / ScoredHistoryMatch
// scoring. The vector is sorted by the size_t (the number of matching pages).
// If an entry is omitted, the appropriate value is assumed to be the one in
// the later bucket.  For example, with a vector containing {{1, 2.0},
// {3, 1.5}}, the score for 2 is inferred to be 1.5.  Values beyond the
// end of the vector are assumed to have scores of 1.0.
typedef std::vector<std::pair<size_t, double>> NumMatchesScores;

// Do not change these values as they need to be in sync with values
// specified in experiment configs on the variations server.
enum EmphasizeTitlesCondition {
  EMPHASIZE_WHEN_NONEMPTY = 0,
  EMPHASIZE_WHEN_TITLE_MATCHES = 1,
  EMPHASIZE_WHEN_ONLY_TITLE_MATCHES = 2,
  EMPHASIZE_NEVER = 3
};

// ---------------------------------------------------------
// For any experiment that's part of the bundled omnibox field trial.

// Returns a bitmap containing AutocompleteProvider::Type values
// that should be disabled in AutocompleteController.
int GetDisabledProviderTypes();

// Returns whether the user is in any dynamic field trial where the
// group has a the prefix |group_prefix|.
bool HasDynamicFieldTrialGroupPrefix(const char* group_prefix);

// ---------------------------------------------------------
// For the suggest field trial.

// Populates |field_trial_hash| with hashes of the active suggest field trial
// names, if any.
void GetActiveSuggestFieldTrialHashes(std::vector<uint32_t>* field_trial_hash);

// ---------------------------------------------------------
// For the AutocompleteController "stop timer" field trial.

// Returns the duration to be used for the AutocompleteController's stop
// timer.  Returns the default value of 1.5 seconds if the stop timer
// override experiment isn't active or if parsing the experiment-provided
// duration fails.
base::TimeDelta StopTimerFieldTrialDuration();

// ---------------------------------------------------------
// For the ZeroSuggestProvider field trial.

// Returns the configured "ZeroSuggestVariant" parameter values for
// |page_classification|.
std::vector<std::string> GetZeroSuggestVariants(
    metrics::OmniboxEventProto::PageClassification page_classification);

// ---------------------------------------------------------
// For the ShortcutsScoringMaxRelevance experiment that's part of the
// bundled omnibox field trial.

// If the user is in an experiment group that, given the provided
// |current_page_classification| context, changes the maximum relevance
// ShortcutsProvider::CalculateScore() is supposed to assign, extract
// that maximum relevance score and put in in |max_relevance|.  Returns
// true on a successful extraction.  CalculateScore()'s return value is
// a product of this maximum relevance score and some attenuating factors
// that are all between 0 and 1.  (Note that Shortcuts results may have
// their scores reduced later if the assigned score is higher than allowed
// for non-inlineable results.  Shortcuts results are not allowed to be
// inlined.)
bool ShortcutsScoringMaxRelevance(
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    int* max_relevance);

// ---------------------------------------------------------
// For the SearchHistory experiment that's part of the bundled omnibox
// field trial.

// Returns true if the user is in the experiment group that, given the
// provided |current_page_classification| context, scores search history
// query suggestions less aggressively so that they don't inline.
bool SearchHistoryPreventInlining(
    metrics::OmniboxEventProto::PageClassification current_page_classification);

// Returns true if the user is in the experiment group that, given the
// provided |current_page_classification| context, disables all query
// suggestions from search history.
bool SearchHistoryDisable(
    metrics::OmniboxEventProto::PageClassification current_page_classification);

// ---------------------------------------------------------
// For the DemoteByType experiment that's part of the bundled omnibox field
// trial.

// If the user is in an experiment group that, in the provided
// |current_page_classification| context, demotes the relevance scores
// of certain types of matches, populates the |demotions_by_type| map
// appropriately.  Otherwise, sets |demotions_by_type| to its default
// value based on the context.
void GetDemotionsByType(
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    DemotionMultipliers* demotions_by_type);

// ---------------------------------------------------------
// For the UIMaxAutocompleteMatchesByProvider experiment that's part of the
// bundled omnibox field trial.

// If the user is in an experiment group that specifies the max results for a
// particular provider, returns the limit. Otherwise returns the default limit.
size_t GetProviderMaxMatches(AutocompleteProvider::Type provider);

// ---------------------------------------------------------
// For the HistoryURL provider new scoring experiment that is part of the
// bundled omnibox field trial.

// Initializes the HUP |scoring_params| based on the active HUP scoring
// experiment.
void GetDefaultHUPScoringParams(HUPScoringParams* scoring_params);
void GetExperimentalHUPScoringParams(HUPScoringParams* scoring_params);

// ---------------------------------------------------------
// For the HQPBookmarkValue experiment that's part of the
// bundled omnibox field trial.

// Returns the value an untyped visit to a bookmark should receive.
// Compare this value with the default of 1 for non-bookmarked untyped
// visits to pages and the default of 20 for typed visits.  Returns
// 10 if the bookmark value experiment isn't active.
float HQPBookmarkValue();

// ---------------------------------------------------------
// For the HQPAllowMatchInTLD experiment that's part of the
// bundled omnibox field trial.

// Returns true if HQP should allow an input term to match in the
// top level domain (e.g., .com) of a URL.  Returns false if the
// allow match in TLD experiment isn't active.
bool HQPAllowMatchInTLDValue();

// ---------------------------------------------------------
// For the HQPAllowMatchInScheme experiment that's part of the
// bundled omnibox field trial.

// Returns true if HQP should allow an input term to match in the
// scheme (e.g., http://) of a URL.  Returns false if the allow
// match in scheme experiment isn't active.
bool HQPAllowMatchInSchemeValue();

// ---------------------------------------------------------
// For SearchProvider related experiments.

// Returns how the search provider should poll Suggest. Currently, we support
// measuring polling delay from the last keystroke or last suggest request.
void GetSuggestPollingStrategy(bool* from_last_keystroke,
                               int* polling_delay_ms);

// ---------------------------------------------------------
// For HQP scoring related experiments to control the topicality and scoring
// ranges of relevancy scores.

// Returns the scoring buckets for HQP experiments. Returns an empty string
// if scoring buckets are not specified in the field trial. Scoring buckets
// are stored in string form giving mapping from (topicality_score,
// frequency_score) to final relevance score. Please see GetRelevancyScore()
// under chrome/browser/history::ScoredHistoryMatch for details.
std::string HQPExperimentalScoringBuckets();

// Returns the topicality threshold for HQP experiments. Returns a default
// value of 0.5 if no threshold is specified in the field trial.
float HQPExperimentalTopicalityThreshold();

// ---------------------------------------------------------
// For experiment to limit HQP url indexing that's part of the bundled
// omnibox field trial.

// Returns the maximum number of history urls to index for HQP at the startup.
// Note: this limit is only applied at startup and more urls can be indexed
// during the session. Returns -1 if limit is not set by trials.
int MaxNumHQPUrlsIndexedAtStartup();

// ---------------------------------------------------------
// For the HQPFixFrequencyScoring experiment that's part of the
// bundled omnibox field trial.

// Returns the number of visits HQP should use when computing frequency
// scores.  Returns 10 if the epxeriment isn't active.
size_t HQPMaxVisitsToScore();

// Returns the score that should be given to typed transitions.  (The score
// of non-typed transitions is 1.)  Returns 1.5 if the experiment isn't
// active.
float HQPTypedValue();

// Returns NumMatchesScores; see comment by the declaration of it.
// If the experiment isn't active, returns an NumMatchesScores of
// {{1, 3}, {2, 2.5}, {3, 2}, {4, 1.5}}.
NumMatchesScores HQPNumMatchesScores();

// ---------------------------------------------------------
// For the HQPNumTitleWords experiment that's part of the
// bundled omnibox field trial.

// Returns the number of title words that are allowed to contribute
// to the topicality score.  Words later in the title are ignored.
// Returns 20 as a default if the experiment isn't active.
size_t HQPNumTitleWordsToAllow();

// ---------------------------------------------------------
// For the replace HUP experiment that's part of the bundled omnibox field
// trial.

// Returns whether HistoryQuick provider (HQP) should attempt to score
// suggestions also with a HistoryURL-provider-like (HUP-like) mode, and
// assign suggestions the max of this score and the normal score.
// Returns false if the experiment isn't active.
bool HQPAlsoDoHUPLikeScoring();

// Returns whether HistoryURL provider (HUP) should search its database for
// URLs and suggest them.  If false, HistoryURL provider merely creates the
// URL-what-you-typed match when appropriate.  Return true if the experiment
// isn't active.
bool HUPSearchDatabase();

// ---------------------------------------------------------
// For the aggressive keyword matching experiment that's part of the bundled
// omnibox field trial.

// One function is missing from here to avoid a cyclic dependency
// between search_engine and omnibox. In the search_engine component
// there is a OmniboxFieldTrialKeywordRequiresRegistry function
// that logically should be here.
//
// It returns whether KeywordProvider should consider the registry portion
// (e.g., co.uk) of keywords that look like hostnames as an important part of
// the keyword name for matching purposes.  Returns true if the experiment
// isn't active.

// For keywords that look like hostnames, returns whether KeywordProvider
// should require users to type a prefix of the hostname to match against
// them, rather than just the domain name portion.  In other words, returns
// whether the prefix before the domain name should be considered important
// for matching purposes.  Returns true if the experiment isn't active.
bool KeywordRequiresPrefixMatch();

// Returns the relevance score that KeywordProvider should assign to keywords
// with sufficiently-complete matches, i.e., the user has typed all of the
// important part of the keyword.  Returns -1 if the experiment isn't active.
int KeywordScoreForSufficientlyCompleteMatch();

// ---------------------------------------------------------
// For the EmphasizeTitles experiment that's part of the bundled omnibox
// field trial.

// Returns the conditions under which the UI code should display the title
// of a URL more prominently than the URL for input |input|. Normally the URL
// is displayed more prominently. Returns NEVER_EMPHASIZE if the experiment
// isn't active.
EmphasizeTitlesCondition GetEmphasizeTitlesConditionForInput(
    const AutocompleteInput& input);

// Returns the maximum number of URL matches that should be allowed within
// the Omnibox if there are search-type matches available to replace them.
// If the capping feature is not enabled, or the parameter cannot be
// parsed, it returns 0.
size_t GetMaxURLMatches();

// ---------------------------------------------------------
// For UI experiments.

// Returns whether preserve default match score is enabled.
bool IsPreserveDefaultMatchScoreEnabled();

// Returns true if the reverse answers flag is enabled.
bool IsReverseAnswersEnabled();

// Returns true if the short bookmark suggestions flag is enabled.
bool IsShortBookmarkSuggestionsEnabled();

// Returns true if either the tab switch suggestions flag is enabled.
bool IsTabSwitchSuggestionsEnabled();

// Returns true if dedicated rows for tab switch suggestions is enabled.
bool IsTabSwitchSuggestionsDedicatedRowEnabled();

// Returns true if feature is enabled to not count submatches towards the
// max suggestion limit.
bool IsLooseMaxLimitOnDedicatedRowsEnabled();

// Returns true if the #omnibox-pedal-suggestions feature is enabled.
bool IsPedalSuggestionsEnabled();

// Returns true if either the steady-state elision flag for scheme is enabled.
bool IsHideSteadyStateUrlSchemeEnabled();

// Returns true if either the steady-state elision flag for trivial
// subdomains is enabled.
bool IsHideSteadyStateUrlTrivialSubdomainsEnabled();

// Simply a convenient wrapper for testing a flag. Used downstream for an
// assortment of keyword mode experiments.
bool IsExperimentalKeywordModeEnabled();

// Returns whether the group suggestions by type feature is enabled,
// which "bunches" search suggestions (except for the default match).
bool IsGroupSuggestionsBySearchVsUrlFeatureEnabled();

// Returns whether the feature to limit the number of shown URL matches
// is enabled.
bool IsMaxURLMatchesFeatureEnabled();

// ---------------------------------------------------------
// Clipboard URL suggestions:

// The parameter "ClipboardURLMaximumAge" doesn't live in this file; instead
// it lives in
// components/open_from_clipboard/clipboard_recent_content.cc.
// Please see ClipboardRecentContent::MaximumAgeOfClipboard() for the usage
// of it.  The parameter cannot live here because that component cannot
// include this component, else there would be a circular dependency.

// ---------------------------------------------------------
// Exposed publicly for the sake of unittests.
extern const char kBundledExperimentFieldTrialName[];
// Rule names used by the bundled experiment.
extern const char kDisableProvidersRule[];
extern const char kShortcutsScoringMaxRelevanceRule[];
extern const char kSearchHistoryRule[];
extern const char kDemoteByTypeRule[];
extern const char kHQPBookmarkValueRule[];
extern const char kHQPTypedValueRule[];
extern const char kHQPAllowMatchInTLDRule[];
extern const char kHQPAllowMatchInSchemeRule[];
extern const char kZeroSuggestVariantRule[];
extern const char kMeasureSuggestPollingDelayFromLastKeystrokeRule[];
extern const char kSuggestPollingDelayMsRule[];
extern const char kHQPMaxVisitsToScoreRule[];
extern const char kHQPNumMatchesScoresRule[];
extern const char kHQPNumTitleWordsRule[];
extern const char kHQPAlsoDoHUPLikeScoringRule[];
extern const char kHUPSearchDatabaseRule[];
extern const char kPreventUWYTDefaultForNonURLInputsRule[];
extern const char kKeywordRequiresRegistryRule[];
extern const char kKeywordRequiresPrefixMatchRule[];
extern const char kKeywordScoreForSufficientlyCompleteMatchRule[];
extern const char kHQPAllowDupMatchesForScoringRule[];
extern const char kEmphasizeTitlesRule[];

// Parameter name used by the Omnibox match capping experiment.
extern const char kOmniboxMaxURLMatchesParam[];

// Parameter names used by the HUP new scoring experiments.
extern const char kHUPNewScoringTypedCountRelevanceCapParam[];
extern const char kHUPNewScoringTypedCountHalfLifeTimeParam[];
extern const char kHUPNewScoringTypedCountScoreBucketsParam[];
extern const char kHUPNewScoringTypedCountUseDecayFactorParam[];
extern const char kHUPNewScoringVisitedCountRelevanceCapParam[];
extern const char kHUPNewScoringVisitedCountHalfLifeTimeParam[];
extern const char kHUPNewScoringVisitedCountScoreBucketsParam[];
extern const char kHUPNewScoringVisitedCountUseDecayFactorParam[];

// Parameter names used by the HQP experimental scoring experiments.
extern const char kHQPExperimentalScoringBucketsParam[];
extern const char kHQPExperimentalScoringTopicalityThresholdParam[];

// Parameter names used by the experiment that limits the number of history
// urls indexed for suggestions.
extern const char kMaxNumHQPUrlsIndexedAtStartupOnLowEndDevicesParam[];
extern const char kMaxNumHQPUrlsIndexedAtStartupOnNonLowEndDevicesParam[];

// Parameter names used by UI experiments.
extern const char kUIMaxAutocompleteMatchesParam[];
extern const char kUIMaxAutocompleteMatchesByProviderParam[];

// The amount of time to wait before sending a new suggest request after the
// previous one unless overridden by a field trial parameter.
// Non-const because some unittests modify this value.
extern int kDefaultMinimumTimeBetweenSuggestQueriesMs;

namespace internal {
// The bundled omnibox experiment comes with a set of parameters
// (key-value pairs).  Each key indicates a certain rule that applies in
// a certain context.  The value indicates what the consequences of
// applying the rule are.  For example, the value of a SearchHistory rule
// in the context of a search results page might indicate that we should
// prevent search history matches from inlining.
//
// This function returns the value associated with the |rule| that applies
// in the current context (which currently consists of |page_classification|
// and whether Instant Extended is enabled).  If no such rule exists in the
// current context, fall back to the rule in various wildcard contexts and
// return its value if found.  If the rule remains unfound in the global
// context, returns the empty string.  For more details, including how we
// prioritize different wildcard contexts, see the implementation.  How to
// interpret the value is left to the caller; this is rule-dependent.
//
// Deprecated. Use GetValueForRuleInContextByFeature instead.
std::string GetValueForRuleInContext(
    const std::string& rule,
    metrics::OmniboxEventProto::PageClassification page_classification);

// Same as GetValueForRuleInContext, but by |feature| instead of the bundled
// omnibox experiment.  Prefer to use this method over GetValueForRuleInContext
// when possible, as it can be useful to configure parameters outside of the
// omnibox bundled experiment.
std::string GetValueForRuleInContextByFeature(
    const base::Feature& feature,
    const std::string& rule,
    metrics::OmniboxEventProto::PageClassification page_classification);

}  // namespace internal

}  // namespace OmniboxFieldTrial

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_
