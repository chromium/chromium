// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_experiments.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "net/base/url_util.h"

namespace previews {

namespace {

// Name for the version parameter of a field trial. Version changes will
// result in older blocklist entries being removed.
const char kVersion[] = "version";

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
  return features::kDeferAllScriptPreviews;
}

}  // namespace

namespace params {

size_t MaxStoredHistoryLengthForPerHostBlockList() {
  return 4;
}

size_t MaxStoredHistoryLengthForHostIndifferentBlockList() {
  return 10;
}

size_t MaxInMemoryHostsInBlockList() {
  return 100;
}

int PerHostBlockListOptOutThreshold() {
  return 2;
}

int HostIndifferentBlockListOptOutThreshold() {
  return 6;
}

base::TimeDelta PerHostBlockListDuration() {
  return base::TimeDelta::FromDays(30);
}

base::TimeDelta HostIndifferentBlockListPerHostDuration() {
  return base::TimeDelta::FromDays(30);
}

base::TimeDelta SingleOptOutDuration() {
  return base::TimeDelta::FromSeconds(60 * 5);
}

net::EffectiveConnectionType GetECTThresholdForPreview(
    previews::PreviewsType type) {
  switch (type) {
    case PreviewsType::NONE:
    case PreviewsType::UNSPECIFIED:
      return net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;
    case PreviewsType::DEFER_ALL_SCRIPT:
      return GetParamValueAsECTByFeature(GetDeferAllScriptPreviewsFeature(),
                                         kEffectiveConnectionTypeThreshold,
                                         net::EFFECTIVE_CONNECTION_TYPE_2G);
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

bool IsDeferAllScriptPreviewsEnabled() {
  return base::FeatureList::IsEnabled(features::kDeferAllScriptPreviews);
}


int DeferAllScriptPreviewsVersion() {
  return GetFieldTrialParamByFeatureAsInt(GetDeferAllScriptPreviewsFeature(),
                                          kVersion, 0);
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

bool OverrideShouldShowPreviewCheck() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kPreviews, "override_should_show_preview_check", false);
}

bool ApplyDeferWhenOptimizationGuideDecisionUnknown() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kPreviews, "apply_deferallscript_when_guide_decision_unknown",
      true);
}

}  // namespace params

std::string GetStringNameForType(PreviewsType type) {
  // The returned string is used to record histograms for the new preview type.
  // Also add the string to Previews.Types histogram suffix in histograms.xml.
  switch (type) {
    case PreviewsType::NONE:
      return "None";
    case PreviewsType::UNSPECIFIED:
      return "Unspecified";
    case PreviewsType::DEFER_ALL_SCRIPT:
      return "DeferAllScript";
    case PreviewsType::LAST:
      break;
  }
  NOTREACHED();
  return std::string();
}

}  // namespace previews
