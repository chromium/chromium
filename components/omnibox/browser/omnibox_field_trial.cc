// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_field_trial.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search/search.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/variations_associated_data.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/material_design/material_design_controller.h"

using metrics::OmniboxEventProto;

namespace {

typedef std::map<std::string, std::string> VariationParams;
typedef HUPScoringParams::ScoreBuckets ScoreBuckets;

// Field trial names.
const char kStopTimerFieldTrialName[] = "OmniboxStopTimer";

void InitializeBucketsFromString(const std::string& bucket_string,
                                 ScoreBuckets* score_buckets) {
  // Clear the buckets.
  score_buckets->buckets().clear();
  base::StringPairs kv_pairs;
  if (base::SplitStringIntoKeyValuePairs(bucket_string, ':', ',', &kv_pairs)) {
    for (base::StringPairs::const_iterator it = kv_pairs.begin();
         it != kv_pairs.end(); ++it) {
      ScoreBuckets::CountMaxRelevance bucket;
      base::StringToDouble(it->first, &bucket.first);
      base::StringToInt(it->second, &bucket.second);
      score_buckets->buckets().push_back(bucket);
    }
    std::sort(score_buckets->buckets().begin(),
              score_buckets->buckets().end(),
              std::greater<ScoreBuckets::CountMaxRelevance>());
  }
}

void InitializeScoreBuckets(const VariationParams& params,
                            const char* relevance_cap_param,
                            const char* half_life_param,
                            const char* score_buckets_param,
                            const char* use_decay_factor_param,
                            ScoreBuckets* score_buckets) {
  auto it = params.find(relevance_cap_param);
  if (it != params.end()) {
    int relevance_cap;
    if (base::StringToInt(it->second, &relevance_cap))
      score_buckets->set_relevance_cap(relevance_cap);
  }

  it = params.find(use_decay_factor_param);
  if (it != params.end()) {
    int use_decay_factor;
    if (base::StringToInt(it->second, &use_decay_factor))
      score_buckets->set_use_decay_factor(use_decay_factor != 0);
  }

  it = params.find(half_life_param);
  if (it != params.end()) {
    int half_life_days;
    if (base::StringToInt(it->second, &half_life_days))
      score_buckets->set_half_life_days(half_life_days);
  }

  it = params.find(score_buckets_param);
  if (it != params.end()) {
    // The value of the score bucket is a comma-separated list of
    // {DecayedCount/DecayedFactor + ":" + MaxRelevance}.
    InitializeBucketsFromString(it->second, score_buckets);
  }
}

// Background and implementation details:
//
// Each experiment group in any field trial can come with an optional set of
// parameters (key-value pairs).  In the bundled omnibox experiment
// (kBundledExperimentFieldTrialName), each experiment group comes with a
// list of parameters in the form:
//   key=<Rule>:
//       <OmniboxEventProto::PageClassification (as an int)>:
//       <whether Instant Extended is enabled (as a 1 or 0)>
//     (note that there are no linebreaks in keys; this format is for
//      presentation only>
//   value=<arbitrary string>
// Both the OmniboxEventProto::PageClassification and the Instant Extended
// entries can be "*", which means this rule applies for all values of the
// matching portion of the context.
// One example parameter is
//   key=SearchHistory:6:1
//   value=PreventInlining
// This means in page classification context 6 (a search result page doing
// search term replacement) with Instant Extended enabled, the SearchHistory
// experiment should PreventInlining.
//
// When an exact match to the rule in the current context is missing, we
// give preference to a wildcard rule that matches the instant extended
// context over a wildcard rule that matches the page classification
// context.  Hopefully, though, users will write their field trial configs
// so as not to rely on this fall back order.
//
// In short, this function tries to find the value associated with key
// |rule|:|page_classification|:|instant_extended|, failing that it looks up
// |rule|:*:|instant_extended|, failing that it looks up
// |rule|:|page_classification|:*, failing that it looks up |rule|:*:*,
// and failing that it returns the empty string.
std::string GetValueForRuleInContextFromVariationParams(
    const std::map<std::string, std::string>& params,
    const std::string& rule,
    OmniboxEventProto::PageClassification page_classification) {
  if (params.empty())
    return std::string();

  const std::string page_classification_str =
      base::NumberToString(static_cast<int>(page_classification));
  const std::string instant_extended =
      search::IsInstantExtendedAPIEnabled() ? "1" : "0";
  // Look up rule in this exact context.
  VariationParams::const_iterator it = params.find(
      rule + ":" + page_classification_str + ":" + instant_extended);
  if (it != params.end())
    return it->second;
  // Fall back to the global page classification context.
  it = params.find(rule + ":*:" + instant_extended);
  if (it != params.end())
    return it->second;
  // Fall back to the global instant extended context.
  it = params.find(rule + ":" + page_classification_str + ":*");
  if (it != params.end())
    return it->second;
  // Look up rule in the global context.
  it = params.find(rule + ":*:*");
  return (it != params.end()) ? it->second : std::string();
}

}  // namespace

