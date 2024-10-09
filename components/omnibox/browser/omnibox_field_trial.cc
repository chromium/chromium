// Copyright 2014 The Chromium Authors
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
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/url_index_private_data.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search/search.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/variations_associated_data.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/ui_base_features.h"

using metrics::OmniboxEventProto;

namespace {

typedef std::map<std::string, std::string> VariationParams;
typedef HUPScoringParams::ScoreBuckets ScoreBuckets;

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
    std::sort(score_buckets->buckets().begin(), score_buckets->buckets().end(),
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
    if (base::StringToInt(it->second, &relevance_cap)) {
      score_buckets->set_relevance_cap(relevance_cap);
    }
  }

  it = params.find(use_decay_factor_param);
  if (it != params.end()) {
    int use_decay_factor;
    if (base::StringToInt(it->second, &use_decay_factor)) {
      score_buckets->set_use_decay_factor(use_decay_factor != 0);
    }
  }

  it = params.find(half_life_param);
  if (it != params.end()) {
    int half_life_days;
    if (base::StringToInt(it->second, &half_life_days)) {
      score_buckets->set_half_life_days(half_life_days);
    }
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
  if (params.empty()) {
    return std::string();
  }

  const std::string page_classification_str =
      base::NumberToString(static_cast<int>(page_classification));
  const std::string instant_extended =
      search::IsInstantExtendedAPIEnabled() ? "1" : "0";
  // Look up rule in this exact context.
  VariationParams::const_iterator it = params.find(
      rule + ":" + page_classification_str + ":" + instant_extended);
  if (it != params.end()) {
    return it->second;
  }
  // Fall back to the global page classification context.
  it = params.find(rule + ":*:" + instant_extended);
  if (it != params.end()) {
    return it->second;
  }
  // Fall back to the global instant extended context.
  it = params.find(rule + ":" + page_classification_str + ":*");
  if (it != params.end()) {
    return it->second;
  }
  // Look up rule in the global context.
  it = params.find(rule + ":*:*");
  return (it != params.end()) ? it->second : std::string();
}

OmniboxFieldTrial::MLConfig& GetMLConfigInternal() {
  static base::NoDestructor<OmniboxFieldTrial::MLConfig> s_config;
  return *s_config;
}

bool IsKoreanLocale(const std::string& locale) {
  return locale == "ko" || locale == "ko-KR";
}

}  // namespace

HUPScoringParams::ScoreBuckets::ScoreBuckets()
    : relevance_cap_(-1), half_life_days_(-1), use_decay_factor_(false) {}

HUPScoringParams::ScoreBuckets::ScoreBuckets(const ScoreBuckets& other) =
    default;

HUPScoringParams::ScoreBuckets::~ScoreBuckets() {}

size_t HUPScoringParams::ScoreBuckets::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(buckets_);
}

double HUPScoringParams::ScoreBuckets::HalfLifeTimeDecay(
    const base::TimeDelta& elapsed_time) const {
  double time_ms;
  if ((half_life_days_ <= 0) ||
      ((time_ms = elapsed_time.InMillisecondsF()) <= 0)) {
    return 1.0;
  }

  const double half_life_intervals =
      time_ms / base::Days(half_life_days_).InMillisecondsF();
  return pow(2.0, -half_life_intervals);
}

size_t HUPScoringParams::EstimateMemoryUsage() const {
  size_t res = 0;

  res += base::trace_event::EstimateMemoryUsage(typed_count_buckets);
  res += base::trace_event::EstimateMemoryUsage(visited_count_buckets);

  return res;
}

int OmniboxFieldTrial::GetDisabledProviderTypes() {
  const std::string& types_string = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName, kDisableProvidersRule);
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
#if BUILDFLAG(IS_ANDROID)
    if (current_page_classification ==
        OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT) {
      demotion_rule = "1:61,2:61,3:61,4:61,16:61,24:61";
    }
