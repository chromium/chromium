// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_

#include <array>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "components/enterprise/common/proto/synced_from_google3/chrome_reporting_entity.pb.h"

namespace enterprise_connectors {

// Alias to reduce verbosity when using Event::EventCase.
using EventCase = ::chrome::cros::reporting::proto::Event::EventCase;

inline constexpr char kExtensionInstallEvent[] = "browserExtensionInstallEvent";
inline constexpr char kExtensionTelemetryEvent[] = "extensionTelemetryEvent";
inline constexpr char kBrowserCrashEvent[] = "browserCrashEvent";
inline constexpr char kKeyUrlFilteringInterstitialEvent[] =
    "urlFilteringInterstitialEvent";
inline constexpr char kKeyPasswordReuseEvent[] = "passwordReuseEvent";
inline constexpr char kKeyPasswordChangedEvent[] = "passwordChangedEvent";
inline constexpr char kKeyDangerousDownloadEvent[] = "dangerousDownloadEvent";
inline constexpr char kKeyInterstitialEvent[] = "interstitialEvent";
inline constexpr char kKeySensitiveDataEvent[] = "sensitiveDataEvent";
inline constexpr char kKeyUnscannedFileEvent[] = "unscannedFileEvent";
inline constexpr char kKeyLoginEvent[] = "loginEvent";
inline constexpr char kKeyPasswordBreachEvent[] = "passwordBreachEvent";

// All the reporting events that can be set in the `enabled_events_names` field
// of `ReportingSettings`
inline constexpr std::array<const char*, 9> kAllReportingEnabledEvents = {
    kKeyPasswordReuseEvent,
    kKeyPasswordChangedEvent,
    kKeyDangerousDownloadEvent,
    kKeyInterstitialEvent,
    kKeySensitiveDataEvent,
    kKeyUnscannedFileEvent,
    kKeyUrlFilteringInterstitialEvent,
    kExtensionInstallEvent,
    kBrowserCrashEvent,
};

// All the reporting events that can be set in the `enabled_opt_in_events` field
// of `ReportingSettings`.
inline constexpr std::array<const char*, 3> kAllReportingOptInEvents = {
    kKeyLoginEvent,
    kKeyPasswordBreachEvent,
    kExtensionTelemetryEvent,
};

inline constexpr char kAllUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.All.UploadSize";
inline constexpr char kPasswordReuseUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.PasswordReuse.UploadSize";
inline constexpr char kPasswordChangedUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.PasswordChanged.UploadSize";
inline constexpr char kDangerousDownloadUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.DangerousDownload.UploadSize";
inline constexpr char kInterstitialUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.Interstitial.UploadSize";
inline constexpr char kSensitiveDataUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.SensitiveData.UploadSize";
inline constexpr char kUnscannedFileUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.UnscannedFile.UploadSize";
inline constexpr char kLoginUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.Login.UploadSize";
inline constexpr char kPasswordBreachUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.PasswordBreach.UploadSize";
inline constexpr char kUrlFilteringInterstitialUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.UrlFilteringInterstitial.UploadSize";
inline constexpr char kExtensionInstallUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.BrowserExtensionInstallEvent.UploadSize";
inline constexpr char kBrowserCrashUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.BrowserCrash.UploadSize";
inline constexpr char kExtensionTelemetryUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.ExtensionTelemetry.UploadSize";
inline constexpr char kUnknownUploadSizeUmaMetricName[] =
    "Enterprise.ReportingEvent.Unknown.UploadSize";

// Mapping from event name to UMA metric name for the payload size histogram.
inline constexpr auto kEventNameToUmaUploadSizeMetricNameMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{kKeyPasswordReuseEvent, kPasswordReuseUploadSizeUmaMetricName},
         {kKeyPasswordChangedEvent, kPasswordChangedUploadSizeUmaMetricName},
         {kKeyDangerousDownloadEvent,
          kDangerousDownloadUploadSizeUmaMetricName},
         {kKeyInterstitialEvent, kInterstitialUploadSizeUmaMetricName},
         {kKeySensitiveDataEvent, kSensitiveDataUploadSizeUmaMetricName},
         {kKeyUnscannedFileEvent, kUnscannedFileUploadSizeUmaMetricName},
         {kKeyLoginEvent, kLoginUploadSizeUmaMetricName},
         {kKeyPasswordBreachEvent, kPasswordBreachUploadSizeUmaMetricName},
         {kKeyUrlFilteringInterstitialEvent,
          kUrlFilteringInterstitialUploadSizeUmaMetricName},
         {kExtensionInstallEvent, kExtensionInstallUploadSizeUmaMetricName},
         {kBrowserCrashEvent, kBrowserCrashUploadSizeUmaMetricName},
         {kExtensionTelemetryEvent,
          kExtensionTelemetryUploadSizeUmaMetricName}});