HUPScoringParams::ScoreBuckets::ScoreBuckets()
    : relevance_cap_(-1),
      half_life_days_(-1),
      use_decay_factor_(false) {
}

HUPScoringParams::ScoreBuckets::ScoreBuckets(const ScoreBuckets& other) =
    default;

HUPScoringParams::ScoreBuckets::~ScoreBuckets() {
}

size_t HUPScoringParams::ScoreBuckets::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(buckets_);
}

double HUPScoringParams::ScoreBuckets::HalfLifeTimeDecay(
    const base::TimeDelta& elapsed_time) const {
  double time_ms;
  if ((half_life_days_ <= 0) ||
      ((time_ms = elapsed_time.InMillisecondsF()) <= 0))
    return 1.0;

  const double half_life_intervals =
      time_ms / base::TimeDelta::FromDays(half_life_days_).InMillisecondsF();
  return pow(2.0, -half_life_intervals);
}

size_t HUPScoringParams::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(typed_count_buckets);
  res += base::trace_event::EstimateMemoryUsage(visited_count_buckets);

  return res;
}

int OmniboxFieldTrial::GetDisabledProviderTypes() {
  const std::string& types_string = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kDisableProvidersRule);
  int types = 0;
  if (types_string.empty() || !base::StringToInt(types_string, &types)) {
    return 0;
  }
  return types;
}

void OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(
    std::vector<uint32_t>* field_trial_hashes) {
  field_trial_hashes->clear();
  if (base::FieldTrialList::TrialExists(kBundledExperimentFieldTrialName)) {
    field_trial_hashes->push_back(
        variations::HashName(kBundledExperimentFieldTrialName));
  }
}

base::TimeDelta OmniboxFieldTrial::StopTimerFieldTrialDuration() {
  int stop_timer_ms;
  if (base::StringToInt(
      base::FieldTrialList::FindFullName(kStopTimerFieldTrialName),
          &stop_timer_ms))
    return base::TimeDelta::FromMilliseconds(stop_timer_ms);
  return base::TimeDelta::FromMilliseconds(1500);
}