#endif
    omnibox::CheckObsoletePageClass(current_page_classification);

    if (current_page_classification == OmniboxEventProto::NTP_REALBOX) {
      demotion_rule = "1:10,2:10,3:10,4:10,5:10,16:10,17:10,24:10";
    }
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

  std::string param_value;
  if (OmniboxFieldTrial::IsMlUrlScoringEnabled()) {
    param_value =
        OmniboxFieldTrial::GetMLConfig().ml_url_scoring_max_matches_by_provider;
  } else {
    param_value = base::GetFieldTrialParamValueByFeature(
        omnibox::kUIExperimentMaxAutocompleteMatches,
        OmniboxFieldTrial::kUIMaxAutocompleteMatchesByProviderParam);
  }

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

      if (kv_pair.first == "*") {
        default_max_matches_per_provider = v;
      } else if (k == provider) {
        return v;
      }
    }
  }

  return default_max_matches_per_provider;
}

bool OmniboxFieldTrial::IsMaxURLMatchesFeatureEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kOmniboxMaxURLMatches);
}

size_t OmniboxFieldTrial::GetMaxURLMatches() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  constexpr size_t kDefaultMaxURLMatches = 5;
#else
  constexpr size_t kDefaultMaxURLMatches = 7;
#endif
  return base::GetFieldTrialParamByFeatureAsInt(
      omnibox::kOmniboxMaxURLMatches,
      OmniboxFieldTrial::kOmniboxMaxURLMatchesParam, kDefaultMaxURLMatches);
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
  if (!base::GetFieldTrialParams(kBundledExperimentFieldTrialName, &params)) {
    return;
  }

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
  std::string bookmark_value_str = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName, kHQPBookmarkValueRule);
  if (bookmark_value_str.empty()) {
    return 10;
  }
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  double bookmark_value;
  base::StringToDouble(bookmark_value_str, &bookmark_value);
  return bookmark_value;
}

bool OmniboxFieldTrial::HQPAllowMatchInTLDValue() {
  return base::GetFieldTrialParamValue(kBundledExperimentFieldTrialName,
                                       kHQPAllowMatchInTLDRule) == "true";
}

bool OmniboxFieldTrial::HQPAllowMatchInSchemeValue() {
  return base::GetFieldTrialParamValue(kBundledExperimentFieldTrialName,
                                       kHQPAllowMatchInSchemeRule) == "true";
}

void OmniboxFieldTrial::GetSuggestPollingStrategy(bool* from_last_keystroke,
                                                  int* polling_delay_ms) {
  *from_last_keystroke =
      base::GetFieldTrialParamValue(
          kBundledExperimentFieldTrialName,
          kMeasureSuggestPollingDelayFromLastKeystrokeRule) == "true";

  const std::string& polling_delay_string = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName, kSuggestPollingDelayMsRule);
  if (polling_delay_string.empty() ||
      !base::StringToInt(polling_delay_string, polling_delay_ms) ||
      (*polling_delay_ms <= 0)) {
    *polling_delay_ms = kDefaultMinimumTimeBetweenSuggestQueriesMs;
  }
}

std::string OmniboxFieldTrial::HQPExperimentalScoringBuckets() {
  return base::GetFieldTrialParamValue(kBundledExperimentFieldTrialName,
                                       kHQPExperimentalScoringBucketsParam);
}

float OmniboxFieldTrial::HQPExperimentalTopicalityThreshold() {
  std::string topicality_threshold_str = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName,
      kHQPExperimentalScoringTopicalityThresholdParam);

  double topicality_threshold;
  if (topicality_threshold_str.empty() ||
      !base::StringToDouble(topicality_threshold_str, &topicality_threshold)) {
    return 0.5f;
  }

  return static_cast<float>(topicality_threshold);
}

int OmniboxFieldTrial::MaxNumHQPUrlsIndexedAtStartup() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // Limits on Android and iOS are chosen based on experiment results. See
  // crbug.com/715852#c18 and crbug.com/1141539#c31.
  constexpr int kDefaultOnLowEndDevices = 100;
  constexpr int kDefaultOnNonLowEndDevices = 400;
#else
  // Use 20,000 entries as a safety cap for users with spammed history,
  // such as users who were stuck in a redirect loop with autogenerated URLs.
  // This limit will only affect 0.01% of Windows users. crbug.com/750845.
  constexpr int kDefaultOnLowEndDevices = 20000;
  constexpr int kDefaultOnNonLowEndDevices = 20000;
