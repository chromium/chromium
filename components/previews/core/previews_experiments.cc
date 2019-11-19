// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "net/base/url_util.h"

namespace previews {

namespace {

// The group of client-side previews experiments. This controls paramters of the
// client side blacklist.
const char kClientSidePreviewsFieldTrial[] = "ClientSidePreviews";

// Name for the version parameter of a field trial. Version changes will
// result in older blacklist entries being removed.
const char kVersion[] = "version";

// Parameter to clarify that the preview for a UserConsistent study should
// be enabled or not.
const char kUserConsistentPreviewEnabled[] = "user_consistent_preview_enabled";

// The threshold of EffectiveConnectionType above which previews will not be
// served.
// See net/nqe/effective_connection_type.h for mapping from string to value.
const char kEffectiveConnectionTypeThreshold[] =
    "max_allowed_effective_connection_type";

// The session maximum threshold of EffectiveConnectionType above which previews
// will not be served. This is maximum limit on top of any per-preview-type
// threshold or per-page-pattern slow page trigger threshold. It is intended
// to be Finch configured on a session basis to limit slow page triggering to
// be a proportion of all eligible page loads.
// See net/nqe/effective_connection_type.h for mapping from string to value.
const char kSessionMaxECTTrigger[] = "session_max_ect_trigger";

// Inflation parameters for estimating NoScript data savings.
const char kNoScriptInflationPercent[] = "NoScriptInflationPercent";
const char kNoScriptInflationBytes[] = "NoScriptInflationBytes";

// Inflation parameters for estimating ResourceLoadingHints data savings.
const char kResourceLoadingHintsInflationPercent[] =
    "ResourceLoadingHintsInflationPercent";
const char kResourceLoadingHintsInflationBytes[] =
    "ResourceLoadingHintsInflationBytes";

size_t GetParamValueAsSizeT(const std::string& trial_name,
                            const std::string& param_name,
                            size_t default_value) {
  size_t value;
  if (!base::StringToSizeT(
          base::GetFieldTrialParamValue(trial_name, param_name), &value)) {
    return default_value;
  }
  return value;
}

int GetParamValueAsInt(const std::string& trial_name,
                       const std::string& param_name,
                       int default_value) {
  int value;
  if (!base::StringToInt(base::GetFieldTrialParamValue(trial_name, param_name),
                         &value)) {
    return default_value;
  }
  return value;
}

net::EffectiveConnectionType GetParamValueAsECT(
    const std::string& trial_name,
    const std::string& param_name,
    net::EffectiveConnectionType default_value) {
  return net::GetEffectiveConnectionTypeForName(
             base::GetFieldTrialParamValue(trial_name, param_name))
      .value_or(default_value);
}

net::EffectiveConnectionType GetParamValueAsECTByFeature(
    const base::Feature& feature,
    const std::string& param_name,
    net::EffectiveConnectionType default_value) {
  return net::GetEffectiveConnectionTypeForName(
             base::GetFieldTrialParamValueByFeature(feature, param_name))
      .value_or(default_value);
}

// Returns the effective Feature for DeferAllScript (which may be the
// UserConsistent variant).
const base::Feature& GetDeferAllScriptPreviewsFeature() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kDeferAllScriptPreviewsUserConsistentStudy)) {
    return features::kDeferAllScriptPreviewsUserConsistentStudy;
  }

  return features::kDeferAllScriptPreviews;
}

// Returns the effective Feature for LitePageServerPreviews (which may be the
// UserConsistent variant).
const base::Feature& GetLitePageServerPreviewsFeature() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kLitePageServerPreviewsUserConsistentStudy)) {
    return features::kLitePageServerPreviewsUserConsistentStudy;
  }

  return features::kLitePageServerPreviews;
}

// Returns the effective Feature for ResourceLoadingHints (which may be the
// UserConsistent variant).
const base::Feature& GetResourceLoadingHintsFeature() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kResourceLoadingHintsUserConsistentStudy)) {
    return features::kResourceLoadingHintsUserConsistentStudy;
  }

  return features::kResourceLoadingHints;
}

