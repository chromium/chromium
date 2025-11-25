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
//
// These keep-sorted instructions group blocks without newlines, and then sort
// those blocks by their BASE_FEATURE. It's strongly recommended to keep a
// FeatureParam associated with the Feature by removing and newlines between
// them.
//
// clang-format off
// keep-sorted start allow_yaml_lists=yes sticky_prefixes=[""] group_prefixes=["#if", "#else", "#endif", "constexpr base::FeatureParam", "//", "BASE_FEATURE", "BASE_FEATURE_PARAM", ");"] by_regex=["BASE_FEATURE\\(.*,"] skip_lines=2
// clang-format on

BASE_FEATURE(kAdSamplerTriggerFeature,
             "SafeBrowsingAdSamplerTrigger",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAddWarningShownTSToClientSafeBrowsingReport,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAutoRevokeSuspiciousNotification,
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int>
    kAutoRevokeSuspiciousNotificationLookBackPeriod{
        &kAutoRevokeSuspiciousNotification, "LookBackPeriod",
        /*default_value=*/1};
constexpr base::FeatureParam<double>
    kAutoRevokeSuspiciousNotificationEngagementScoreCutOff{
        &kAutoRevokeSuspiciousNotification, "MaxEngagementScore",
        /*default_value=*/50.0};
constexpr base::FeatureParam<int>
    kAutoRevokeSuspiciousNotificationMinNotificationCount{
        &kAutoRevokeSuspiciousNotification, "MinNotificationCount",
        /*default_value=*/2};

BASE_FEATURE(kBundledSecuritySettings, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionClipboardCopyApi,
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<double> kCsdClipboardCopyApiHCAcceptanceRate{
    &kClientSideDetectionClipboardCopyApi, "HCAcceptanceRate",
    /*default_value=*/0.0};
constexpr base::FeatureParam<double> kCsdClipboardCopyApiSampleRate{
    &kClientSideDetectionClipboardCopyApi, "SampleRate",
    /*default_value=*/0.0};
constexpr base::FeatureParam<int> kCsdClipboardCopyApiMaxLength{
    &kClientSideDetectionClipboardCopyApi, "MaxLength",
    /*default_value=*/1000};
constexpr base::FeatureParam<int> kCsdClipboardCopyApiMinLength{
    &kClientSideDetectionClipboardCopyApi, "MinLength",
    /*default_value=*/0};
const base::FeatureParam<bool> kCSDClipboardCopyApiProcessPayload{
    &kClientSideDetectionClipboardCopyApi, "ProcessPayload",
    /*default_value=*/false};

BASE_FEATURE(kClientSideDetectionCreditCardForm,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<double> kCsdCreditCardFormHCAcceptanceRate{
    &kClientSideDetectionCreditCardForm, "HCAcceptanceRate",
    /*default_value=*/1.0};
const base::FeatureParam<double> kCsdCreditCardFormSampleRate{
    &kClientSideDetectionCreditCardForm, "SampleRate",
    /*default_value=*/0.0};
const base::FeatureParam<int> kCsdCreditCardFormMaxUserVisit{
    &kClientSideDetectionCreditCardForm, "MaxUserVisit",
    /*default_value=*/1};
const base::FeatureParam<bool> kCsdCreditCardFormPingOnDetection{
    &kClientSideDetectionCreditCardForm, "PingOnDetection",
    /*default_value=*/false};
const base::FeatureParam<bool> kCsdCreditCardFormPingOnInteraction{
    &kClientSideDetectionCreditCardForm, "PingOnInteraction",
    /*default_value=*/false};
const base::FeatureParam<bool> kCsdCreditCardFormEnableNewSiteFilter{
    &kClientSideDetectionCreditCardForm, "EnableNewSiteFilter",
    /*default_value=*/false};
const base::FeatureParam<bool> kCsdCreditCardFormEnableHeuristicFilter{
    &kClientSideDetectionCreditCardForm, "EnableHeuristicFilter",
    /*default_value=*/false};
const base::FeatureParam<bool> kCsdCreditCardFormEnableReferringAppFilter{
    &kClientSideDetectionCreditCardForm, "EnableReferringAppFilter",
    /*default_value=*/false};

BASE_FEATURE(kClientSideDetectionForcedLlamaRedirectChainKillswitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionKillswitch, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionLlamaForcedTriggerInfoForScamDetection,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionRedirectChainKillswitch,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionRetryLimit, base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kClientSideDetectionRetryLimitTime{
    &kClientSideDetectionRetryLimit, /*name=*/"RetryTimeMax",
    /*default_value=*/15};

BASE_FEATURE(kClientSideDetectionSamplePing, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kClientSideDetectionSendIntelligentScanInfoAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kClientSideDetectionSendLlamaForcedTriggerInfo,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionShowLlamaScamVerdictWarning,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kClientSideDetectionShowScamVerdictWarningAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kClientSideDetectionVibrationApi,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kConditionalImageResize, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateNotificationsAcceptedClientSafeBrowsingReports,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCreateWarningShownClientSafeBrowsingReports,
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

BASE_FEATURE(kDlpRegionalizedEndpoints, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadWarningSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
// A default value of -1 indicates configuration error.
constexpr base::FeatureParam<int> kDownloadWarningSurveyType{
    &kDownloadWarningSurvey, "survey_type", -1};
constexpr base::FeatureParam<int> kDownloadWarningSurveyIgnoreDelaySeconds{
    &kDownloadWarningSurvey, "ignore_delay_seconds", 300};

BASE_FEATURE(kEnhancedFieldsForSecOps,
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kEnhancedSafeBrowsingPromo,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kEnterpriseFileSystemAccessDeepScan,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnterprisePasswordReuseUiRefresh,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEsbAsASyncedSetting,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kExtendedReportingRemovePrefDependency,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtendedReportingRemovePrefDependencyIos,
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

BASE_FEATURE(kExtensionTelemetrySearchHijackingSignal,
             "SafeBrowsingExtensionTelemetrySearchHijackingSignal",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int>
    kExtensionTelemetrySearchHijackingSignalHeuristicCheckIntervalSeconds{
        &kExtensionTelemetrySearchHijackingSignal,
        "HeuristicCheckIntervalSeconds", 28800 /* 8 hours */};
constexpr base::FeatureParam<int>
    kExtensionTelemetrySearchHijackingSignalHeuristicThreshold{
        &kExtensionTelemetrySearchHijackingSignal, "HeuristicThreshold", 2};

BASE_FEATURE(kExternalAppRedirectTelemetry,
             "SafeBrowsingExternalAppRedirectTelemetry",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGlobalCacheListForGatingNotificationProtections,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGooglePlayProtectInApkTelemetry,
             "SafeBrowsingGooglePlayProtectInApkTelemetry",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGooglePlayProtectReducesWarnings,
             "SafeBrowsingGooglePlayProtectReducesWarnings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kGoogleStandardDeviceBoundSessionCredentials,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kHashPrefixRealTimeLookups,
             "SafeBrowsingHashPrefixRealTimeLookups",
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<std::string> kHashPrefixRealTimeLookupsRelayUrl{
    &kHashPrefixRealTimeLookups,
    "SafeBrowsingHashPrefixRealTimeLookupsRelayUrl",
    /*default_value=*/
    "https://google-ohttp-relay-safebrowsing.fastly-edge.com/"};
constexpr base::FeatureParam<std::string> kHashPrefixRealTimeLookupsKeyFetchUrl{
    &kHashPrefixRealTimeLookups,
    "SafeBrowsingHashPrefixRealTimeLookupsKeyFetchUrl",
    /*default_value=*/
    "https://safebrowsingohttpgateway.googleapis.com/v1/ohttp/hpkekeyconfig"};

BASE_FEATURE(kHashPrefixRealTimeLookupsSamplePing,
             "SafeBrowsingHashPrefixRealTimeLookupsSamplePing",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kHashPrefixRealTimeLookupsSampleRate{
    &kHashPrefixRealTimeLookupsSamplePing,
    "HashPrefixRealTimeLookupsSampleRate", /*default_value=*/100};

BASE_FEATURE(kLocalListsUseSBv5,
             "SafeBrowsingLocalListsUseSBv5",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kMaliciousApkDownloadCheck, base::FEATURE_DISABLED_BY_DEFAULT);
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

BASE_FEATURE(kModifiedESBFetchErrorHandling, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMovePasswordLeakDetectionToggleIos,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationTelemetry, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNotificationTelemetrySwb, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<bool> kNotificationTelemetrySwbSendReports{
    &kNotificationTelemetrySwb, "NotificationTelemetrySwbSendReports",
    /*default_value=*/true};
constexpr base::FeatureParam<int> kNotificationTelemetrySwbPollingInterval{
    &kNotificationTelemetrySwb, "NotificationTelemetrySwbPollingInterval",
    /*default_value=*/60};

BASE_FEATURE(kRedWarningSurvey, base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<std::string> kRedWarningSurveyTriggerId{
    &kRedWarningSurvey, "RedWarningSurveyTriggerId", /*default_value=*/""};
constexpr base::FeatureParam<std::string> kRedWarningSurveyReportTypeFilter{
    &kRedWarningSurvey, "RedWarningSurveyReportTypeFilter",
    /*default_value=*/
    "URL_PHISHING,URL_MALWARE,URL_UNWANTED,URL_CLIENT_SIDE_PHISHING"};
constexpr base::FeatureParam<std::string> kRedWarningSurveyDidProceedFilter{
    &kRedWarningSurvey, "RedWarningSurveyDidProceedFilter",
    /*default_value=*/"TRUE,FALSE"};

BASE_FEATURE(kRelaunchNotificationForAdvancedProtection,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReportNotificationContentDetectionData,
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kReportNotificationContentDetectionDataRate{
    &kReportNotificationContentDetectionData,
    "ReportNotificationContentDetectionDataRate",
    /*default_value=*/100};

BASE_FEATURE(kSafeBrowsingDailyPhishingReportsLimit,
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr base::FeatureParam<int> kSafeBrowsingDailyPhishingReportsLimitESB{
    &kSafeBrowsingDailyPhishingReportsLimit,
    /*name=*/"kMaxReportsPerIntervalESB", /*default_value=*/10};

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSafeBrowsingSyncCheckerCheckAllowlist,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSavePasswordHashFromProfilePicker,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShowManualNotificationRevocationsSafetyHub,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kShowWarningsForSuspiciousNotifications,
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
        /*default_value=*/70};
constexpr base::FeatureParam<bool>
    kShowWarningsForSuspiciousNotificationsShouldSwapButtons{
        &kShowWarningsForSuspiciousNotifications,
        "ShowWarningsForSuspiciousNotificationsShouldSwapButtons",
        /*default_value=*/false};

BASE_FEATURE(kSuspiciousSiteTriggerQuotaFeature,
             "SafeBrowsingSuspiciousSiteTriggerQuota",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTailoredSecurityIntegration, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreatDomDetailsTagAndAttributeFeature,
             "ThreatDomDetailsTagAttributes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVisualFeaturesSizes, base::FEATURE_DISABLED_BY_DEFAULT);

// keep-sorted end

// Returns the list of the experimental features that are enabled or disabled,
// as part of currently running Safe Browsing experiments.
base::Value::List GetFeatureStatusList() {
  // List of Safe Browsing feature that should be listed on
  // chrome://safe-browsing. Features should be listed in alphabetical order.
  const base::Feature* kExperimentalFeatures[] = {
      // keep-sorted start
      &kAutoRevokeSuspiciousNotification,
      &kBundledSecuritySettings,
      &kClientSideDetectionClipboardCopyApi,
      &kClientSideDetectionForcedLlamaRedirectChainKillswitch,
      &kClientSideDetectionKillswitch,
      &kClientSideDetectionRedirectChainKillswitch,
      &kCreateNotificationsAcceptedClientSafeBrowsingReports,
      &kDelayedWarnings,
      &kDlpRegionalizedEndpoints,
      &kEnhancedFieldsForSecOps,
      &kEnhancedSafeBrowsingPromo,
      &kEnterprisePasswordReuseUiRefresh,
      &kEsbAsASyncedSetting,
      &kExtensionTelemetryDeclarativeNetRequestActionSignal,
      &kExternalAppRedirectTelemetry,
      &kHashPrefixRealTimeLookups,
      &kLocalListsUseSBv5,
      &kNotificationTelemetrySwb,
      &kReportNotificationContentDetectionData,
      &kShowManualNotificationRevocationsSafetyHub,
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
  param_list.Append(kHashPrefixRealTimeLookupsKeyFetchUrl.Get());
  param_list.Append(kHashPrefixRealTimeLookupsKeyFetchUrl.name);

  return param_list;
}

}  // namespace safe_browsing