#endif

  if (base::SysInfo::IsLowEndDeviceOrPartialLowEndModeEnabled()) {
    return kDefaultOnLowEndDevices;
  } else {
    return kDefaultOnNonLowEndDevices;
  }
}

size_t OmniboxFieldTrial::HQPMaxVisitsToScore() {
  std::string max_visits_str = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName, kHQPMaxVisitsToScoreRule);
  constexpr size_t kDefaultMaxVisitsToScore = 10;
  static_assert(
      URLIndexPrivateData::kMaxVisitsToStoreInCache >= kDefaultMaxVisitsToScore,
      "HQP should store at least as many visits as it expects to score");
  if (max_visits_str.empty()) {
    return kDefaultMaxVisitsToScore;
  }
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  size_t max_visits_value;
  base::StringToSizeT(max_visits_str, &max_visits_value);
  return max_visits_value;
}

float OmniboxFieldTrial::HQPTypedValue() {
  std::string typed_value_str = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName, kHQPTypedValueRule);
  if (typed_value_str.empty()) {
    return 1.5;
  }
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need for handle
  // errors smartly.
  double typed_value;
  base::StringToDouble(typed_value_str, &typed_value);
  return typed_value;
}

OmniboxFieldTrial::NumMatchesScores OmniboxFieldTrial::HQPNumMatchesScores() {
  std::string str = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName, kHQPNumMatchesScoresRule);
  static constexpr char kDefaultNumMatchesScores[] = "1:3,2:2.5,3:2,4:1.5";
  if (str.empty()) {
    str = kDefaultNumMatchesScores;
  }
  // The parameter is a comma-separated list of (number, value) pairs such as
  // listed above.
  // This is a best-effort conversion; we trust the hand-crafted parameters
  // downloaded from the server to be perfect.  There's no need to handle
  // errors smartly.
  base::StringPairs kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(str, ':', ',', &kv_pairs)) {
    return NumMatchesScores{};
  }
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
          base::GetFieldTrialParamValue(kBundledExperimentFieldTrialName,
                                        kHQPNumTitleWordsRule),
          &num_title_words)) {
    return 20;
  }
  return num_title_words;
}

bool OmniboxFieldTrial::HQPAlsoDoHUPLikeScoring() {
  return base::GetFieldTrialParamValue(kBundledExperimentFieldTrialName,
                                       kHQPAlsoDoHUPLikeScoringRule) == "true";
}

bool OmniboxFieldTrial::HUPSearchDatabase() {
  const std::string& value = base::GetFieldTrialParamValue(
      kBundledExperimentFieldTrialName, kHUPSearchDatabaseRule);
  return value.empty() || (value == "true");
}

bool OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForIncognito() {
  return base::FeatureList::IsEnabled(omnibox::kOnDeviceHeadProviderIncognito);
}

bool OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForNonIncognito() {
  return base::FeatureList::IsEnabled(
      omnibox::kOnDeviceHeadProviderNonIncognito);
}

bool OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForAnyMode() {
  return IsOnDeviceHeadSuggestEnabledForIncognito() ||
         IsOnDeviceHeadSuggestEnabledForNonIncognito();
}

bool OmniboxFieldTrial::IsOnDeviceTailSuggestEnabled() {
  // Tail model will only be enabled when head provider is also enabled.
  return base::FeatureList::IsEnabled(omnibox::kOnDeviceTailModel) &&
         IsOnDeviceHeadSuggestEnabledForAnyMode();
}

bool OmniboxFieldTrial::ShouldEncodeLeadingSpaceForOnDeviceTailSuggest() {
  return base::GetFieldTrialParamByFeatureAsBool(omnibox::kOnDeviceTailModel,
                                                 "ShouldEncodeLeadingSpace",
                                                 /*default_value=*/false);
}

bool OmniboxFieldTrial::ShouldApplyOnDeviceHeadModelSelectionFix() {
  return base::GetFieldTrialParamByFeatureAsBool(
             omnibox::kOnDeviceHeadProviderNonIncognito,
             OmniboxFieldTrial::kOnDeviceHeadModelSelectionFix,
             /*default_value=*/false) ||
         base::GetFieldTrialParamByFeatureAsBool(
             omnibox::kOnDeviceHeadProviderIncognito,
             OmniboxFieldTrial::kOnDeviceHeadModelSelectionFix,
             /*default_value=*/false);
}