// Returns the effective Feature for NoScriptPreviews (which may be the
// UserConsistent variant).
const base::Feature& GetNoScriptPreviewsFeature() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kNoScriptPreviewsUserConsistentStudy)) {
    return features::kNoScriptPreviewsUserConsistentStudy;
  }

  return features::kNoScriptPreviews;
}

}  // namespace

namespace params {

size_t MaxStoredHistoryLengthForPerHostBlackList() {
  return GetParamValueAsSizeT(kClientSidePreviewsFieldTrial,
                              "per_host_max_stored_history_length", 4);
}

size_t MaxStoredHistoryLengthForHostIndifferentBlackList() {
  return GetParamValueAsSizeT(kClientSidePreviewsFieldTrial,
                              "host_indifferent_max_stored_history_length", 10);
}

size_t MaxInMemoryHostsInBlackList() {
  return GetParamValueAsSizeT(kClientSidePreviewsFieldTrial,
                              "max_hosts_in_blacklist", 100);
}

int PerHostBlackListOptOutThreshold() {
  return GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                            "per_host_opt_out_threshold", 2);
}

int HostIndifferentBlackListOptOutThreshold() {
  return GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                            "host_indifferent_opt_out_threshold", 6);
}

base::TimeDelta PerHostBlackListDuration() {
  return base::TimeDelta::FromDays(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "per_host_black_list_duration_in_days", 30));
}

base::TimeDelta HostIndifferentBlackListPerHostDuration() {
  return base::TimeDelta::FromDays(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "host_indifferent_black_list_duration_in_days", 30));
}

base::TimeDelta SingleOptOutDuration() {
  return base::TimeDelta::FromSeconds(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "single_opt_out_duration_in_seconds", 60 * 5));
}

base::TimeDelta OfflinePreviewFreshnessDuration() {
  return base::TimeDelta::FromDays(
      GetParamValueAsInt(kClientSidePreviewsFieldTrial,
                         "offline_preview_freshness_duration_in_days", 7));
}

base::TimeDelta LitePagePreviewsSingleBypassDuration() {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      GetLitePageServerPreviewsFeature(), "single_bypass_duration_in_seconds",
      60 * 5));
}

base::TimeDelta LitePagePreviewsNavigationTimeoutDuration() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(GetLitePageServerPreviewsFeature(),
                                             "navigation_timeout_milliseconds",
                                             30 * 1000));
}

int PreviewServerLoadshedMaxSeconds() {
  return base::GetFieldTrialParamByFeatureAsInt(
      GetLitePageServerPreviewsFeature(), "loadshed_max_seconds",
      5 * 60 /* 5 minutes */);
}

bool LitePagePreviewsTriggerOnLocalhost() {
  return base::GetFieldTrialParamByFeatureAsBool(
      GetLitePageServerPreviewsFeature(), "trigger_on_localhost", false);
}

bool LitePagePreviewsOverridePageHints() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kLitePageRedirectOverridesPageHints)) {
    return true;
  }
  return base::GetFieldTrialParamByFeatureAsBool(
      GetLitePageServerPreviewsFeature(), "override_pagehints", false);
}

GURL GetLitePagePreviewsDomainURL() {
  // Command line override takes priority.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLitePageServerPreviewHost)) {
    const std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kLitePageServerPreviewHost);
    const GURL url(switch_value);
    if (url.is_valid())
      return url;
    LOG(WARNING)
        << "The following litepages previews host URL specified at the "
        << "command-line is invalid: " << switch_value;
  }

  std::string variable_host_str = GetFieldTrialParamValueByFeature(
      GetLitePageServerPreviewsFeature(), "previews_host");
  if (!variable_host_str.empty()) {
    GURL variable_host(variable_host_str);
    DCHECK(variable_host.is_valid());
    DCHECK(variable_host.has_scheme());
    return variable_host;
  }
  return GURL("https://litepages.googlezip.net/");
}

bool IsInLitePageRedirectControl() {
  return base::GetFieldTrialParamByFeatureAsBool(
      GetLitePageServerPreviewsFeature(), "control_group", false);
}

