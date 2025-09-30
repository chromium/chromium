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

inline constexpr char kUnspecifiedDangerousDownloadThreatType[] =
    "DANGEROUS_DOWNLOAD_THREAT_TYPE_UNSPECIFIED";
inline constexpr char kDangerousDownloadThreatType[] = "DANGEROUS";
inline constexpr char kDangerousHostDownloadThreatType[] = "DANGEROUS_HOST";
inline constexpr char kPotentiallyUnwantedDownloadThreatType[] =
    "POTENTIALLY_UNWANTED";
inline constexpr char kUnknownDownloadThreatType[] = "UNKNOWN";
inline constexpr char kUncommonDownloadThreatType[] = "UNCOMMON";
inline constexpr char kDangerousFileTypeDownloadThreatType[] =
    "DANGEROUS_FILE_TYPE";
inline constexpr char kDangerousUrlDownloadThreatType[] = "DANGEROUS_URL";
inline constexpr char kDangerousAccountCompromiseDownloadThreatType[] =
    "DANGEROUS_ACCOUNT_COMPROMISE";

inline constexpr char kFilePasswordProtectedUnscannedReason[] =
    "FILE_PASSWORD_PROTECTED";
inline constexpr char kFileTooLargeUnscannedReason[] = "FILE_TOO_LARGE";
inline constexpr char kDlpScanFailedUnscannedReason[] = "DLP_SCAN_FAILED";
inline constexpr char kMalwareScanFailedUnscannedReason[] =
    "MALWARE_SCAN_FAILED";
inline constexpr char kDlpScanUnsupportedFileTypeUnscannedReason[] =
    "DLP_SCAN_UNSUPPORTED_FILE_TYPE";
inline constexpr char kMalwareScanUnsupportedFileTypeUnscannedReason[] =
    "MALWARE_SCAN_UNSUPPORTED_FILE_TYPE";
inline constexpr char kServiceUnavailableUnscannedReason[] =
    "SERVICE_UNAVAILABLE";
inline constexpr char kTooManyRequestsUnscannedReason[] = "TOO_MANY_REQUESTS";
inline constexpr char kTimeoutUnscannedReason[] = "TIMEOUT";

inline constexpr char kFileDownloadDataTransferEventTrigger[] = "FILE_DOWNLOAD";
inline constexpr char kFileUploadDataTransferEventTrigger[] = "FILE_UPLOAD";
inline constexpr char kWebContentUploadDataTransferEventTrigger[] =
    "WEB_CONTENT_UPLOAD";
inline constexpr char kPagePrintDataTransferEventTrigger[] = "PAGE_PRINT";
inline constexpr char kUrlVisitedDataTransferEventTrigger[] = "URL_VISITED";
inline constexpr char kClipboardCopyDataTransferEventTrigger[] =
    "CLIPBOARD_COPY";
inline constexpr char kFileTransferDataTransferEventTrigger[] = "FILE_TRANSFER";
inline constexpr char kPageLoadDataTransferEventTrigger[] = "PAGE_LOAD";
inline constexpr char kMutationDataTransferEventTrigger[] = "MUTATION";
inline constexpr char kMouseActionDataTransferEventTrigger[] = "MOUSE_ACTION";

inline constexpr char kContentTransferMethodUnknown[] =
    "CONTENT_TRANSFER_METHOD_UNKNOWN";
inline constexpr char kContentTransferMethodFilePicker[] =
    "CONTENT_TRANSFER_METHOD_FILE_PICKER";
inline constexpr char kContentTransferMethodDragAndDrop[] =
    "CONTENT_TRANSFER_METHOD_DRAG_AND_DROP";
inline constexpr char kContentTransferMethodFilePaste[] =
    "CONTENT_TRANSFER_METHOD_FILE_PASTE";

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