bool OmniboxFieldTrial::IsOnDeviceHeadSuggestEnabledForLocale(
    const std::string& locale) {
  if (IsKoreanLocale(locale) &&
      !base::FeatureList::IsEnabled(omnibox::kOnDeviceHeadProviderKorean)) {
    return false;
  }
  return IsOnDeviceHeadSuggestEnabledForAnyMode();
}

std::string OmniboxFieldTrial::OnDeviceHeadModelLocaleConstraint(
    bool is_incognito) {
  const base::Feature* feature =
      is_incognito ? &omnibox::kOnDeviceHeadProviderIncognito
                   : &omnibox::kOnDeviceHeadProviderNonIncognito;
  std::string constraint = base::GetFieldTrialParamValueByFeature(
      *feature, kOnDeviceHeadModelLocaleConstraint);
  if (constraint.empty()) {
    constraint = "500000";
  }
  return constraint;
}

const char OmniboxFieldTrial::kBundledExperimentFieldTrialName[] =
    "OmniboxBundledExperimentV1";
const char OmniboxFieldTrial::kDisableProvidersRule[] = "DisableProviders";
const char OmniboxFieldTrial::kDemoteByTypeRule[] = "DemoteByType";
const char OmniboxFieldTrial::kHQPBookmarkValueRule[] = "HQPBookmarkValue";
const char OmniboxFieldTrial::kHQPTypedValueRule[] = "HQPTypedValue";
const char OmniboxFieldTrial::kHQPAllowMatchInTLDRule[] = "HQPAllowMatchInTLD";
const char OmniboxFieldTrial::kHQPAllowMatchInSchemeRule[] =
    "HQPAllowMatchInScheme";
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
const char OmniboxFieldTrial::kHUPSearchDatabaseRule[] = "HUPSearchDatabase";
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

const char OmniboxFieldTrial::kMaxZeroSuggestMatchesParam[] =
    "MaxZeroSuggestMatches";
const char OmniboxFieldTrial::kOmniboxMaxURLMatchesParam[] =
    "OmniboxMaxURLMatches";
const char OmniboxFieldTrial::kUIMaxAutocompleteMatchesByProviderParam[] =
    "UIMaxAutocompleteMatchesByProvider";
const char OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam[] =
    "UIMaxAutocompleteMatches";
const char OmniboxFieldTrial::kDynamicMaxAutocompleteUrlCutoffParam[] =
    "OmniboxDynamicMaxAutocompleteUrlCutoff";
const char OmniboxFieldTrial::kDynamicMaxAutocompleteIncreasedLimitParam[] =
    "OmniboxDynamicMaxAutocompleteIncreasedLimit";

const char OmniboxFieldTrial::kOnDeviceHeadModelLocaleConstraint[] =
    "ForceModelLocaleConstraint";
const char OmniboxFieldTrial::kOnDeviceHeadModelSelectionFix[] = "SelectionFix";

int OmniboxFieldTrial::kDefaultMinimumTimeBetweenSuggestQueriesMs = 100;

namespace OmniboxFieldTrial {

// Local history zero-prefix (aka zero-suggest) and prefix suggestions:

// The debouncing delay (in milliseconds) to use when throttling ZPS prefetch
// requests.
const base::FeatureParam<int> kZeroSuggestPrefetchDebounceDelay(
    &omnibox::kZeroSuggestPrefetchDebouncing,
    "ZeroSuggestPrefetchDebounceDelay",
    300);

// Whether to calculate debouncing delay relative to the latest successful run
// (instead of the latest run request).
const base::FeatureParam<bool> kZeroSuggestPrefetchDebounceFromLastRun(
    &omnibox::kZeroSuggestPrefetchDebouncing,
    "ZeroSuggestPrefetchDebounceFromLastRun",
    true);

// The maximum number of entries stored by the in-memory zero-suggest cache at
// at any given time (LRU eviction policy is used to enforce this limit).
const base::FeatureParam<int> kZeroSuggestCacheMaxSize(
    &omnibox::kZeroSuggestInMemoryCaching,
    "ZeroSuggestCacheMaxSize",
    5);

// The relevance score for remote zero-suggest ranges from 550-1400. A default
// value of 500 places local history zero-suggest below the remote zero-suggest.
const base::FeatureParam<int> kLocalHistoryZeroSuggestRelevanceScore(
    &omnibox::kAdjustLocalHistoryZeroSuggestRelevanceScore,
    "LocalHistoryZeroSuggestRelevanceScore",
    500);

bool IsZeroSuggestPrefetchingEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetching) ||
         base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetchingOnSRP) ||
         base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetchingOnWeb);
}

