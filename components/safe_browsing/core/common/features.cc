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
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/variations/variations_associated_data.h"

#include "base/values.h"
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

const base::Feature kClientSideDetectionDocumentScanning{
    "ClientSideDetectionDocumentScanning", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kClientSideDetectionForAndroid{
    "ClientSideDetectionModelOnAndroid", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable only for Android
#if BUILDFLAG(IS_ANDROID)
const base::Feature kClientSideDetectionModelIsFlatBuffer{
    "ClientSideDetectionModelIsFlatBuffer", base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kClientSideDetectionModelIsFlatBuffer{
    "ClientSideDetectionModelIsFlatBuffer", base::FEATURE_DISABLED_BY_DEFAULT};
#endif

extern const base::Feature kClientSideDetectionModelVersion{
    "ClientSideDetectionModel", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kClientSideDetectionModelTag{
    "ClientSideDetectionTag", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kClientSideDetectionModelHighMemoryTag{
    "ClientSideDetectionHighMemoryTag", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kClientSideDetectionReferrerChain{
    "ClientSideDetectionReferrerChain", base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(b/197749390): Add tests for this feature being enabled when it's
// finalized.
const base::Feature kConnectorsScanningReportOnlyUI{
    "ConnectorsScanningReportOnlyUI", base::FEATURE_DISABLED_BY_DEFAULT};

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

const base::Feature kEnhancedProtection {
  "SafeBrowsingEnhancedProtection",
#if BUILDFLAG(IS_IOS)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

const base::Feature kExtensionTelemetry{"SafeBrowsingExtensionTelemetry",
                                        base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kExtensionTelemetryUploadIntervalSeconds{
    &kExtensionTelemetry, "UploadIntervalSeconds",
    /*default_value=*/3600};
const base::Feature kExtensionTelemetryTabsExecuteScriptSignal{
    "SafeBrowsingExtensionTelemetryTabsExecuteScriptSignal",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kExtensionTelemetryReportContactedHosts{
    "SafeBrowsingExtensionTelemetryReportContactedHosts",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kFileTypePoliciesTag{"FileTypePoliciesTag",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSimplifiedUrlDisplay{"SimplifiedUrlDisplay",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTailoredSecurityIntegration{
  "TailoredSecurityIntegration",
#if BUILDFLAG(IS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

const base::Feature kOmitNonUserGesturesFromReferrerChain{
    "SafeBrowsingOmitNonUserGesturesFromReferrerChain",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPasswordProtectionForSignedInUsers{
    "SafeBrowsingPasswordProtectionForSignedInUsers",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPromptEsbForDeepScanning{
    "SafeBrowsingPromptEsbForDeepScanning", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingCsbrrWithToken{
    "SafeBrowsingCsbrrWithToken", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingCTDownloadWarning{
    "SafeBrowsingCTDownloadWarning", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingEnterpriseCsd{
    "SafeBrowsingEnterpriseCsd", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingDisableConsumerCsdForEnterprise{
    "SafeBrowsingDisableConsumerCsdForEnterprise",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingPageLoadToken{
    "SafeBrowsingPageLoadToken", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature
    kSafeBrowsingPasswordCheckIntegrationForSavedPasswordsAndroid{
        "SafeBrowsingPasswordCheckIntegrationForSavedPasswordsAndroid",
        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSafeBrowsingRemoveCookiesInAuthRequests{
    "SafeBrowsingRemoveCookiesInAuthRequests",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSendSampledPingsForProtegoAllowlistDomains{
    "SafeBrowsingSendSampledPingsForProtegoAllowlistDomains",
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
const base::Feature kUseNewDownloadWarnings{"UseNewDownloadWarnings",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVisualFeaturesInPasswordProtectionAndroid{
    "VisualFeaturesInPasswordProtectionAndroid",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kVisualFeaturesSizes{"VisualFeaturesSizes",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

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
    {&kClientSideDetectionForAndroid, true},
    {&kClientSideDetectionModelIsFlatBuffer, true},
    {&kClientSideDetectionModelVersion, true},
    {&kClientSideDetectionReferrerChain, true},
    {&kConnectorsScanningReportOnlyUI, true},
    {&kDelayedWarnings, true},
    {&kDownloadBubble, true},
    {&kEnhancedProtection, true},
    {&kExtensionTelemetry, true},
    {&kExtensionTelemetryReportContactedHosts, true},
    {&kFileTypePoliciesTag, true},
    {&kOmitNonUserGesturesFromReferrerChain, true},
    {&kPasswordProtectionForSignedInUsers, true},
    {&kSafeBrowsingCsbrrWithToken, true},
    {&kSafeBrowsingPageLoadToken, true},
    {&kSafeBrowsingPasswordCheckIntegrationForSavedPasswordsAndroid, true},
    {&kSafeBrowsingRemoveCookiesInAuthRequests, true},
    {&kSendSampledPingsForProtegoAllowlistDomains, true},
    {&kSuspiciousSiteTriggerQuotaFeature, true},
    {&kThreatDomDetailsTagAndAttributeFeature, false},
    {&kTriggerThrottlerDailyQuotaFeature, false},
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

  // Manually add experimental features that we want param values for.
  param_list.Append(base::Value(variations::GetVariationParamValueByFeature(
      safe_browsing::kClientSideDetectionModelTag,
      kClientSideDetectionTagParamName)));
  param_list.Append(base::Value(kClientSideDetectionModelTag.name));
  param_list.Append(base::Value(variations::GetVariationParamValueByFeature(
      safe_browsing::kClientSideDetectionModelHighMemoryTag,
      kClientSideDetectionTagParamName)));
  param_list.Append(base::Value(kClientSideDetectionModelHighMemoryTag.name));
  param_list.Append(base::Value(variations::GetVariationParamValueByFeature(
      kFileTypePoliciesTag, kFileTypePoliciesTagParamName)));
  param_list.Append(base::Value(kFileTypePoliciesTag.name));

  return param_list;
}

bool GetShouldFillOldPhishGuardProto() {
  return kShouldFillOldPhishGuardProto.Get();
}

std::string GetClientSideDetectionTag() {
  constexpr char kMemoryThresholdParamName[] = "memory_threshold_mb";
  const int kDefaultMemoryThresholdMB = 4096;
  if (base::FeatureList::IsEnabled(
          safe_browsing::kClientSideDetectionModelTag)) {
    return variations::GetVariationParamValueByFeature(
        safe_browsing::kClientSideDetectionModelTag,
        kClientSideDetectionTagParamName);
  } else if (base::FeatureList::IsEnabled(
                 safe_browsing::kClientSideDetectionModelHighMemoryTag)) {
    int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
        safe_browsing::kClientSideDetectionModelHighMemoryTag,
        kMemoryThresholdParamName, kDefaultMemoryThresholdMB);
    if (base::SysInfo::AmountOfPhysicalMemoryMB() >= memory_threshold_mb) {
      return variations::GetVariationParamValueByFeature(
          safe_browsing::kClientSideDetectionModelHighMemoryTag,
          kClientSideDetectionTagParamName);
    }
  }

  return "default";
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