bool LitePageRedirectPreviewShouldPreconnect() {
  return base::GetFieldTrialParamByFeatureAsBool(
      GetLitePageServerPreviewsFeature(), "preconnect_on_slow_connections",
      false);
}

bool LitePageRedirectPreviewShouldPresolve() {
  return base::GetFieldTrialParamByFeatureAsBool(
      GetLitePageServerPreviewsFeature(), "preresolve_on_slow_connections",
      true);
}

bool LitePageRedirectPreviewIgnoresOptimizationGuideFilter() {
  return base::GetFieldTrialParamByFeatureAsBool(
             GetLitePageServerPreviewsFeature(),
             "ignore_optimization_guide_filtering", false) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kIgnoreLitePageRedirectOptimizationBlacklist);
}

bool LitePageRedirectOnlyTriggerOnSuccessfulProbe() {
  return base::GetFieldTrialParamByFeatureAsBool(
      GetLitePageServerPreviewsFeature(), "only_trigger_after_probe_success",
      true);
}

GURL LitePageRedirectProbeURL() {
  GURL url(GetFieldTrialParamValueByFeature(GetLitePageServerPreviewsFeature(),
                                            "full_probe_url"));
  if (url.is_valid())
    return url;
  return GURL("https://litepages.googlezip.net/e2e_probe");
}

base::TimeDelta LitePageRedirectPreviewPreresolvePreconnectInterval() {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      GetLitePageServerPreviewsFeature(),
      "preresolveconnect_interval_in_seconds", 60));
}

net::EffectiveConnectionType
LitePageRedirectPreviewPreresolvePreconnectECTThreshold() {
  return GetParamValueAsECTByFeature(GetLitePageServerPreviewsFeature(),
                                     "preresolveconnect_ect_threshold",
                                     net::EFFECTIVE_CONNECTION_TYPE_2G);
}

base::TimeDelta LitePageRedirectPreviewProbeInterval() {
  return base::TimeDelta::FromSeconds(base::GetFieldTrialParamByFeatureAsInt(
      GetLitePageServerPreviewsFeature(), "probe_interval_in_seconds", 30));
}

bool LitePageRedirectShouldProbeOrigin() {
  return base::GetFieldTrialParamByFeatureAsBool(
      GetLitePageServerPreviewsFeature(), "should_probe_origin", false);
}

bool LitePageRedirectTriggerOnAPITransition() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kLitePageServerPreviews, "should_trigger_on_api_transitions",
      false);
}

bool LitePageRedirectValidateForwardBackTransition() {
  // When enabled, validate every forward/back transition to ensure we reuse
  // the same previews state. For example, if we navigate to A then B, then
  // click back, we will show a preview for A iff the first navigation to A
  // showed a preview.
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kLitePageServerPreviews,
      "should_validate_forward_back_transitions", true);
}

base::TimeDelta LitePageRedirectPreviewOriginProbeTimeout() {
  return base::TimeDelta::FromMilliseconds(
      base::GetFieldTrialParamByFeatureAsInt(GetLitePageServerPreviewsFeature(),
                                             "origin_probe_timeout_ms",
                                             30 * 1000));
}