// Mapping from event case to UMA metric name.
inline constexpr auto kEventCaseToEventNameMap =
    base::MakeFixedFlatMap<EventCase, std::string_view>(
        {{EventCase::kPasswordReuseEvent, kKeyPasswordReuseEvent},
         {EventCase::kPasswordChangedEvent, kKeyPasswordChangedEvent},
         {EventCase::kDangerousDownloadEvent, kKeyDangerousDownloadEvent},
         {EventCase::kInterstitialEvent, kKeyInterstitialEvent},
         {EventCase::kSensitiveDataEvent, kKeySensitiveDataEvent},
         {EventCase::kUnscannedFileEvent, kKeyUnscannedFileEvent},
         {EventCase::kLoginEvent, kKeyLoginEvent},
         {EventCase::kPasswordBreachEvent, kKeyPasswordBreachEvent},
         {EventCase::kUrlFilteringInterstitialEvent,
          kKeyUrlFilteringInterstitialEvent},
         {EventCase::kBrowserExtensionInstallEvent, kExtensionInstallEvent},
         {EventCase::kBrowserCrashEvent, kBrowserCrashEvent},
         {EventCase::kExtensionTelemetryEvent, kExtensionTelemetryEvent}});

std::string GetPayloadSizeUmaMetricName(std::string_view event_name);

std::string GetPayloadSizeUmaMetricName(EventCase event_case);

std::string GetEventName(EventCase event_case);

// Key names used with when building the dictionary to pass to the real-time
// reporting API. Should be removed once the proto synced migration is complete.
inline constexpr char kKeyTrigger[] = "trigger";
inline constexpr char kKeyUrl[] = "url";
inline constexpr char kKeyTabUrl[] = "tabUrl";
inline constexpr char kKeySource[] = "source";
inline constexpr char kKeyDestination[] = "destination";
inline constexpr char kKeyDownloadDigestSha256[] = "downloadDigestSha256";
inline constexpr char kKeyFileName[] = "fileName";
inline constexpr char kKeyContentType[] = "contentType";
inline constexpr char kKeyUnscannedReason[] = "unscannedReason";
inline constexpr char kKeyContentSize[] = "contentSize";
inline constexpr char kKeyIsFederated[] = "isFederated";
inline constexpr char kKeyFederatedOrigin[] = "federatedOrigin";
inline constexpr char kKeyLoginUserName[] = "loginUserName";
inline constexpr char kKeyPasswordBreachIdentities[] = "identities";
inline constexpr char kKeyPasswordBreachIdentitiesUrl[] = "url";
inline constexpr char kKeyPasswordBreachIdentitiesUsername[] = "username";
inline constexpr char kKeyClickedThrough[] = "clickedThrough";
inline constexpr char kKeyContentTransferMethod[] = "contentTransferMethod";
inline constexpr char kKeyThreatType[] = "threatType";
inline constexpr char kKeyEventResult[] = "eventResult";
inline constexpr char kKeyTriggeredRuleName[] = "ruleName";
inline constexpr char kKeyTriggeredRuleId[] = "ruleId";
inline constexpr char kKeyTriggeredRuleInfo[] = "triggeredRuleInfo";
inline constexpr char kKeyUrlCategory[] = "urlCategory";
inline constexpr char kKeyAction[] = "action";
inline constexpr char kKeyHasWatermarking[] = "hasWatermarking";
inline constexpr char kKeyReason[] = "reason";
inline constexpr char kKeyScanId[] = "scanId";
inline constexpr char kKeyNetErrorCode[] = "netErrorCode";
inline constexpr char kKeyUserName[] = "userName";
inline constexpr char kKeyIframeUrls[] = "iframeUrls";
inline constexpr char kKeyIsPhishingUrl[] = "isPhishingUrl";
inline constexpr char kKeyReferrers[] = "referrers";
inline constexpr char kKeySourceWebAppSignedInAccount[] =
    "sourceWebAppSignedInAccount";
inline constexpr char kKeyWebAppSignedInAccount[] = "webAppSignedInAccount";
inline constexpr char kKeyUserJustification[] = "userJustification";

enum EnterpriseRealTimeUrlCheckMode {
  REAL_TIME_CHECK_DISABLED = 0,
  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED = 1,
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_
