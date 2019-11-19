// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/features.h"

#include <stddef.h>
#include <algorithm>
#include <utility>
#include <vector>
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/safe_browsing/buildflags.h"

#include "base/macros.h"
#include "base/values.h"
namespace safe_browsing {
// Please define any new SafeBrowsing related features in this file, and add
// them to the ExperimentalFeaturesList below to start displaying their status
// on the chrome://safe-browsing page.
const base::Feature kAdPopupTriggerFeature{"SafeBrowsingAdPopupTrigger",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAdRedirectTriggerFeature{
    "SafeBrowsingAdRedirectTrigger", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls various parameters related to occasionally collecting ad samples,
// for example to control how often collection should occur.
const base::Feature kAdSamplerTriggerFeature{"SafeBrowsingAdSamplerTrigger",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCaptureInlineJavascriptForGoogleAds{
    "CaptureInlineJavascriptForGoogleAds", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCaptureSafetyNetId{"SafeBrowsingCaptureSafetyNetId",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCommittedSBInterstitials{
    "SafeBrowsingCommittedInterstitials", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDeepScanningOfDownloads{
    "SafeBrowsingDeepScanningOfDownloads", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPasswordProtectionForSavedPasswords{
    "SafeBrowsingPasswordProtectionForSavedPasswords",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPasswordProtectionShowDomainsForSavedPasswords{
    "SafeBrowsingPasswordProtectionShowDomainsForSavedPasswords",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPasswordProtectionForSignedInUsers{
    "SafeBrowsingPasswordProtectionForSignedInUsers",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRealTimeUrlLookupEnabled{
    "SafeBrowsingRealTimeUrlLookupEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendOnFocusPing {
  "SafeBrowsingSendOnFocusPing",
#if BUILDFLAG(FULL_SAFE_BROWSING)
      base::FEATURE_ENABLED_BY_DEFAULT
};
#else
      base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

const base::Feature kSendPasswordReusePing {
  "SafeBrowsingSendPasswordReusePing",
#if BUILDFLAG(FULL_SAFE_BROWSING)
      base::FEATURE_ENABLED_BY_DEFAULT
};
#else
      base::FEATURE_DISABLED_BY_DEFAULT
};
#endif

const base::Feature kSendSampledPingsForAllowlistDomains{
    "SafeBrowsingSendSampledPingsForAllowlistDomain",
    base::FEATURE_DISABLED_BY_DEFAULT};

constexpr base::FeatureParam<bool> kShouldFillOldPhishGuardProto{
    &kPasswordProtectionForSignedInUsers, "DeprecateOldProto", false};

const base::Feature kSuspiciousSiteTriggerQuotaFeature{
    "SafeBrowsingSuspiciousSiteTriggerQuota", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kThreatDomDetailsTagAndAttributeFeature{
    "ThreatDomDetailsTagAttributes", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTriggerThrottlerDailyQuotaFeature{
    "SafeBrowsingTriggerThrottlerDailyQuota",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseLocalBlacklistsV2{"SafeBrowsingUseLocalBlacklistsV2",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

namespace {
// List of Safe Browsing features. Boolean value for each list member should be
// set to true if the experiment state should be listed on
// chrome://safe-browsing.
constexpr struct {
  const base::Feature* feature;
  // True if the feature's state should be listed on chrome://safe-browsing.
  bool show_state;
} kExperimentalFeatures[]{
    {&kAdPopupTriggerFeature, true},
    {&kAdRedirectTriggerFeature, true},
    {&kAdSamplerTriggerFeature, false},
    {&kCaptureInlineJavascriptForGoogleAds, true},
    {&kCaptureSafetyNetId, true},
    {&kCommittedSBInterstitials, true},
    {&kPasswordProtectionForSavedPasswords, true},
    {&kPasswordProtectionShowDomainsForSavedPasswords, true},
    {&kPasswordProtectionForSignedInUsers, true},
    {&kRealTimeUrlLookupEnabled, true},
    {&kSendOnFocusPing, true},
    {&kSendPasswordReusePing, true},
    {&kSendSampledPingsForAllowlistDomains, false},
    {&kSuspiciousSiteTriggerQuotaFeature, true},
    {&kThreatDomDetailsTagAndAttributeFeature, false},
    {&kTriggerThrottlerDailyQuotaFeature, false},
    {&kUseLocalBlacklistsV2, true},
};

// Adds the name and the enabled/disabled status of a given feature.
void AddFeatureAndAvailability(const base::Feature* exp_feature,
                               base::ListValue* param_list) {
  param_list->Append(base::Value(exp_feature->name));
  if (base::FeatureList::IsEnabled(*exp_feature)) {
    param_list->Append(base::Value("Enabled"));
  } else {
    param_list->Append(base::Value("Disabled"));
  }
}
}  // namespace

// Returns the list of the experimental features that are enabled or disabled,
// as part of currently running Safe Browsing experiments.
base::ListValue GetFeatureStatusList() {
  base::ListValue param_list;
  for (const auto& feature_status : kExperimentalFeatures) {
    if (feature_status.show_state)
      AddFeatureAndAvailability(feature_status.feature, &param_list);
  }
  return param_list;
}

bool GetShouldFillOldPhishGuardProto() {
  return kShouldFillOldPhishGuardProto.Get();
}

}  // namespace safe_browsing