bool IsZeroSuggestPrefetchingEnabledInContext(
    metrics::OmniboxEventProto::PageClassification page_classification) {
  switch (page_classification) {
    case metrics::OmniboxEventProto::NTP_ZPS_PREFETCH:
      return base::FeatureList::IsEnabled(omnibox::kZeroSuggestPrefetching);
    case metrics::OmniboxEventProto::SRP_ZPS_PREFETCH:
      return base::FeatureList::IsEnabled(
          omnibox::kZeroSuggestPrefetchingOnSRP);
    case metrics::OmniboxEventProto::OTHER_ZPS_PREFETCH:
      return base::FeatureList::IsEnabled(
          omnibox::kZeroSuggestPrefetchingOnWeb);
    default:
      return false;
  }
}

// Rich autocompletion.

bool IsRichAutocompletionEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kRichAutocompletion);
}

bool RichAutocompletionShowAdditionalText() {
  return IsRichAutocompletionEnabled() &&
         kRichAutocompletionShowAdditionalText.Get();
}

const base::FeatureParam<bool> kRichAutocompletionAutocompleteTitles(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionAutocompleteTitles",
    false);

const base::FeatureParam<bool>
    kRichAutocompletionAutocompleteTitlesShortcutProvider(
        &omnibox::kRichAutocompletion,
        "RichAutocompletionAutocompleteTitlesShortcutProvider",
        true);

const base::FeatureParam<int> kRichAutocompletionAutocompleteTitlesMinChar(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionAutocompleteTitlesMinChar",
    3);

const base::FeatureParam<bool> kRichAutocompletionAutocompleteNonPrefixAll(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionAutocompleteNonPrefixAll",
    false);

const base::FeatureParam<bool>
    kRichAutocompletionAutocompleteNonPrefixShortcutProvider(
        &omnibox::kRichAutocompletion,
        "RichAutocompletionAutocompleteNonPrefixShortcutProvider",
        false);

const base::FeatureParam<int> kRichAutocompletionAutocompleteNonPrefixMinChar(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionAutocompleteNonPrefixMinChar",
    0);

const base::FeatureParam<bool> kRichAutocompletionShowAdditionalText(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionAutocompleteShowAdditionalText",
    true);

const base::FeatureParam<bool> kRichAutocompletionAdditionalTextWithParenthesis(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionAdditionalTextWithParenthesis",
    false);

const base::FeatureParam<bool> kRichAutocompletionAutocompleteShortcutText(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionAutocompleteShortcutText",
    true);

const base::FeatureParam<int>
    kRichAutocompletionAutocompleteShortcutTextMinChar(
        &omnibox::kRichAutocompletion,
        "RichAutocompletionAutocompleteShortcutTextMinChar",
        3);

const base::FeatureParam<bool> kRichAutocompletionCounterfactual(
    &omnibox::kRichAutocompletion,
    "RichAutocompletionCounterfactual",
    false);

const base::FeatureParam<bool>
    kRichAutocompletionAutocompletePreferUrlsOverPrefixes(
        &omnibox::kRichAutocompletion,
        "RichAutocompletionAutocompletePreferUrlsOverPrefixes",
        false);

const base::FeatureParam<bool> kDomainSuggestionsCounterfactual(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsCounterfactual",
    false);

const base::FeatureParam<int> kDomainSuggestionsTypedUrlsThreshold(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsTypedUrlsThreshold",
    7);

const base::FeatureParam<int> kDomainSuggestionsTypedUrlsOffset(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsTypedUrlsOffset",
    1);

const base::FeatureParam<int> kDomainSuggestionsTypedVisitThreshold(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsTypedVisitThreshold",
    4);

const base::FeatureParam<int> kDomainSuggestionsTypedVisitOffset(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsTypedVisitOffset",
    1);

