// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_

#include <array>

namespace enterprise_connectors {

inline constexpr char kExtensionInstallEvent[] = "browserExtensionInstallEvent";
inline constexpr char kExtensionTelemetryEvent[] = "extensionTelemetryEvent";
// This event is used to add DOM activity signals to the existing
// `extensionTelemetryEvent` and therefore is not added to the list of
// events in `kAllReportingEnabledEvents`. This is a separate opt-in
// event because processing DOM activity signals is resource intensive
// and should only be enabled when necessary.
inline constexpr char kExtensionDOMActivityEvent[] =
    "extensionDOMActivityEvent";
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
inline constexpr char kKeySaasUsageEvent[] = "saasUsageEvent";
inline constexpr char kKeyBrowserLaunchEvent[] = "browserLaunchEvent";

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
inline constexpr char kUserCancelledUnscannedReason[] = "USER_CANCELLED";

inline constexpr char kFileDownloadDataTransferEventTrigger[] = "FILE_DOWNLOAD";
inline constexpr char kFileUploadDataTransferEventTrigger[] = "FILE_UPLOAD";
inline constexpr char kWebContentUploadDataTransferEventTrigger[] =
    "WEB_CONTENT_UPLOAD";
inline constexpr char kPagePrintDataTransferEventTrigger[] = "PAGE_PRINT";
inline constexpr char kUrlVisitedDataTransferEventTrigger[] = "URL_VISITED";
inline constexpr char kClipboardCopyDataTransferEventTrigger[] =
    "CLIPBOARD_COPY";
inline constexpr char kNetworkRequestDataTransferEventTrigger[] =
    "NETWORK_REQUEST";
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
inline constexpr char kContentTransferMethodClipboardCopy[] =
    "CONTENT_TRANSFER_METHOD_CLIPBOARD_COPY";

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
inline constexpr char kSaasUsageUmaMetricName[] =
    "Enterprise.ReportingEvent.SaasUsage.";
inline constexpr char kBrowserLaunchUmaMetricName[] =
    "Enterprise.ReportingEvent.BrowserLaunch.";
inline constexpr char kUnknownUmaMetricName[] =
    "Enterprise.ReportingEvent.Unknown.";

enum EnterpriseRealTimeUrlCheckMode : int {
  REAL_TIME_CHECK_DISABLED = 0,
  REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED = 1,
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_