// Mapping from event case to UMA metric name for the payload size histogram.
inline constexpr auto kEventCaseToUmaUploadSizeMetricNameMap =
    base::MakeFixedFlatMap<EventCase, std::string_view>(
        {{EventCase::kPasswordReuseEvent,
          kPasswordReuseUploadSizeUmaMetricName},
         {EventCase::kPasswordChangedEvent,
          kPasswordChangedUploadSizeUmaMetricName},
         {EventCase::kDangerousDownloadEvent,
          kDangerousDownloadUploadSizeUmaMetricName},
         {EventCase::kInterstitialEvent, kInterstitialUploadSizeUmaMetricName},
         {EventCase::kSensitiveDataEvent,
          kSensitiveDataUploadSizeUmaMetricName},
         {EventCase::kUnscannedFileEvent,
          kUnscannedFileUploadSizeUmaMetricName},
         {EventCase::kLoginEvent, kLoginUploadSizeUmaMetricName},
         {EventCase::kPasswordBreachEvent,
          kPasswordBreachUploadSizeUmaMetricName},
         {EventCase::kUrlFilteringInterstitialEvent,
          kUrlFilteringInterstitialUploadSizeUmaMetricName},
         {EventCase::kBrowserExtensionInstallEvent,
          kExtensionInstallUploadSizeUmaMetricName},
         {EventCase::kBrowserCrashEvent, kBrowserCrashUploadSizeUmaMetricName},
         {EventCase::kExtensionTelemetryEvent,
          kExtensionTelemetryUploadSizeUmaMetricName}});

std::string_view GetPayloadSizeUmaMetricName(std::string_view eventName);

std::string_view GetPayloadSizeUmaMetricName(EventCase eventCase);

// Key names used with when building the dictionary to pass to the real-time
// reporting API. Should be removed once the proto synced migration is complete.
inline constexpr char kKeyTrigger[] = "trigger";
inline constexpr char kKeyUrl[] = "url";
inline constexpr char kKeyIsFederated[] = "isFederated";
inline constexpr char kKeyFederatedOrigin[] = "federatedOrigin";
inline constexpr char kKeyLoginUserName[] = "loginUserName";
inline constexpr char kKeyPasswordBreachIdentities[] = "identities";
inline constexpr char kKeyPasswordBreachIdentitiesUrl[] = "url";
inline constexpr char kKeyPasswordBreachIdentitiesUsername[] = "username";
inline constexpr char kKeyClickedThrough[] = "clickedThrough";
inline constexpr char kKeyThreatType[] = "threatType";
inline constexpr char kKeyEventResult[] = "eventResult";
inline constexpr char kKeyTriggeredRuleName[] = "ruleName";
inline constexpr char kKeyTriggeredRuleId[] = "ruleId";
inline constexpr char kKeyTriggeredRuleInfo[] = "triggeredRuleInfo";
inline constexpr char kKeyUrlCategory[] = "urlCategory";
inline constexpr char kKeyAction[] = "action";
inline constexpr char kKeyHasWatermarking[] = "hasWatermarking";
inline constexpr char kKeyReason[] = "reason";
inline constexpr char kKeyNetErrorCode[] = "netErrorCode";
inline constexpr char kKeyUserName[] = "userName";
inline constexpr char kKeyIsPhishingUrl[] = "isPhishingUrl";
inline constexpr char kKeyReferrers[] = "referrers";

enum EnterpriseRealTimeUrlCheckMode {
  REAL_TIME_CHECK_DISABLED = 0,
  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED = 1,
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_