// static
std::vector<std::string> OmniboxFieldTrial::GetZeroSuggestVariants(
    OmniboxEventProto::PageClassification page_classification) {
  // We check all these features for ZeroSuggestVariant because it's not
  // possible to enable multiple features using Finch Forcing groups
  // (omnibox::kOnFocusSuggestions as well as another feature). Therefore, in
  // order to specify the ZeroSuggestVariant parameter in those groups we allow
  // it to be associated with the feature that is being force enabled.
  const base::Feature* features_to_check[] = {
      &omnibox::kZeroSuggestionsOnNTP,
      &omnibox::kZeroSuggestionsOnNTPRealbox,
      &omnibox::kZeroSuggestionsOnSERP,
      &omnibox::kOnFocusSuggestions,
  };
  for (const base::Feature* feature : features_to_check) {
    auto parameter_value = internal::GetValueForRuleInContextByFeature(
        *feature, kZeroSuggestVariantRule, page_classification);
    if (!parameter_value.empty()) {
      return base::SplitString(parameter_value, ",", base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    }
  }

  return {};
}

bool OmniboxFieldTrial::ShortcutsScoringMaxRelevance(
    OmniboxEventProto::PageClassification current_page_classification,
    int* max_relevance) {
  // The value of the rule is a string that encodes an integer containing
  // the max relevance.
  const std::string& max_relevance_str =
      OmniboxFieldTrial::internal::GetValueForRuleInContext(
          kShortcutsScoringMaxRelevanceRule, current_page_classification);
  if (max_relevance_str.empty())
    return false;
  if (!base::StringToInt(max_relevance_str, max_relevance))
    return false;
  return true;
}

bool OmniboxFieldTrial::SearchHistoryPreventInlining(
    OmniboxEventProto::PageClassification current_page_classification) {
  return OmniboxFieldTrial::internal::GetValueForRuleInContext(
             kSearchHistoryRule, current_page_classification) ==
         "PreventInlining";
}

bool OmniboxFieldTrial::SearchHistoryDisable(
    OmniboxEventProto::PageClassification current_page_classification) {
  return OmniboxFieldTrial::internal::GetValueForRuleInContext(
             kSearchHistoryRule, current_page_classification) == "Disable";
}

void OmniboxFieldTrial::GetDemotionsByType(
    OmniboxEventProto::PageClassification current_page_classification,
    DemotionMultipliers* demotions_by_type) {
  demotions_by_type->clear();

  // Explicitly check whether the feature is enabled before calling
  // |GetValueForRuleInContextByFeature| because it is possible for
  // |GetValueForRuleInContextByFeature| to return an empty string even if the
  // feature is enabled, and we don't want to fallback to
  // |GetValueForRuleInContext| in this case.
  std::string demotion_rule =
      base::FeatureList::IsEnabled(omnibox::kOmniboxDemoteByType)
          ? OmniboxFieldTrial::internal::GetValueForRuleInContextByFeature(
                omnibox::kOmniboxDemoteByType, kDemoteByTypeRule,
                current_page_classification)
          : OmniboxFieldTrial::internal::GetValueForRuleInContext(
                kDemoteByTypeRule, current_page_classification);
  // If there is no demotion rule for this context, then use the default
  // value for that context.
  if (demotion_rule.empty()) {
    // This rule demotes URLs as strongly as possible without violating user
    // expectations.  In particular, for URL-seeking inputs, if the user would
    // likely expect a URL first (i.e., it would be inline autocompleted), then
    // that URL will still score strongly enough to be first.  This is done
    // using a demotion multipler of 0.61.  If a URL would get a score high
    // enough to be inline autocompleted (1400+), even after demotion it will
    // score above 850 ( 1400 * 0.61 > 850).  850 is the maximum score for
    // queries when the input has been detected as URL-seeking.
#if defined(OS_ANDROID)
    if (current_page_classification == OmniboxEventProto::
        SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT)
      demotion_rule = "1:61,2:61,3:61,4:61,16:61,24:61";
#endif
    if (current_page_classification ==
        OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS)
      demotion_rule = "1:10,2:10,3:10,4:10,5:10,16:10,17:10,24:10";
  }

  // The value of the DemoteByType rule is a comma-separated list of
  // {ResultType + ":" + Number} where ResultType is an AutocompleteMatchType::
  // Type enum represented as an integer and Number is an integer number
  // between 0 and 100 inclusive.   Relevance scores of matches of that result
  // type are multiplied by Number / 100.  100 means no change.
  base::StringPairs kv_pairs;
  if (base::SplitStringIntoKeyValuePairs(demotion_rule, ':', ',', &kv_pairs)) {
    for (base::StringPairs::const_iterator it = kv_pairs.begin();
         it != kv_pairs.end(); ++it) {
      // This is a best-effort conversion; we trust the hand-crafted parameters
      // downloaded from the server to be perfect.  There's no need to handle
      // errors smartly.
      int k, v;
      base::StringToInt(it->first, &k);
      base::StringToInt(it->second, &v);
      (*demotions_by_type)[static_cast<AutocompleteMatchType::Type>(k)] =
          static_cast<float>(v) / 100.0f;
    }
  }
}

size_t OmniboxFieldTrial::GetProviderMaxMatches(
    AutocompleteProvider::Type provider) {
  size_t default_max_matches_per_provider = 3;

  std::string param_value = base::GetFieldTrialParamValueByFeature(
      omnibox::kUIExperimentMaxAutocompleteMatches,
      OmniboxFieldTrial::kUIMaxAutocompleteMatchesByProviderParam);

  // If the experiment param specifies a max results for |provider|, return the
  // specified limit.
  // E.g., if param_value = '3:2' and provider = 3, return 2.
  // Otherwise, if the experiment param specifies a default value for
  // unspecified providers, return the default value.
  // E.g., if param_value = '3:3,*:4' and provider = 1, return 4,
  // Otherwise, return |default_max_matches_per_provider|.
  base::StringPairs kv_pairs;
  if (base::SplitStringIntoKeyValuePairs(param_value, ':', ',', &kv_pairs)) {
    for (const auto& kv_pair : kv_pairs) {
      int k;
      base::StringToInt(kv_pair.first, &k);
      size_t v;
      base::StringToSizeT(kv_pair.second, &v);

      if (kv_pair.first == "*")
        default_max_matches_per_provider = v;
      else if (k == provider)
        return v;
    }
  }

  return default_max_matches_per_provider;
}

void OmniboxFieldTrial::GetDefaultHUPScoringParams(
    HUPScoringParams* scoring_params) {
  ScoreBuckets* type_score_buckets = &scoring_params->typed_count_buckets;
  type_score_buckets->set_half_life_days(30);
  type_score_buckets->set_use_decay_factor(false);
  // Default typed count buckets based on decayed typed count. The
  // values here are based on the results of field trials to determine what
  // maximized overall result quality.
  const std::string& typed_count_score_buckets_str =
    "1.0:1413,0.97:1390,0.93:1360,0.85:1340,0.72:1320,0.50:1250,0.0:1203";
  InitializeBucketsFromString(typed_count_score_buckets_str,
                              type_score_buckets);

  ScoreBuckets* visit_score_buckets = &scoring_params->visited_count_buckets;
  visit_score_buckets->set_half_life_days(30);
  visit_score_buckets->set_use_decay_factor(false);
  // Buckets based on visit count.  Like the typed count buckets above, the
  // values here were chosen based on field trials.  Note that when a URL hasn't
  // been visited in the last 30 days, we clamp its score to 100, which
  // basically demotes it below any other results in the dropdown.
  const std::string& visit_count_score_buckets_str = "4.0:790,0.5:590,0.0:100";
  InitializeBucketsFromString(visit_count_score_buckets_str,
                              visit_score_buckets);
}

void OmniboxFieldTrial::GetExperimentalHUPScoringParams(
    HUPScoringParams* scoring_params) {
  VariationParams params;
  if (!variations::GetVariationParams(kBundledExperimentFieldTrialName,
                                      &params))
    return;

  InitializeScoreBuckets(params, kHUPNewScoringTypedCountRelevanceCapParam,
      kHUPNewScoringTypedCountHalfLifeTimeParam,
      kHUPNewScoringTypedCountScoreBucketsParam,
      kHUPNewScoringTypedCountUseDecayFactorParam,
      &scoring_params->typed_count_buckets);
  InitializeScoreBuckets(params, kHUPNewScoringVisitedCountRelevanceCapParam,
      kHUPNewScoringVisitedCountHalfLifeTimeParam,
      kHUPNewScoringVisitedCountScoreBucketsParam,
      kHUPNewScoringVisitedCountUseDecayFactorParam,
      &scoring_params->visited_count_buckets);
}

float OmniboxFieldTrial::HQPBookmarkValue() {
  std::string bookmark_value_str =
      variations::GetVariationParamValue(kBundledExperimentFieldTrialName,
                                         kHQPBookmarkValueRule);
  if (bookmark_value_str.empty())
    return 10;
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  double bookmark_value;
  base::StringToDouble(bookmark_value_str, &bookmark_value);
  return bookmark_value;
}

bool OmniboxFieldTrial::HQPAllowMatchInTLDValue() {
  return variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kHQPAllowMatchInTLDRule) == "true";
}

