// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/common/features.h"

#include <stddef.h>
#include <algorithm>
#include <utility>
#include "base/feature_list.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics_action.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/safe_browsing/buildflags.h"

namespace safe_browsing {
// Please define any new SafeBrowsing related features in this file, and add
// them to the ExperimentalFeaturesList below to start displaying their status
// on the chrome://safe-browsing page.

BASE_FEATURE(kAdSamplerTriggerFeature,
             "SafeBrowsingAdSamplerTrigger",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kAddWarningShownTSToClientSafeBrowsingReport,
             "AddWarningShownTSToClientSafeBrowsingReport",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionKillswitch,
             "ClientSideDetectionKillswitch",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionKeyboardPointerLockRequest,
             "ClientSideDetectionKeyboardPointerLockRequest",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionNotificationPrompt,
             "ClientSideDetectionNotificationPrompt",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionSamplePing,
             "ClientSideDetectionSamplePing",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateNotificationsAcceptedClientSafeBrowsingReports,
             "CreateNotificationsAcceptedClientSafeBrowsingReports",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kCreateWarningShownClientSafeBrowsingReports,
             "CreateWarningShownClientSafeBrowsingReports",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeepScanningPromptRemoval,
             "SafeBrowsingDeepScanningPromptRemoval",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDeepScanningCriteria,
             "SafeBrowsingDeepScanningCriteria",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDelayedWarnings,
             "SafeBrowsingDelayedWarnings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If true, a delayed warning will be shown when the user clicks on the page.
// If false, the warning won't be shown, but a metric will be recorded on the
// first click.
const base::FeatureParam<bool> kDelayedWarningsEnableMouseClicks{
    &kDelayedWarnings, "mouse",
    /*default_value=*/false};

BASE_FEATURE(kDlpRegionalizedEndpoints,
             "kDlpRegionalizedEndpoints",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadReportWithoutUserDecision,
             "DownloadReportWithoutUserDecision",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadTailoredWarnings,
             "DownloadTailoredWarnings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDownloadWarningSurvey,
             "DownloadWarningSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

// A default value of -1 indicates configuration error.
const base::FeatureParam<int> kDownloadWarningSurveyType{
    &kDownloadWarningSurvey, "survey_type", -1};

const base::FeatureParam<int> kDownloadWarningSurveyIgnoreDelaySeconds{
    &kDownloadWarningSurvey, "ignore_delay_seconds", 300};

BASE_FEATURE(kEncryptedArchivesMetadata,
             "SafeBrowsingEncryptedArchivesMetadata",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtendedReportingRemovePrefDependency,
             "ExtendedReportingRemovePrefDependency",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryConfiguration,
             "SafeBrowsingExtensionTelemetryConfiguration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryDeclarativeNetRequestActionSignal,
             "SafeBrowsingExtensionTelemetryDeclarativeNetRequestActionSignal",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryFileDataForCommandLineExtensions,
             "SafeBrowsingExtensionTelemetryFileDataForCommandLineExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryPotentialPasswordTheft,
             "SafeBrowsingExtensionTelemetryPotentialPasswordTheft",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryReportContactedHosts,
             "SafeBrowsingExtensionTelemetryReportContactedHosts",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryReportHostsContactedViaWebSocket,
             "SafeBrowsingExtensionTelemetryReportHostsContactedViaWebsocket",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(
    kExtensionTelemetryInterceptRemoteHostsContactedInRenderer,
    "SafeBrowsingExtensionTelmetryInterceptRemoteHostsContactedInRenderer",
    base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryTabsApiSignal,
             "SafeBrowsingExtensionTelemetryTabsApiSignal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryTabsApiSignalCaptureVisibleTab,
             "SafeBrowsingExtensionTelemetryTabsApiSignalCaptureVisibleTab",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryTabsExecuteScriptSignal,
             "SafeBrowsingExtensionTelemetryTabsExecuteScriptSignal",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kExtensionTelemetryDisableOffstoreExtensions,
             "SafeBrowsingExtensionTelemetryDisableOffstoreExtensions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFriendlierSafeBrowsingSettingsEnhancedProtection,
             "FriendlierSafeBrowsingSettingsEnhancedProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFriendlierSafeBrowsingSettingsStandardProtection,
             "FriendlierSafeBrowsingSettingsStandardProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHashPrefixRealTimeLookups,
             "SafeBrowsingHashPrefixRealTimeLookups",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

constexpr base::FeatureParam<std::string> kHashPrefixRealTimeLookupsRelayUrl{
    &kHashPrefixRealTimeLookups,
    "SafeBrowsingHashPrefixRealTimeLookupsRelayUrl",
    /*default_value=*/
    "https://google-ohttp-relay-safebrowsing.fastly-edge.com/"};

BASE_FEATURE(kImprovedDownloadPageWarnings,
             "ImprovedDownloadPageWarnings",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLogAccountEnhancedProtectionStateInProtegoPings,
             "TailoredSecurityLogAccountEnhancedProtectionStateInProtegoPings",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMaldocaSkipCheck,
             "MaldocaSkipCheck",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMmapSafeBrowsingDatabase,
             "MmapSafeBrowsingDatabase",
// TODO(crbug.com/40061554): Fix iOS tests with this enabled.
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

constexpr base::FeatureParam<bool> kMmapSafeBrowsingDatabaseAsync{
    &kMmapSafeBrowsingDatabase, "MmapSafeBrowsingDatabaseAsync",
// TODO(crbug.com/40061554): Fix iOS tests with this enabled.
#if BUILDFLAG(IS_IOS)
    /*default_value=*/false
#else
    /*default_value=*/true
#endif
};

BASE_FEATURE(kNestedArchives,
             "SafeBrowsingArchiveImprovements",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kRealTimeUrlFilteringCustomMessage,
             "RealTimeUrlFilteringCustomMessage",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kReferrerChainParameters,
             "SafeBrowsingReferrerChainParameters",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kReferrerChainEventMaximumAgeSeconds{
    &kReferrerChainParameters, "MaximumEventAgeSeconds", /*default_value=*/120};

constexpr base::FeatureParam<int> kReferrerChainEventMaximumCount{
    &kReferrerChainParameters, "MaximumEventCount",
    /*default_value=*/100};

BASE_FEATURE(kSafeBrowsingAsyncRealTimeCheck,
             "SafeBrowsingAsyncRealTimeCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kSafeBrowsingCallNewGmsApiOnStartup,
             "SafeBrowsingCallNewGmsApiOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingNewGmsApiForBrowseUrlDatabaseCheck,
             "SafeBrowsingNewGmsApiForBrowseUrlDatabaseCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingNewGmsApiForSubresourceFilterCheck,
             "SafeBrowsingNewGmsApiForSubresourceFilterCheck",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kSafeBrowsingOnUIThread,
             "SafeBrowsingOnUIThread",
// TODO(crbug.com/40061554): Fix iOS tests with this enabled.
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kSafeBrowsingReferrerChainWithCopyPasteNavigation,
             "SafeBrowsingReferrerChainWithCopyPasteNavigation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingRemoveCookiesInAuthRequests,
             "SafeBrowsingRemoveCookiesInAuthRequests",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingSkipSubresources2,
             "SafeBrowsingSkipSubResources2",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafetyHubAbusiveNotificationRevocation,
             "SafetyHubAbusiveNotificationRevocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSevenZipEvaluationEnabled,
             "SafeBrowsingSevenZipEvaluationEnabled",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSimplifiedUrlDisplay,
             "SimplifiedUrlDisplay",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kStrictDownloadTimeout,
             "SafeBrowsingStrictDownloadtimeout",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kStrictDownloadTimeoutMilliseconds{
    &kStrictDownloadTimeout, "TimeoutMilliseconds",
    /*default_value=*/7000};

BASE_FEATURE(kSuspiciousSiteDetectionRTLookups,
             "SuspiciousSiteDetectionRTLookups",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSuspiciousSiteTriggerQuotaFeature,
             "SafeBrowsingSuspiciousSiteTriggerQuota",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTailoredSecurityRetryForSyncUsers,
             "TailoredSecurityRetryForSyncUsers",
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
BASE_FEATURE(kTailoredSecurityObserverRetries,
             "TailoredSecurityObserverRetries",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kTailoredSecurityIntegration,
             "TailoredSecurityIntegration",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThreatDomDetailsTagAndAttributeFeature,
             "ThreatDomDetailsTagAttributes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kVisualFeaturesSizes,
             "VisualFeaturesSizes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionModelImageEmbedder,
             "ClientSideDetectionModelImageEmbedder",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingPhishingClassificationESBThreshold,
             "SafeBrowsingPhishingClassificationESBThreshold",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSafeBrowsingDailyPhishingReportsLimit,
             "SafeBrowsingDailyPhishingReportsLimit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionImagesCache,
             "ClientSideDetectionImagesCache",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClientSideDetectionDebuggingMetadataCache,
             "ClientSideDetectionDebuggingMetadataCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnhancedSafeBrowsingPromo,
             "EnhancedSafeBrowsingPromo",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<int> kSafeBrowsingDailyPhishingReportsLimitESB{
    &kSafeBrowsingDailyPhishingReportsLimit,
    /*name=*/"kMaxReportsPerIntervalESB", /*default_value=*/3};

namespace {
// List of Safe Browsing features. Boolean value for each list member should
// be set to true if the experiment state should be listed on
// chrome://safe-browsing. Features should be listed in alphabetical order.
constexpr struct {
  // This field is not a raw_ptr<> because it was filtered by the rewriter for:
  // #global-scope, #constexpr-var-initializer
  RAW_PTR_EXCLUSION const base::Feature* feature;
  // True if the feature's state should be listed on chrome://safe-browsing.
  bool show_state;
} kExperimentalFeatures[]{
    {&kAdSamplerTriggerFeature, false},
    {&kAddWarningShownTSToClientSafeBrowsingReport, false},
    {&kClientSideDetectionKillswitch, true},
    {&kClientSideDetectionKeyboardPointerLockRequest, true},
    {&kClientSideDetectionNotificationPrompt, true},
    {&kCreateNotificationsAcceptedClientSafeBrowsingReports, true},
    {&kCreateWarningShownClientSafeBrowsingReports, false},
    {&kDelayedWarnings, true},
    {&kDlpRegionalizedEndpoints, true},
    {&kDownloadReportWithoutUserDecision, true},
    {&kDownloadTailoredWarnings, true},
    {&kExtensionTelemetryDeclarativeNetRequestActionSignal, true},
    {&kExtensionTelemetryDisableOffstoreExtensions, true},
    {&kExtensionTelemetryInterceptRemoteHostsContactedInRenderer, true},
    {&kExtensionTelemetryPotentialPasswordTheft, true},
    {&kExtensionTelemetryReportContactedHosts, true},
    {&kExtensionTelemetryReportHostsContactedViaWebSocket, true},
    {&kExtensionTelemetryTabsApiSignal, true},
    {&kExtensionTelemetryTabsApiSignalCaptureVisibleTab, true},
    {&kExtensionTelemetryTabsExecuteScriptSignal, true},
    {&kHashPrefixRealTimeLookups, true},
    {&kImprovedDownloadPageWarnings, true},
    {&kLogAccountEnhancedProtectionStateInProtegoPings, true},
    {&kMmapSafeBrowsingDatabase, true},
    {&kNestedArchives, true},
    {&kRealTimeUrlFilteringCustomMessage, true},
    {&kSafeBrowsingAsyncRealTimeCheck, true},
    {&kSafeBrowsingRemoveCookiesInAuthRequests, true},
    {&kSafeBrowsingSkipSubresources2, true},
    {&kSafetyHubAbusiveNotificationRevocation, true},
    {&kSevenZipEvaluationEnabled, true},
    {&kSimplifiedUrlDisplay, true},
    {&kStrictDownloadTimeout, true},
    {&kSuspiciousSiteDetectionRTLookups, false},
    {&kSuspiciousSiteTriggerQuotaFeature, true},
    {&kTailoredSecurityIntegration, true},
    {&kThreatDomDetailsTagAndAttributeFeature, false},
    {&kVisualFeaturesSizes, true},
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
    if (feature_status.show_state) {
      AddFeatureAndAvailability(feature_status.feature, &param_list);
    }
  }

  // Manually add experimental features that we want param values for.
  param_list.Append(kHashPrefixRealTimeLookupsRelayUrl.Get());
  param_list.Append(kHashPrefixRealTimeLookupsRelayUrl.name);

  return param_list;
}

}  // namespace safe_browsing