net::EffectiveConnectionType GetECTThresholdForPreview(
    previews::PreviewsType type) {
  switch (type) {
    case PreviewsType::OFFLINE:
      return GetParamValueAsECTByFeature(features::kOfflinePreviews,
                                         kEffectiveConnectionTypeThreshold,
                                         net::EFFECTIVE_CONNECTION_TYPE_2G);
    case PreviewsType::NOSCRIPT:
      return GetParamValueAsECTByFeature(features::kNoScriptPreviews,
                                         kEffectiveConnectionTypeThreshold,
                                         net::EFFECTIVE_CONNECTION_TYPE_2G);
    case PreviewsType::LITE_PAGE_REDIRECT: {
      // First check ECT threshold in kLitePageServerPreviews and return that
      // (if it's available).
      net::EffectiveConnectionType lite_page_ect = GetParamValueAsECTByFeature(
          features::kLitePageServerPreviews, kEffectiveConnectionTypeThreshold,
          net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
      if (lite_page_ect != net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN)
        return lite_page_ect;

      // Next check ECT threshold in kClientSidePreviewsFieldTrial and return
      // that (if it's available). In M-78, the ECT threshold for
      // LITE_PAGE_REDIRECT is determined from kClientSidePreviewsFieldTrial.
      // So, checking kClientSidePreviewsFieldTrial makes the code backwards
      // compatible.
      net::EffectiveConnectionType client_side_ect = GetParamValueAsECT(
          kClientSidePreviewsFieldTrial, kEffectiveConnectionTypeThreshold,
          net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
      if (client_side_ect != net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN)
        return client_side_ect;

      // Return the default value.
      return net::EFFECTIVE_CONNECTION_TYPE_2G;
    }

    case PreviewsType::LITE_PAGE:
      NOTREACHED();
      break;
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
    case PreviewsType::RESOURCE_LOADING_HINTS:
      return GetParamValueAsECTByFeature(GetResourceLoadingHintsFeature(),
                                         kEffectiveConnectionTypeThreshold,
                                         net::EFFECTIVE_CONNECTION_TYPE_2G);
    case PreviewsType::DEFER_ALL_SCRIPT:
      return GetParamValueAsECTByFeature(GetDeferAllScriptPreviewsFeature(),
                                         kEffectiveConnectionTypeThreshold,
                                         net::EFFECTIVE_CONNECTION_TYPE_2G);
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::DEPRECATED_LOFI:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
}

net::EffectiveConnectionType GetSessionMaxECTThreshold() {
  return GetParamValueAsECTByFeature(features::kSlowPageTriggering,
                                     kSessionMaxECTTrigger,
                                     net::EFFECTIVE_CONNECTION_TYPE_3G);
}

bool ArePreviewsAllowed() {
  return base::FeatureList::IsEnabled(features::kPreviews);
}

bool IsOfflinePreviewsEnabled() {
  return base::FeatureList::IsEnabled(features::kOfflinePreviews);
}

bool IsNoScriptPreviewsEnabled() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kNoScriptPreviewsUserConsistentStudy)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::kNoScriptPreviewsUserConsistentStudy,
        kUserConsistentPreviewEnabled, false);
  }
  return base::FeatureList::IsEnabled(features::kNoScriptPreviews);
}

bool IsResourceLoadingHintsEnabled() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kResourceLoadingHintsUserConsistentStudy)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::kResourceLoadingHintsUserConsistentStudy,
        kUserConsistentPreviewEnabled, false);
  }
  return base::FeatureList::IsEnabled(features::kResourceLoadingHints);
}

bool IsLitePageServerPreviewsEnabled() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kLitePageServerPreviewsUserConsistentStudy)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::kLitePageServerPreviewsUserConsistentStudy,
        kUserConsistentPreviewEnabled, false);
  }
  return base::FeatureList::IsEnabled(features::kLitePageServerPreviews);
}

bool IsDeferAllScriptPreviewsEnabled() {
  if (base::FeatureList::IsEnabled(features::kEligibleForUserConsistentStudy) &&
      base::FeatureList::IsEnabled(
          features::kDeferAllScriptPreviewsUserConsistentStudy)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        features::kDeferAllScriptPreviewsUserConsistentStudy,
        kUserConsistentPreviewEnabled, false);
  }
  return base::FeatureList::IsEnabled(features::kDeferAllScriptPreviews);
}

int OfflinePreviewsVersion() {
  return GetParamValueAsInt(kClientSidePreviewsFieldTrial, kVersion, 0);
}

int LitePageServerPreviewsVersion() {
  return base::GetFieldTrialParamByFeatureAsInt(
      GetLitePageServerPreviewsFeature(), kVersion, 0);
}

int NoScriptPreviewsVersion() {
  return GetFieldTrialParamByFeatureAsInt(GetNoScriptPreviewsFeature(),
                                          kVersion, 0);
}

