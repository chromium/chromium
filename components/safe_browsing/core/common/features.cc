// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/features.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics_action.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/safe_browsing/buildflags.h"
#include "features.h"

namespace safe_browsing {
// Please define any new SafeBrowsing related features in this file, and add
// them to the ExperimentalFeaturesList below to start displaying their status
// on the chrome://safe-browsing page.

// keep-sorted start sticky_prefixes=["#if"] group_prefixes=["#else", "#endif", "constexpr base::FeatureParam"] newline_separated=yes

BASE_FEATURE(kAdSamplerTriggerFeature,
             "SafeBrowsingAdSamplerTrigger",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kAddReferringAppInfoToProtegoPings,
             "AddReferringAppInfoToProtegoPings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAddReferringWebApkToProtegoPings,
             "AddReferringWebApkToProtegoPings",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kAddWarningShownTSToClientSafeBrowsingReport,
             "AddWarningShownTSToClientSafeBrowsingReport",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBundledSecuritySettings,
             "BundledSecuritySettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionAcceptHCAllowlist,
             "ClientSideDetectionAcceptHCAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionBrandAndIntentForScamDetection,
             "ClientSideDetectionBrandAndIntentForScamDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionDebuggingMetadataCache,
             "ClientSideDetectionDebuggingMetadataCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionForcedLlamaRedirectChainKillswitch,
             "ClientSideDetectionForcedLlamaRedirectChainKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionKillswitch,
             "ClientSideDetectionKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionSendLlamaForcedTriggerInfo,
             "ClientSideDetectionSendLlamaForcedTriggerInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionLlamaForcedTriggerInfoForScamDetection,
             "ClientSideDetectionLlamaForcedTriggerInfoForScamDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionKeyboardPointerLockRequest,
             "ClientSideDetectionKeyboardPointerLockRequest",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionNotificationPrompt,
             "ClientSideDetectionNotificationPrompt",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionRedirectChainKillswitch,
             "ClientSideDetectionRedirectChainKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionSamplePing,
             "ClientSideDetectionSamplePing",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionShowLlamaScamVerdictWarning,
             "ClientSideDetectionShowLlamaScamVerdictWarning",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionShowScamVerdictWarning,
             "ClientSideDetectionShowScamVerdictWarning",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionVibrationApi,
             "ClientSideDetectionVibrationApi",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kConditionalImageResize,
             "ConditionalImageResize",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateNotificationsAcceptedClientSafeBrowsingReports,
             "CreateNotificationsAcceptedClientSafeBrowsingReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCreateWarningShownClientSafeBrowsingReports,
             "CreateWarningShownClientSafeBrowsingReports",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDelayedWarnings,
             "SafeBrowsingDelayedWarnings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If true, a delayed warning will be shown when the user clicks on the page.
// If false, the warning won't be shown, but a metric will be recorded on the
// first click.
constexpr base::FeatureParam<bool> kDelayedWarningsEnableMouseClicks{
    &kDelayedWarnings, "mouse",
    /*default_value=*/false};

BASE_FEATURE(kDlpRegionalizedEndpoints,
             "DlpRegionalizedEndpoints",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadsPageReferrerUrl,
             "DownloadsPageReferrerUrl",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadWarningSurvey,
             "DownloadWarningSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A default value of -1 indicates configuration error.
constexpr base::FeatureParam<int> kDownloadWarningSurveyType{
    &kDownloadWarningSurvey, "survey_type", -1};

constexpr base::FeatureParam<int> kDownloadWarningSurveyIgnoreDelaySeconds{
    &kDownloadWarningSurvey, "ignore_delay_seconds", 300};

BASE_FEATURE(kEnhancedFieldsForSecOps,
             "EnhancedFieldsForSecOps",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnhancedSafeBrowsingPromo,
             "EnhancedSafeBrowsingPromo",
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnterpriseFileSystemAccessDeepScan,
             "EnterpriseFileSystemAccessDeepScan",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnterprisePasswordReuseUiRefresh,
             "EnterprisePasswordReuseUiRefresh",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnterpriseRealTimeUrlCheckOnAndroid,
             "EnterpriseRealTimeUrlCheckOnAndroid",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEsbAsASyncedSetting,
             "EsbAsASyncedSetting",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtendedReportingRemovePrefDependency,
             "ExtendedReportingRemovePrefDependency",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryConfiguration,
             "SafeBrowsingExtensionTelemetryConfiguration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryDeclarativeNetRequestActionSignal,
             "SafeBrowsingExtensionTelemetryDeclarativeNetRequestActionSignal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryFileDataForCommandLineExtensions,
             "SafeBrowsingExtensionTelemetryFileDataForCommandLineExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryForEnterprise,
             "SafeBrowsingExtensionTelemetryForEnterprise",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kExtensionTelemetryEnterpriseReportingIntervalSeconds{
        &kExtensionTelemetryForEnterprise, "EnterpriseReportingIntervalSeconds",
        /*default_value=*/300};

BASE_FEATURE(kExternalAppRedirectTelemetry,
             "SafeBrowsingExternalAppRedirectTelemetry",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGoogleStandardDeviceBoundSessionCredentials,
             "GoogleStandardDeviceBoundSessionCredentials",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGooglePlayProtectInApkTelemetry,
             "SafeBrowsingGooglePlayProtectInApkTelemetry",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGooglePlayProtectReducesWarnings,
             "SafeBrowsingGooglePlayProtectReducesWarnings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHashPrefixRealTimeLookups,
             "SafeBrowsingHashPrefixRealTimeLookups",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kHashPrefixRealTimeLookupsRelayUrl{
    &kHashPrefixRealTimeLookups,
    "SafeBrowsingHashPrefixRealTimeLookupsRelayUrl",
    /*default_value=*/
    "https://google-ohttp-relay-safebrowsing.fastly-edge.com/"};

BASE_FEATURE(kHashPrefixRealTimeLookupsSamplePing,
             "SafeBrowsingHashPrefixRealTimeLookupsSamplePing",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kHashPrefixRealTimeLookupsSampleRate{
    &kHashPrefixRealTimeLookupsSamplePing,
    "HashPrefixRealTimeLookupsSampleRate", /*default_value=*/100};

BASE_FEATURE(kLocalIpAddressInEvents,
             "LocalIpAddressInEvents",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLocalListsUseSBv5,
             "SafeBrowsingLocalListsUseSBv5",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMaliciousApkDownloadCheck,
             "MaliciousApkDownloadCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kMaliciousApkDownloadCheckTelemetryOnly,
                   &kMaliciousApkDownloadCheck,
                   "telemetry_only",
                   /*default_value=*/false);

BASE_FEATURE_PARAM(int,
                   kMaliciousApkDownloadCheckSamplePercentage,
                   &kMaliciousApkDownloadCheck,
                   "sample_percentage",
                   /*default_value=*/100);

constexpr base::FeatureParam<std::string>
    kMaliciousApkDownloadCheckServiceUrlOverride{&kMaliciousApkDownloadCheck,
                                                 "service_url_override",
                                                 /*default_value=*/""};
#endif

BASE_FEATURE(kOnDeviceNotificationContentDetectionModel,
             "OnDeviceNotificationContentDetectionModel",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationTelemetry,
             "NotificationTelemetry",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kOnDeviceNotificationContentDetectionModelAllowlistSamplingRate{
        &kOnDeviceNotificationContentDetectionModel,
        "OnDeviceNotificationContentDetectionModelAllowlistSamplingRate",
        /*default_value=*/0};

BASE_FEATURE(kRedWarningSurvey,
             "RedWarningSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<std::string> kRedWarningSurveyTriggerId{
    &kRedWarningSurvey, "RedWarningSurveyTriggerId", /*default_value=*/""};
constexpr base::FeatureParam<std::string> kRedWarningSurveyReportTypeFilter{
    &kRedWarningSurvey, "RedWarningSurveyReportTypeFilter",
    /*default_value=*/
    "URL_PHISHING,URL_MALWARE,URL_UNWANTED,URL_CLIENT_SIDE_PHISHING"};
constexpr base::FeatureParam<std::string> kRedWarningSurveyDidProceedFilter{
    &kRedWarningSurvey, "RedWarningSurveyDidProceedFilter",
    /*default_value=*/"TRUE,FALSE"};

BASE_FEATURE(kReportNotificationContentDetectionData,
             "ReportNotificationContentDetectionData",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kReportNotificationContentDetectionDataRate{
    &kReportNotificationContentDetectionData,
    "ReportNotificationContentDetectionDataRate",
    /*default_value=*/100};

BASE_FEATURE(kSafeBrowsingDailyPhishingReportsLimit,
             "SafeBrowsingDailyPhishingReportsLimit",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kSafeBrowsingDailyPhishingReportsLimitESB{
    &kSafeBrowsingDailyPhishingReportsLimit,
    /*name=*/"kMaxReportsPerIntervalESB", /*default_value=*/10};

BASE_FEATURE(kSafeBrowsingRemoveCookiesInAuthRequests,
             "SafeBrowsingRemoveCookiesInAuthRequests",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSafeBrowsingSyncCheckerCheckAllowlist,
             "SafeBrowsingSyncCheckerCheckAllowlist",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSafetyHubAbusiveNotificationRevocation,
             "SafetyHubAbusiveNotificationRevocation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSavePasswordHashFromProfilePicker,
             "SavePasswordHashFromProfilePicker",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShowWarningsForSuspiciousNotifications,
             "ShowWarningsForSuspiciousNotifications",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

constexpr base::FeatureParam<int>
    kShowWarningsForSuspiciousNotificationsScoreThreshold{
        &kShowWarningsForSuspiciousNotifications,
        "ShowWarningsForSuspiciousNotificationsScoreThreshold",
        /*default_value=*/90};

constexpr base::FeatureParam<bool>
    kShowWarningsForSuspiciousNotificationsShouldSwapButtons{
        &kShowWarningsForSuspiciousNotifications,
        "ShowWarningsForSuspiciousNotificationsShouldSwapButtons",
        /*default_value=*/false};

BASE_FEATURE(kSuspiciousSiteTriggerQuotaFeature,
             "SafeBrowsingSuspiciousSiteTriggerQuota",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTailoredSecurityIntegration,
             "TailoredSecurityIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreatDomDetailsTagAndAttributeFeature,
             "ThreatDomDetailsTagAttributes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVisualFeaturesSizes,
             "VisualFeaturesSizes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// keep-sorted end

// Returns the list of the experimental features that are enabled or disabled,
// as part of currently running Safe Browsing experiments.
base::Value::List GetFeatureStatusList() {
  // List of Safe Browsing feature that should be listed on
  // chrome://safe-browsing. Features should be listed in alphabetical order.
  const base::Feature* kExperimentalFeatures[] = {
      // keep-sorted start
      &kClientSideDetectionForcedLlamaRedirectChainKillswitch,
      &kClientSideDetectionKeyboardPointerLockRequest,
      &kClientSideDetectionKillswitch,
      &kClientSideDetectionNotificationPrompt,
      &kClientSideDetectionRedirectChainKillswitch,
      &kCreateNotificationsAcceptedClientSafeBrowsingReports,
      &kDelayedWarnings,
      &kDlpRegionalizedEndpoints,
      &kEnhancedFieldsForSecOps,
      &kEnhancedSafeBrowsingPromo,
      &kEnterprisePasswordReuseUiRefresh,
      &kEnterpriseRealTimeUrlCheckOnAndroid,
      &kExtensionTelemetryDeclarativeNetRequestActionSignal,
      &kExtensionTelemetryForEnterprise,
      &kExternalAppRedirectTelemetry,
      &kHashPrefixRealTimeLookups,
      &kLocalIpAddressInEvents,
      &kLocalListsUseSBv5,
      &kOnDeviceNotificationContentDetectionModel,
      &kReportNotificationContentDetectionData,
      &kSafeBrowsingRemoveCookiesInAuthRequests,
      &kSafetyHubAbusiveNotificationRevocation,
      &kShowWarningsForSuspiciousNotifications,
      &kSuspiciousSiteTriggerQuotaFeature,
      &kTailoredSecurityIntegration,
      &kVisualFeaturesSizes,
      // keep-sorted end
  };

  base::Value::List param_list;
  for (const base::Feature* feature : kExperimentalFeatures) {
    param_list.Append(feature->name);
    if (base::FeatureList::IsEnabled(*feature)) {
      param_list.Append("Enabled");
    } else {
      param_list.Append("Disabled");
    }
  }

  // Manually add experimental features that we want param values for.
  param_list.Append(kHashPrefixRealTimeLookupsRelayUrl.Get());
  param_list.Append(kHashPrefixRealTimeLookupsRelayUrl.name);

  return param_list;
}

}  // namespace safe_browsing