bool OmniboxFieldTrial::HQPAllowMatchInSchemeValue() {
  return variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kHQPAllowMatchInSchemeRule) == "true";
}

void OmniboxFieldTrial::GetSuggestPollingStrategy(bool* from_last_keystroke,
                                                  int* polling_delay_ms) {
  *from_last_keystroke = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kMeasureSuggestPollingDelayFromLastKeystrokeRule) == "true";

  const std::string& polling_delay_string = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kSuggestPollingDelayMsRule);
  if (polling_delay_string.empty() ||
      !base::StringToInt(polling_delay_string, polling_delay_ms) ||
      (*polling_delay_ms <= 0)) {
    *polling_delay_ms = kDefaultMinimumTimeBetweenSuggestQueriesMs;
  }
}

std::string OmniboxFieldTrial::HQPExperimentalScoringBuckets() {
  return variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kHQPExperimentalScoringBucketsParam);
}

float OmniboxFieldTrial::HQPExperimentalTopicalityThreshold() {
  std::string topicality_threshold_str = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kHQPExperimentalScoringTopicalityThresholdParam);

  double topicality_threshold;
  if (topicality_threshold_str.empty() ||
      !base::StringToDouble(topicality_threshold_str, &topicality_threshold))
    return 0.5f;

  return static_cast<float>(topicality_threshold);
}