const base::FeatureParam<int> kDomainSuggestionsTypedVisitCapPerVisit(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsTypedVisitCapPerVisit",
    2);

const base::FeatureParam<int> kDomainSuggestionsMinInputLength(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsMinInputLength",
    4);

const base::FeatureParam<int> kDomainSuggestionsMaxMatchesPerDomain(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsMaxMatchesPerDomain",
    2);

const base::FeatureParam<double> kDomainSuggestionsScoreFactor(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsScoreFactor",
    1);

const base::FeatureParam<bool> kDomainSuggestionsAlternativeScoring(
    &omnibox::kDomainSuggestions,
    "DomainSuggestionsAlternativeScoring",
    false);

// ---------------------------------------------------------
// ML Relevance Scoring ->

// If true, enables scoring signal annotators for logging Omnibox scoring
// signals to OmniboxEventProto.
const base::FeatureParam<bool> kEnableScoringSignalsAnnotatorsForLogging(
    &omnibox::kLogUrlScoringSignals,
    "enable_scoring_signals_annotators",
    false);

// If true, enables scoring signal annotators for ML scoring.
const base::FeatureParam<bool> kEnableScoringSignalsAnnotatorsForMlScoring(
    &omnibox::kMlUrlScoring,
    "enable_scoring_signals_annotators_for_ml_scoring",
    true);

// If true, runs the ML scoring model but does not assign new relevance scores
// to the URL suggestions and does not rerank them.
const base::FeatureParam<bool> kMlUrlScoringCounterfactual(
    &omnibox::kMlUrlScoring,
    "MlUrlScoringCounterfactual",
    false);

// If true, increases the number of candidates the URL autocomplete providers
// pass to the controller beyond `provider_max_matches`.
const base::FeatureParam<bool> kMlUrlScoringUnlimitedNumCandidates(
    &omnibox::kMlUrlScoring,
    "MlUrlScoringUnlimitedNumCandidates",
    false);

const base::FeatureParam<std::string> kMlUrlScoringMaxMatchesByProvider(
    &omnibox::kMlUrlScoring,
    "MlUrlScoringMaxMatchesByProvider",
    "");

