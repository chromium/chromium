// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"

namespace optimization_guide {
namespace features {

// Enables the syncing of the Optimization Hints component, which provides
// hints for what Previews can be applied on a page load.
const base::Feature kOptimizationHints {
  "OptimizationHints",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

// Enables Optimization Hints that are marked as experimental. Optimizations are
// marked experimental by setting an experiment name in the "experiment_name"
// field of the Optimization proto. This allows experiments at the granularity
// of a single PreviewType for a single host (or host suffix). The intent is
// that optimizations that may not work properly for certain sites can be tried
// at a small scale via Finch experiments. Experimental optimizations can be
// activated by enabling this feature and passing an experiment name as a
// parameter called "experiment_name" that matches the experiment name in the
// Optimization proto.
const base::Feature kOptimizationHintsExperiments{
    "OptimizationHintsExperiments", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching optimization hints from a remote Optimization Guide Service.
const base::Feature kOptimizationHintsFetching{
  "OptimizationHintsFetching",
#if defined(OS_ANDROID)
      base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

const base::Feature kOptimizationHintsFetchingAnonymousDataConsent{
    "OptimizationHintsFetchingAnonymousDataConsent",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the prediction of optimization targets.
const base::Feature kOptimizationTargetPrediction{
    "OptimizationTargetPrediction", base::FEATURE_DISABLED_BY_DEFAULT};

size_t MaxHintsFetcherTopHostBlacklistSize() {
  // The blacklist will be limited to the most engaged hosts and will hold twice
  // (2*N) as many hosts that the HintsFetcher request hints for. The extra N
  // hosts on the blacklist are meant to cover the case that the engagement
  // scores on some of the top N host engagement scores decay and they fall out
  // of the top N.
  return GetFieldTrialParamByFeatureAsInt(kOptimizationHintsFetching,
                                          "top_host_blacklist_size_multiplier",
                                          3) *
         MaxHostsForOptimizationGuideServiceHintsFetch();
}

size_t MaxHostsForOptimizationGuideServiceHintsFetch() {
  return GetFieldTrialParamByFeatureAsInt(
      kOptimizationHintsFetching,
      "max_hosts_for_optimization_guide_service_hints_fetch", 30);
}

size_t MaxHostsForRecordingSuccessfullyCovered() {
  return GetFieldTrialParamByFeatureAsInt(
      kOptimizationHintsFetching,
      "max_hosts_for_recording_successfully_covered", 200);
}

double MinTopHostEngagementScoreThreshold() {
  // The default initial site engagement score for a navigation is 3.0, 1.5
  // points for a navigation from the omnibox and 1.5 points for the first
  // navigation of the day.
  return GetFieldTrialParamByFeatureAsDouble(
      kOptimizationHintsFetching, "min_top_host_engagement_score_threshold",
      2.0);
}

base::TimeDelta StoredFetchedHintsFreshnessDuration() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kOptimizationHintsFetching,
      "max_store_duration_for_featured_hints_in_days", 7));
}

base::TimeDelta DurationApplyLowEngagementScoreThreshold() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kOptimizationHintsFetching,
      "duration_apply_low_engagement_score_threshold_in_days", 30));
}

std::string GetOptimizationGuideServiceAPIKey() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceAPIKey)) {
    return command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceAPIKey);
  }

  return google_apis::GetAPIKey();
}

GURL GetOptimizationGuideServiceGetHintsURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kOptimizationGuideServiceGetHintsURL)) {
    // Assume the command line switch is correct and return it.
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceGetHintsURL));
  }

  std::string url = base::GetFieldTrialParamValueByFeature(
      kOptimizationHintsFetching, "optimization_guide_service_url");
  if (url.empty() || !GURL(url).SchemeIs(url::kHttpsScheme)) {
    if (!url.empty())
      LOG(WARNING)
          << "Empty or invalid optimization_guide_service_url provided: "
          << url;
    return GURL(kOptimizationGuideServiceGetHintsDefaultURL);
  }

  return GURL(url);
}

GURL GetOptimizationGuideServiceGetModelsURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kOptimizationGuideServiceGetModelsURL)) {
    // Assume the command line switch is correct and return it.
    return GURL(command_line->GetSwitchValueASCII(
        switches::kOptimizationGuideServiceGetModelsURL));
  }

  GURL get_models_url(kOptimizationGuideServiceGetModelsDefaultURL);
  CHECK(get_models_url.SchemeIs(url::kHttpsScheme));
  return get_models_url;
}

bool IsOptimizationHintsEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationHints);
}

bool IsHintsFetchingEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationHintsFetching);
}

bool IsHintsFetchingForAnonymousDataConsentEnabled() {
  return base::FeatureList::IsEnabled(
      kOptimizationHintsFetchingAnonymousDataConsent);
}

int MaxServerBloomFilterByteSize() {
  return base::GetFieldTrialParamByFeatureAsInt(
      kOptimizationHints, "max_bloom_filter_byte_size", 250 * 1024 /* 250KB */);
}

base::Optional<net::EffectiveConnectionType>
GetMaxEffectiveConnectionTypeForNavigationHintsFetch() {
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kOptimizationHintsFetching,
      "max_effective_connection_type_for_navigation_hints_fetch");

  // Use a default value.
  if (param_value.empty())
    return net::EFFECTIVE_CONNECTION_TYPE_3G;

  return net::GetEffectiveConnectionTypeForName(param_value);
}

base::TimeDelta GetHintsFetchRefreshDuration() {
  return base::TimeDelta::FromHours(72);
}

base::TimeDelta StoredHostModelFeaturesFreshnessDuration() {
  return base::TimeDelta::FromDays(GetFieldTrialParamByFeatureAsInt(
      kOptimizationTargetPrediction,
      "max_store_duration_for_host_model_features_in_days", 7));
}

bool IsOptimizationTargetPredictionEnabled() {
  return base::FeatureList::IsEnabled(kOptimizationTargetPrediction);
}

bool ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
    proto::OptimizationTarget optimization_target) {
  if (optimization_target != proto::OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD)
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kOptimizationTargetPrediction, "painful_page_load_metrics_only", false);
}

}  // namespace features
}  // namespace optimization_guide