int OmniboxFieldTrial::MaxNumHQPUrlsIndexedAtStartup() {
  const char* param = kMaxNumHQPUrlsIndexedAtStartupOnNonLowEndDevicesParam;
  const bool is_low_end_device = base::SysInfo::IsLowEndDevice();
  if (is_low_end_device)
    param = kMaxNumHQPUrlsIndexedAtStartupOnLowEndDevicesParam;
  std::string param_value(variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName, param));
  int num_urls;
  if (base::StringToInt(param_value, &num_urls))
    return num_urls;

#if defined(OS_ANDROID)
  // Limits on Android are chosen based on experiment results. See
  // crbug.com/715852#c18.
  constexpr int kMaxNumHQPUrlsIndexedAtStartupOnLowEndDevices = 100;
  constexpr int kMaxNumHQPUrlsIndexedAtStartupOnNonLowEndDevices = 1000;
  if (is_low_end_device)
    return kMaxNumHQPUrlsIndexedAtStartupOnLowEndDevices;
  return kMaxNumHQPUrlsIndexedAtStartupOnNonLowEndDevices;
#else
  // Default value is set to -1 for unlimited number of urls.
  return -1;
#endif  // defined(OS_ANDROID)
}

size_t OmniboxFieldTrial::HQPMaxVisitsToScore() {
  std::string max_visits_str = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName, kHQPMaxVisitsToScoreRule);
  constexpr size_t kDefaultMaxVisitsToScore = 10;
  static_assert(
      URLIndexPrivateData::kMaxVisitsToStoreInCache >= kDefaultMaxVisitsToScore,
      "HQP should store at least as many visits as it expects to score");
  if (max_visits_str.empty())
    return kDefaultMaxVisitsToScore;
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  size_t max_visits_value;
  base::StringToSizeT(max_visits_str, &max_visits_value);
  return max_visits_value;
}