MLConfig::MLConfig() {
  log_url_scoring_signals =
      base::FeatureList::IsEnabled(omnibox::kLogUrlScoringSignals);
  enable_history_scoring_signals_annotator_for_searches =
      base::FeatureList::IsEnabled(
          omnibox::kEnableHistoryScoringSignalsAnnotatorForSearches);
  enable_scoring_signals_annotators =
      kEnableScoringSignalsAnnotatorsForLogging.Get() ||
      kEnableScoringSignalsAnnotatorsForMlScoring.Get();
  shortcut_document_signals =
      base::FeatureParam<bool>(&omnibox::kLogUrlScoringSignals,
                               "MlUrlScoringShortcutDocumentSignals", false)
          .Get() ||
      base::FeatureParam<bool>(&omnibox::kMlUrlScoring,
                               "MlUrlScoringShortcutDocumentSignals", true)
          .Get();

  ml_url_scoring = base::FeatureList::IsEnabled(omnibox::kMlUrlScoring);
  ml_url_scoring_counterfactual = kMlUrlScoringCounterfactual.Get();
  ml_url_scoring_unlimited_num_candidates =
      kMlUrlScoringUnlimitedNumCandidates.Get();
  ml_url_scoring_max_matches_by_provider =
      kMlUrlScoringMaxMatchesByProvider.Get();

  // `kMlUrlSearchBlending` parameters.
  mapped_search_blending =
      base::FeatureParam<bool>(&omnibox::kMlUrlSearchBlending,
                               "MlUrlSearchBlending_MappedSearchBlending",
                               mapped_search_blending)
          .Get();
  mapped_search_blending_min =
      base::FeatureParam<int>(&omnibox::kMlUrlSearchBlending,
                              "MlUrlSearchBlending_MappedSearchBlendingMin",
                              mapped_search_blending_min)
          .Get();
  mapped_search_blending_max =
      base::FeatureParam<int>(&omnibox::kMlUrlSearchBlending,
                              "MlUrlSearchBlending_MappedSearchBlendingMax",
                              mapped_search_blending_max)
          .Get();
  mapped_search_blending_grouping_threshold =
      base::FeatureParam<int>(
          &omnibox::kMlUrlSearchBlending,
          "MlUrlSearchBlending_MappedSearchBlendingGroupingThreshold",
          mapped_search_blending_grouping_threshold)
          .Get();

  // `kMlUrlPiecewiseMappedSearchBlending` parameters.
  piecewise_mapped_search_blending =
      base::FeatureParam<bool>(&omnibox::kMlUrlPiecewiseMappedSearchBlending,
                               "MlUrlPiecewiseMappedSearchBlending",
                               piecewise_mapped_search_blending)
          .Get();
  piecewise_mapped_search_blending_grouping_threshold =
      base::FeatureParam<int>(
          &omnibox::kMlUrlPiecewiseMappedSearchBlending,
          "MlUrlPiecewiseMappedSearchBlending_GroupingThreshold",
          piecewise_mapped_search_blending_grouping_threshold)
          .Get();
  piecewise_mapped_search_blending_break_points =
      base::FeatureParam<std::string>(
          &omnibox::kMlUrlPiecewiseMappedSearchBlending,
          "MlUrlPiecewiseMappedSearchBlending_BreakPoints", "")
          .Get();
  piecewise_mapped_search_blending_break_points_verbatim_url =
      base::FeatureParam<std::string>(
          &omnibox::kMlUrlPiecewiseMappedSearchBlending,
          "MlUrlPiecewiseMappedSearchBlending_BreakPoints_VerbatimUrl",
          piecewise_mapped_search_blending_break_points.c_str())
          .Get();
  piecewise_mapped_search_blending_break_points_search =
      base::FeatureParam<std::string>(
          &omnibox::kMlUrlPiecewiseMappedSearchBlending,
          "MlUrlPiecewiseMappedSearchBlending_BreakPoints_Search", "0,0;1,1400")
          .Get();
  piecewise_mapped_search_blending_relevance_bias =
      base::FeatureParam<int>(
          &omnibox::kMlUrlPiecewiseMappedSearchBlending,
          "MlUrlPiecewiseMappedSearchBlending_RelevanceBias",
          piecewise_mapped_search_blending_relevance_bias)
          .Get();

  url_scoring_model = base::FeatureList::IsEnabled(omnibox::kUrlScoringModel);

  ml_url_score_caching =
      base::FeatureList::IsEnabled(omnibox::kMlUrlScoreCaching);
  max_ml_score_cache_size =
      base::FeatureParam<int>(&omnibox::kMlUrlScoreCaching,
                              "MlUrlScoreCaching_MaxMlScoreCacheSize",
                              max_ml_score_cache_size)
          .Get();

  enable_ml_scoring_for_searches =
      base::FeatureParam<bool>(&omnibox::kMlUrlScoring,
                               "MlUrlScoring_EnableMlScoringForSearches", false)
          .Get();
  enable_ml_scoring_for_verbatim_urls =
      base::FeatureParam<bool>(&omnibox::kMlUrlScoring,
                               "MlUrlScoring_EnableMlScoringForVerbatimUrls",
                               false)
          .Get();
}

MLConfig::~MLConfig() = default;

MLConfig::MLConfig(const MLConfig&) = default;

MLConfig& MLConfig::operator=(const MLConfig& other) = default;

ScopedMLConfigForTesting::ScopedMLConfigForTesting()
    : original_config_(std::make_unique<MLConfig>(GetMLConfig())) {
  GetMLConfigInternal() = {};
}

ScopedMLConfigForTesting::~ScopedMLConfigForTesting() {
  GetMLConfigInternal() = *original_config_;
}

MLConfig& ScopedMLConfigForTesting::GetMLConfig() {
  return GetMLConfigInternal();
}

const MLConfig& GetMLConfig() {
  return GetMLConfigInternal();
}

bool IsReportingUrlScoringSignalsEnabled() {
  return GetMLConfig().log_url_scoring_signals;
}

bool IsPopulatingUrlScoringSignalsEnabled() {
  return IsReportingUrlScoringSignalsEnabled() || IsMlUrlScoringEnabled();
}

