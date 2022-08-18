// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/features.h"

#include <stddef.h>
#include <algorithm>
#include <utility>
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/safe_browsing/buildflags.h"
#include "components/variations/variations_associated_data.h"

namespace safe_browsing {
// Please define any new SafeBrowsing related features in this file, and add
// them to the ExperimentalFeaturesList below to start displaying their status
// on the chrome://safe-browsing page.

const base::Feature kAccuracyTipsFeature{"AccuracyTips",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAdSamplerTriggerFeature{"SafeBrowsingAdSamplerTrigger",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBetterTelemetryAcrossReports{
    "SafeBrowsingBetterTelemetryAcrossReports",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable only for Android
#if BUILDFLAG(IS_ANDROID)
const base::Feature kClientSideDetectionModelIsFlatBuffer{
    "ClientSideDetectionModelIsFlatBuffer", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kClientSideDetectionModelIsFlatBuffer{
    "ClientSideDetectionModelIsFlatBuffer", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

extern const base::Feature kClientSideDetectionModelTag{
    "ClientSideDetectionTag", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kClientSideDetectionReferrerChain{
    "ClientSideDetectionReferrerChain", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClientSideDetectionKillswitch{
  "ClientSideDetectionKillswitch",
#if BUILDFLAG(IS_MAC)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

const base::Feature kConnectorsScanningAccessToken{
    "ConnectorsScanningAccessToken", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kConnectorsScanningReportOnlyUI{
    "ConnectorsScanningReportOnlyUI", base::FEATURE_ENABLED_BY_DEFAULT};

#if BUILDFLAG(IS_ANDROID)
const base::Feature kCreateSafebrowsingOnStartup{
    "CreateSafebrowsingOnStartup", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

const base::Feature kDelayedWarnings{"SafeBrowsingDelayedWarnings",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// If true, a delayed warning will be shown when the user clicks on the page.
// If false, the warning won't be shown, but a metric will be recorded on the
// first click.
const base::FeatureParam<bool> kDelayedWarningsEnableMouseClicks{
    &kDelayedWarnings, "mouse",
    /*default_value=*/false};

const base::Feature kDownloadBubble{"DownloadBubble",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadBubbleV2{"DownloadBubbleV2",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnhancedProtection{"SafeBrowsingEnhancedProtection",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnhancedProtectionPhase2IOS{
    "SafeBrowsingEnhancedProtectionPhase2IOS",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kExtensionTelemetry{"SafeBrowsingExtensionTelemetry",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kExtensionTelemetryPersistence{
    "SafeBrowsingExtensionTelemetryPersistence",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kExtensionTelemetryUploadIntervalSeconds{
    &kExtensionTelemetry, "UploadIntervalSeconds",
    /*default_value=*/3600};

const base::FeatureParam<int> kExtensionTelemetryWritesPerInterval{
    &kExtensionTelemetry, "NumberOfWritesInInterval",
    /*default_value=*/4};

const base::Feature kExtensionTelemetryTabsExecuteScriptSignal{
    "SafeBrowsingExtensionTelemetryTabsExecuteScriptSignal",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kExtensionTelemetryReportContactedHosts{
    "SafeBrowsingExtensionTelemetryReportContactedHosts",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFileTypePoliciesTag{"FileTypePoliciesTag",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kLogAccountEnhancedProtectionStateInProtegoPings{
    "TailoredSecurityLogAccountEnhancedProtectionStateInProtegoPings",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSimplifiedUrlDisplay{"SimplifiedUrlDisplay",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTailoredSecurityDesktopNotice{
    "TailoredSecurityDesktopNotice", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTailoredSecurityIntegration{
  "TailoredSecurityIntegration",
#if BUILDFLAG(IS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

const base::Feature kOmitNonUserGesturesFromReferrerChain{
    "SafeBrowsingOmitNonUserGesturesFromReferrerChain",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingCsbrrWithToken{
    "SafeBrowsingCsbrrWithToken", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingEnterpriseCsd{
    "SafeBrowsingEnterpriseCsd", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingDisableConsumerCsdForEnterprise{
    "SafeBrowsingDisableConsumerCsdForEnterprise",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingPageLoadToken{
    "SafeBrowsingPageLoadToken", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingRemoveCookiesInAuthRequests{
    "SafeBrowsingRemoveCookiesInAuthRequests",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendSampledPingsForProtegoAllowlistDomains{
    "SafeBrowsingSendSampledPingsForProtegoAllowlistDomains",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSuspiciousSiteTriggerQuotaFeature{
    "SafeBrowsingSuspiciousSiteTriggerQuota", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kThreatDomDetailsTagAndAttributeFeature{
    "ThreatDomDetailsTagAttributes", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUseNewDownloadWarnings{"UseNewDownloadWarnings",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVisualFeaturesSizes{"VisualFeaturesSizes",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVisualFeaturesInCsppPings{
    "VisualFeaturesInCsppPings", base::FEATURE_ENABLED_BY_DEFAULT};

namespace {
// List of Safe Browsing features. Boolean value for each list member should
// be set to true if the experiment state should be listed on
// chrome://safe-browsing. Features should be listed in alphabetical order.
constexpr struct {
  const base::Feature* feature;
  // True if the feature's state should be listed on chrome://safe-browsing.
  bool show_state;
} kExperimentalFeatures[]{
    {&kAccuracyTipsFeature, true},
    {&kAdSamplerTriggerFeature, false},
    {&kBetterTelemetryAcrossReports, true},
    {&kClientSideDetectionModelIsFlatBuffer, true},
    {&kClientSideDetectionReferrerChain, true},
    {&kConnectorsScanningReportOnlyUI, true},
    {&kDelayedWarnings, true},
    {&kDownloadBubble, true},
    {&kDownloadBubbleV2, true},
    {&kEnhancedProtection, true},
    {&kEnhancedProtectionPhase2IOS, true},
    {&kExtensionTelemetry, true},
    {&kExtensionTelemetryReportContactedHosts, true},
    {&kExtensionTelemetryPersistence, true},
    {&kFileTypePoliciesTag, true},
    {&kOmitNonUserGesturesFromReferrerChain, true},
    {&kSafeBrowsingCsbrrWithToken, true},
    {&kSafeBrowsingPageLoadToken, true},
    {&kSafeBrowsingRemoveCookiesInAuthRequests, true},
    {&kSendSampledPingsForProtegoAllowlistDomains, true},
    {&kSuspiciousSiteTriggerQuotaFeature, true},
    {&kThreatDomDetailsTagAndAttributeFeature, false},
};

// Adds the name and the enabled/disabled status of a given feature.
void AddFeatureAndAvailability(const base::Feature* exp_feature,
                               base::Value::List* param_list) {
  param_list->Append(exp_feature->name);
  if (base::FeatureList::IsEnabled(*exp_feature)) {
    param_list->Append("Enabled");
  } else {
    param_list->Append("Disabled");
  }
}
}  // namespace

// Returns the list of the experimental features that are enabled or disabled,
// as part of currently running Safe Browsing experiments.
base::Value::List GetFeatureStatusList() {
  base::Value::List param_list;
  for (const auto& feature_status : kExperimentalFeatures) {
    if (feature_status.show_state)
      AddFeatureAndAvailability(feature_status.feature, &param_list);
  }

  // Manually add experimental features that we want param values for.
  param_list.Append(variations::GetVariationParamValueByFeature(
      safe_browsing::kClientSideDetectionModelTag,
      kClientSideDetectionTagParamName));
  param_list.Append(kClientSideDetectionModelTag.name);
  param_list.Append(variations::GetVariationParamValueByFeature(
      kFileTypePoliciesTag, kFileTypePoliciesTagParamName));
  param_list.Append(kFileTypePoliciesTag.name);

  return param_list;
}

std::string GetClientSideDetectionTag() {
  if (base::FeatureList::IsEnabled(
          safe_browsing::kClientSideDetectionModelTag)) {
    return variations::GetVariationParamValueByFeature(
        safe_browsing::kClientSideDetectionModelTag,
        kClientSideDetectionTagParamName);
  }

#if BUILDFLAG(IS_ANDROID)
  return "android_1";
#else
  return "desktop_1";
#endif
}

std::string GetFileTypePoliciesTag() {
  if (!base::FeatureList::IsEnabled(kFileTypePoliciesTag)) {
    return "default";
  }
  std::string tag_value = variations::GetVariationParamValueByFeature(
      kFileTypePoliciesTag, kFileTypePoliciesTagParamName);

  return tag_value.empty() ? "default" : tag_value;
}

}  // namespace safe_browsing