float OmniboxFieldTrial::HQPTypedValue() {
  std::string typed_value_str = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName, kHQPTypedValueRule);
  if (typed_value_str.empty())
    return 1.5;
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  double typed_value;
  base::StringToDouble(typed_value_str, &typed_value);
  return typed_value;
}

OmniboxFieldTrial::NumMatchesScores OmniboxFieldTrial::HQPNumMatchesScores() {
  std::string str = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName, kHQPNumMatchesScoresRule);
  static constexpr char kDefaultNumMatchesScores[] = "1:3,2:2.5,3:2,4:1.5";
  if (str.empty())
    str = kDefaultNumMatchesScores;
  // The parameter is a comma-separated list of (number, value) pairs such as
  // listed above.
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need to handle
  // errors smartly.
  base::StringPairs kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(str, ':', ',', &kv_pairs))
    return NumMatchesScores{};
  NumMatchesScores num_matches_scores(kv_pairs.size());
  for (size_t i = 0; i < kv_pairs.size(); ++i) {
    base::StringToSizeT(kv_pairs[i].first, &num_matches_scores[i].first);
    // The input must be sorted by number of matches.
    DCHECK((i == 0) ||
           (num_matches_scores[i].first > num_matches_scores[i - 1].first));
    base::StringToDouble(kv_pairs[i].second, &num_matches_scores[i].second);
  }
  return num_matches_scores;
}

size_t OmniboxFieldTrial::HQPNumTitleWordsToAllow() {
  // The value of the rule is a string that encodes an integer (actually
  // size_t) containing the number of words.
  size_t num_title_words;
  if (!base::StringToSizeT(
          variations::GetVariationParamValue(kBundledExperimentFieldTrialName,
                                             kHQPNumTitleWordsRule),
          &num_title_words))
    return 20;
  return num_title_words;
}

bool OmniboxFieldTrial::HQPAlsoDoHUPLikeScoring() {
  return variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kHQPAlsoDoHUPLikeScoringRule) == "true";
}

bool OmniboxFieldTrial::HUPSearchDatabase() {
  const std::string& value = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kHUPSearchDatabaseRule);
  return value.empty() || (value == "true");
}

bool OmniboxFieldTrial::KeywordRequiresPrefixMatch() {
  const std::string& value = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kKeywordRequiresPrefixMatchRule);
  return value.empty() || (value == "true");
}

int OmniboxFieldTrial::KeywordScoreForSufficientlyCompleteMatch() {
  std::string value_str = variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      kKeywordScoreForSufficientlyCompleteMatchRule);
  if (value_str.empty())
    return -1;
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  int value;
  base::StringToInt(value_str, &value);
  return value;
}

OmniboxFieldTrial::EmphasizeTitlesCondition
OmniboxFieldTrial::GetEmphasizeTitlesConditionForInput(
    const AutocompleteInput& input) {
  if (base::FeatureList::IsEnabled(omnibox::kUIExperimentSwapTitleAndUrl)) {
    return EMPHASIZE_WHEN_NONEMPTY;
  }

  // Touch-optimized UI always swaps title and URL.
  if (ui::MaterialDesignController::touch_ui())
    return EMPHASIZE_WHEN_NONEMPTY;

  // Look up the parameter named kEmphasizeTitlesRule + "_" + input.type(),
  // find its value, and return that value as an enum.  If the parameter
  // isn't redefined, fall back to the generic rule kEmphasizeTitlesRule + "_*"
  std::string value_str(variations::GetVariationParamValue(
      kBundledExperimentFieldTrialName,
      std::string(kEmphasizeTitlesRule) + "_" +
          base::NumberToString(static_cast<int>(input.type()))));
  if (value_str.empty()) {
    value_str = variations::GetVariationParamValue(
        kBundledExperimentFieldTrialName,
        std::string(kEmphasizeTitlesRule) + "_*");
  }
  if (value_str.empty())
    return EMPHASIZE_NEVER;
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  int value;
  base::StringToInt(value_str, &value);
  return static_cast<EmphasizeTitlesCondition>(value);
}