int ResourceLoadingHintsVersion() {
  return GetFieldTrialParamByFeatureAsInt(GetResourceLoadingHintsFeature(),
                                          kVersion, 0);
}

int DeferAllScriptPreviewsVersion() {
  return GetFieldTrialParamByFeatureAsInt(GetDeferAllScriptPreviewsFeature(),
                                          kVersion, 0);
}

int NoScriptPreviewsInflationPercent() {
  // The default value was determined from lab experiment data of whitelisted
  // URLs. It may be improved once there is enough UKM live experiment data
  // via the field trial param.
  return GetFieldTrialParamByFeatureAsInt(GetNoScriptPreviewsFeature(),
                                          kNoScriptInflationPercent, 80);
}

int NoScriptPreviewsInflationBytes() {
  return GetFieldTrialParamByFeatureAsInt(GetNoScriptPreviewsFeature(),
                                          kNoScriptInflationBytes, 0);
}

int ResourceLoadingHintsPreviewsInflationPercent() {
  return GetFieldTrialParamByFeatureAsInt(GetResourceLoadingHintsFeature(),
                                          kResourceLoadingHintsInflationPercent,
                                          20);
}

int ResourceLoadingHintsPreviewsInflationBytes() {
  return GetFieldTrialParamByFeatureAsInt(
      GetResourceLoadingHintsFeature(), kResourceLoadingHintsInflationBytes, 0);
}

size_t OfflinePreviewsHelperMaxPrefSize() {
  return GetFieldTrialParamByFeatureAsInt(
      features::kOfflinePreviewsFalsePositivePrevention, "max_pref_entries",
      100);
}

bool ShouldOverrideNavigationCoinFlipToHoldback() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kCoinFlipHoldback, "force_coin_flip_always_holdback", false);
}

bool ShouldOverrideNavigationCoinFlipToAllowed() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kCoinFlipHoldback, "force_coin_flip_always_allow", false);
}

bool ShouldExcludeMediaSuffix(const GURL& url) {
  if (!base::FeatureList::IsEnabled(features::kExcludedMediaSuffixes))
    return false;

  std::vector<std::string> suffixes = {
      ".apk", ".avi",  ".gif", ".gifv", ".jpeg", ".jpg", ".mp3",
      ".mp4", ".mpeg", ".pdf", ".png",  ".webm", ".webp"};

  std::string csv = base::GetFieldTrialParamValueByFeature(
      features::kExcludedMediaSuffixes, "excluded_path_suffixes");
  if (csv != "") {
    suffixes = base::SplitString(csv, ",", base::TRIM_WHITESPACE,
                                 base::SPLIT_WANT_NONEMPTY);
  }

  for (const std::string& suffix : suffixes) {
    if (base::EndsWith(url.path(), suffix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }
  return false;
}

bool DetectDeferRedirectLoopsUsingCache() {
  if (!IsDeferAllScriptPreviewsEnabled())
    return false;

  return GetFieldTrialParamByFeatureAsBool(GetDeferAllScriptPreviewsFeature(),
                                           "detect_redirect_loop_using_cache",
                                           true);
}

}  // namespace params

std::string GetStringNameForType(PreviewsType type) {
  // The returned string is used to record histograms for the new preview type.
  // Also add the string to Previews.Types histogram suffix in histograms.xml.
  switch (type) {
    case PreviewsType::NONE:
      return "None";
    case PreviewsType::OFFLINE:
      return "Offline";
    case PreviewsType::LITE_PAGE:
      return "LitePage";
    case PreviewsType::LITE_PAGE_REDIRECT:
      return "LitePageRedirect";
    case PreviewsType::NOSCRIPT:
      return "NoScript";
    case PreviewsType::UNSPECIFIED:
      return "Unspecified";
    case PreviewsType::RESOURCE_LOADING_HINTS:
      return "ResourceLoadingHints";
    case PreviewsType::DEFER_ALL_SCRIPT:
      return "DeferAllScript";
    case PreviewsType::DEPRECATED_AMP_REDIRECTION:
    case PreviewsType::DEPRECATED_LOFI:
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return std::string();
}

}  // namespace previews