bool AreScoringSignalsAnnotatorsEnabled() {
  return GetMLConfig().enable_scoring_signals_annotators;
}
bool IsMlUrlScoringEnabled() {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  return IsUrlScoringModelEnabled() && GetMLConfig().ml_url_scoring;
#else
  return false;
#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}
bool IsMlUrlScoringCounterfactual() {
  return IsMlUrlScoringEnabled() && GetMLConfig().ml_url_scoring_counterfactual;
}
bool IsMlUrlScoringUnlimitedNumCandidatesEnabled() {
  return IsMlUrlScoringEnabled() &&
         GetMLConfig().ml_url_scoring_unlimited_num_candidates;
}
bool IsUrlScoringModelEnabled() {
  return GetMLConfig().url_scoring_model;
}
bool IsMlUrlScoreCachingEnabled() {
  return GetMLConfig().ml_url_score_caching;
}

std::vector<std::pair<double, int>> GetPiecewiseMappingBreakPoints(
    PiecewiseMappingVariant mapping_variant) {
  std::vector<std::pair<double, int>> break_points;

  std::string param_value;
  switch (mapping_variant) {
    case PiecewiseMappingVariant::kRegular:
      param_value = OmniboxFieldTrial::GetMLConfig()
                        .piecewise_mapped_search_blending_break_points;
      break;
    case PiecewiseMappingVariant::kVerbatimUrl:
      param_value =
          OmniboxFieldTrial::GetMLConfig()
              .piecewise_mapped_search_blending_break_points_verbatim_url;
      break;
    case PiecewiseMappingVariant::kSearch:
      param_value = OmniboxFieldTrial::GetMLConfig()
                        .piecewise_mapped_search_blending_break_points_search;
      break;
    default:
      NOTREACHED();
  }

  base::StringPairs pairs;
  if (base::SplitStringIntoKeyValuePairs(param_value, ',', ';', &pairs)) {
    for (const auto& p : pairs) {
      double ml_score;
      base::StringToDouble(p.first, &ml_score);
      int relevance;
      base::StringToInt(p.second, &relevance);

      break_points.push_back(std::make_pair(ml_score, relevance));
    }
  }

  return break_points;
}

// <- ML Relevance Scoring
// ---------------------------------------------------------
// Touch Down Trigger For Prefetch ->
const base::FeatureParam<int>
    kTouchDownTriggerForPrefetchMaxPrefetchesPerOmniboxSession(
        &omnibox::kOmniboxTouchDownTriggerForPrefetch,
        "max_prefetches_per_omnibox_session",
        5);
// <- Touch Down Trigger For Prefetch
// ---------------------------------------------------------
// Site Search Starter Pack ->
const base::FeatureParam<std::string> kGeminiUrlOverride(
    &omnibox::kStarterPackExpansion,
    "StarterPackGeminiUrlOverride",
    "https://gemini.google.com/prompt?"
    "utm_source=chrome_omnibox&utm_medium=owned&utm_campaign=gemini_shortcut");

bool IsStarterPackExpansionEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kStarterPackExpansion);
}

bool IsStarterPackIPHEnabled() {
  return base::FeatureList::IsEnabled(omnibox::kStarterPackIPH);
}
// <- Site Search Starter Pack
// ---------------------------------------------------------
// Featured Enterprise Site Search ->
bool IsFeaturedEnterpriseSearchIPHEnabled() {
  return base::FeatureList::IsEnabled(
      omnibox::kShowFeaturedEnterpriseSiteSearchIPH);
}

}  // namespace OmniboxFieldTrial

std::string OmniboxFieldTrial::internal::GetValueForRuleInContext(
    const std::string& rule,
    OmniboxEventProto::PageClassification page_classification) {
  VariationParams params;
  if (!base::GetFieldTrialParams(kBundledExperimentFieldTrialName, &params)) {
    return std::string();
  }

  return GetValueForRuleInContextFromVariationParams(params, rule,
                                                     page_classification);
}

std::string OmniboxFieldTrial::internal::GetValueForRuleInContextByFeature(
    const base::Feature& feature,
    const std::string& rule,
    metrics::OmniboxEventProto::PageClassification page_classification) {
  VariationParams params;
  if (!base::GetFieldTrialParamsByFeature(feature, &params)) {
    return std::string();
  }

  return GetValueForRuleInContextFromVariationParams(params, rule,
                                                     page_classification);
}