size_t OmniboxFieldTrial::GetMaxURLMatches() {
  constexpr size_t kDefaultMaxURLMatches = 7;
  return base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kOmniboxMaxURLMatches,
      OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, kDefaultMaxURLMatches);
}

bool OmniboxFieldTrial::IsPreserveDefaultMatchScoreEnabled() {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxPreserveDefaultMatchScore);
}

bool OmniboxFieldTrial::IsReverseAnswersEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxReverseAnswers);
}

bool OmniboxFieldTrial::IsShortBookmarkSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(
             omnibox::kOmniboxShortBookmarkSuggestions) ||
         base::FeatureList::IsEnabled(omnibox::kAutocompleteTitles);
}

bool OmniboxFieldTrial::IsTabSwitchSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxTabSwitchSuggestions);
}

bool OmniboxFieldTrial::IsTabSwitchSuggestionsDedicatedRowEnabled() {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxTabSwitchSuggestionsDedicatedRow);
}

bool OmniboxFieldTrial::IsLooseMaxLimitOnDedicatedRowsEnabled() {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxLooseMaxLimitOnDedicatedRows);
}

bool OmniboxFieldTrial::IsPedalSuggestionsEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxPedalSuggestions);
}

bool OmniboxFieldTrial::IsHideSteadyStateUrlSchemeEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kHideSteadyStateUrlScheme);
}

bool OmniboxFieldTrial::IsHideSteadyStateUrlTrivialSubdomainsEnabled() {
  return base::FeatureList::IsEnabled(
      omnibox::kHideSteadyStateUrlTrivialSubdomains);
}

bool OmniboxFieldTrial::IsExperimentalKeywordModeEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kExperimentalKeywordMode);
}

bool OmniboxFieldTrial::IsGroupSuggestionsBySearchVsUrlFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      omnibox::kOmniboxGroupSuggestionsBySearchVsUrl);
}

bool OmniboxFieldTrial::IsMaxURLMatchesFeatureEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxMaxURLMatches);
}

const char OmniboxFieldTrial::kBundledExperimentFieldTrialName[] =
    "OmniboxBundledExperimentV1";
const char OmniboxFieldTrial::kDisableProvidersRule[] = "DisableProviders";
const char OmniboxFieldTrial::kShortcutsScoringMaxRelevanceRule[] =
    "ShortcutsScoringMaxRelevance";
const char OmniboxFieldTrial::kSearchHistoryRule[] = "SearchHistory";
const char OmniboxFieldTrial::kDemoteByTypeRule[] = "DemoteByType";
const char OmniboxFieldTrial::kHQPBookmarkValueRule[] =
    "HQPBookmarkValue";
const char OmniboxFieldTrial::kHQPTypedValueRule[] = "HQPTypedValue";
const char OmniboxFieldTrial::kHQPAllowMatchInTLDRule[] = "HQPAllowMatchInTLD";
const char OmniboxFieldTrial::kHQPAllowMatchInSchemeRule[] =
    "HQPAllowMatchInScheme";
const char OmniboxFieldTrial::kZeroSuggestVariantRule[] = "ZeroSuggestVariant";
const char
OmniboxFieldTrial::kMeasureSuggestPollingDelayFromLastKeystrokeRule[] =
    "MeasureSuggestPollingDelayFromLastKeystroke";
const char OmniboxFieldTrial::kSuggestPollingDelayMsRule[] =
    "SuggestPollingDelayMs";
const char OmniboxFieldTrial::kHQPMaxVisitsToScoreRule[] =
    "HQPMaxVisitsToScoreRule";
const char OmniboxFieldTrial::kHQPNumMatchesScoresRule[] =
    "HQPNumMatchesScores";
const char OmniboxFieldTrial::kHQPNumTitleWordsRule[] = "HQPNumTitleWords";
const char OmniboxFieldTrial::kHQPAlsoDoHUPLikeScoringRule[] =
    "HQPAlsoDoHUPLikeScoring";
