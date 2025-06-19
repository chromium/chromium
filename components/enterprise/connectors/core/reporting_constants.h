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

inline constexpr char kEnterpriseWarnedSeenThreatType[] =
    "ENTERPRISE_WARNED_SEEN";
inline constexpr char kEnterpriseWarnedBypassTheatType[] =
    "ENTERPRISE_WARNED_BYPASS";
inline constexpr char kEnterpriseBlockedSeenThreatType[] =
    "ENTERPRISE_BLOCKED_SEEN";

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
inline constexpr char kPasswordReuseUmaMetricName[] =
    "Enterprise.ReportingEvent.PasswordReuse.";
inline constexpr char kPasswordChangedUmaMetricName[] =
    "Enterprise.ReportingEvent.PasswordChanged.";
inline constexpr char kDangerousDownloadUmaMetricName[] =
    "Enterprise.ReportingEvent.DangerousDownload.";
inline constexpr char kInterstitialUmaMetricName[] =
    "Enterprise.ReportingEvent.Interstitial.";
inline constexpr char kSensitiveDataUmaMetricName[] =
    "Enterprise.ReportingEvent.SensitiveData.";
inline constexpr char kUnscannedFileUmaMetricName[] =
    "Enterprise.ReportingEvent.UnscannedFile.";
inline constexpr char kLoginUmaMetricName[] =
    "Enterprise.ReportingEvent.Login.";
inline constexpr char kPasswordBreachUmaMetricName[] =
    "Enterprise.ReportingEvent.PasswordBreach.";
inline constexpr char kUrlFilteringInterstitialUmaMetricName[] =
    "Enterprise.ReportingEvent.UrlFilteringInterstitial.";
inline constexpr char kExtensionInstallUmaMetricName[] =
    "Enterprise.ReportingEvent.BrowserExtensionInstallEvent.";
inline constexpr char kBrowserCrashUmaMetricName[] =
    "Enterprise.ReportingEvent.BrowserCrash.";
inline constexpr char kExtensionTelemetryUmaMetricName[] =
    "Enterprise.ReportingEvent.ExtensionTelemetry.";
inline constexpr char kUnknownUmaMetricName[] =
    "Enterprise.ReportingEvent.Unknown.";

// Mapping from event name to UMA metric name.
inline constexpr auto kEventNameToUmaMetricNameMap =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{kKeyPasswordReuseEvent, kPasswordReuseUmaMetricName},
         {kKeyPasswordChangedEvent, kPasswordChangedUmaMetricName},
         {kKeyDangerousDownloadEvent, kDangerousDownloadUmaMetricName},
         {kKeyInterstitialEvent, kInterstitialUmaMetricName},
         {kKeySensitiveDataEvent, kSensitiveDataUmaMetricName},
         {kKeyUnscannedFileEvent, kUnscannedFileUmaMetricName},
         {kKeyLoginEvent, kLoginUmaMetricName},
         {kKeyPasswordBreachEvent, kPasswordBreachUmaMetricName},
         {kKeyUrlFilteringInterstitialEvent,
          kUrlFilteringInterstitialUmaMetricName},
         {kExtensionInstallEvent, kExtensionInstallUmaMetricName},
         {kBrowserCrashEvent, kBrowserCrashUmaMetricName},
         {kExtensionTelemetryEvent, kExtensionTelemetryUmaMetricName}});

// Mapping from event case to UMA metric name.
inline constexpr auto kEventCaseToUmaMetricNameMap =
    base::MakeFixedFlatMap<EventCase, std::string_view>(
        {{EventCase::kPasswordReuseEvent, kPasswordReuseUmaMetricName},
         {EventCase::kPasswordChangedEvent, kPasswordChangedUmaMetricName},
         {EventCase::kDangerousDownloadEvent, kDangerousDownloadUmaMetricName},
         {EventCase::kInterstitialEvent, kInterstitialUmaMetricName},
         {EventCase::kSensitiveDataEvent, kSensitiveDataUmaMetricName},
         {EventCase::kUnscannedFileEvent, kUnscannedFileUmaMetricName},
         {EventCase::kLoginEvent, kLoginUmaMetricName},
         {EventCase::kPasswordBreachEvent, kPasswordBreachUmaMetricName},
         {EventCase::kUrlFilteringInterstitialEvent,
          kUrlFilteringInterstitialUmaMetricName},
         {EventCase::kBrowserExtensionInstallEvent,
          kExtensionInstallUmaMetricName},
         {EventCase::kBrowserCrashEvent, kBrowserCrashUmaMetricName},
         {EventCase::kExtensionTelemetryEvent,
          kExtensionTelemetryUmaMetricName}});

std::string GetPayloadSizeUmaMetricName(std::string_view event_name);

std::string GetPayloadSizeUmaMetricName(EventCase event_case);

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