const char OmniboxFieldTrial::kHUPSearchDatabaseRule[] =
    "HUPSearchDatabase";
const char OmniboxFieldTrial::kKeywordRequiresRegistryRule[] =
    "KeywordRequiresRegistry";
const char OmniboxFieldTrial::kKeywordRequiresPrefixMatchRule[] =
    "KeywordRequiresPrefixMatch";
const char OmniboxFieldTrial::kKeywordScoreForSufficientlyCompleteMatchRule[] =
    "KeywordScoreForSufficientlyCompleteMatch";
const char OmniboxFieldTrial::kEmphasizeTitlesRule[] = "EmphasizeTitles";

const char OmniboxFieldTrial::kOmniboxMaxURLMatchesParam[] =
    "OmniboxMaxURLMatches";

const char OmniboxFieldTrial::kHUPNewScoringTypedCountRelevanceCapParam[] =
    "TypedCountRelevanceCap";
const char OmniboxFieldTrial::kHUPNewScoringTypedCountHalfLifeTimeParam[] =
    "TypedCountHalfLifeTime";
const char OmniboxFieldTrial::kHUPNewScoringTypedCountScoreBucketsParam[] =
    "TypedCountScoreBuckets";
const char OmniboxFieldTrial::kHUPNewScoringTypedCountUseDecayFactorParam[] =
    "TypedCountUseDecayFactor";
const char OmniboxFieldTrial::kHUPNewScoringVisitedCountRelevanceCapParam[] =
    "VisitedCountRelevanceCap";
const char OmniboxFieldTrial::kHUPNewScoringVisitedCountHalfLifeTimeParam[] =
    "VisitedCountHalfLifeTime";
const char OmniboxFieldTrial::kHUPNewScoringVisitedCountScoreBucketsParam[] =
    "VisitedCountScoreBuckets";
const char OmniboxFieldTrial::kHUPNewScoringVisitedCountUseDecayFactorParam[] =
    "VisitedCountUseDecayFactor";

const char OmniboxFieldTrial::kHQPExperimentalScoringBucketsParam[] =
    "HQPExperimentalScoringBuckets";
const char
    OmniboxFieldTrial::kHQPExperimentalScoringTopicalityThresholdParam[] =
      "HQPExperimentalScoringTopicalityThreshold";

const char
    OmniboxFieldTrial::kMaxNumHQPUrlsIndexedAtStartupOnLowEndDevicesParam[] =
        "MaxNumHQPUrlsIndexedAtStartupOnLowEndDevices";
const char
    OmniboxFieldTrial::kMaxNumHQPUrlsIndexedAtStartupOnNonLowEndDevicesParam[] =
        "MaxNumHQPUrlsIndexedAtStartupOnNonLowEndDevices";

const char OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam[] =
    "UIMaxAutocompleteMatches";
const char OmniboxFieldTrial::kUIMaxAutocompleteMatchesByProviderParam[] =
    "UIMaxAutocompleteMatchesByProvider";

// static
int OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs = 100;

std::string OmniboxFieldTrial::internal::GetValueForRuleInContext(
    const std::string& rule,
    OmniboxEventProto::PageClassification page_classification) {
  VariationParams params;
  if (!base::GetFieldTrialParams(kBundledExperimentFieldTrialName, &params))
    return std::string();

  return GetValueForRuleInContextFromVariationParams(params, rule,
                                                     page_classification);
}

std::string OmniboxFieldTrial::internal::GetValueForRuleInContextByFeature(
    const base::Feature& feature,
    const std::string& rule,
    metrics::OmniboxEventProto::PageClassification page_classification) {
  VariationParams params;
  if (!base::GetFieldTrialParamsByFeature(feature, &params))
    return std::string();

  return GetValueForRuleInContextFromVariationParams(params, rule,
                                                     page_classification);
}
