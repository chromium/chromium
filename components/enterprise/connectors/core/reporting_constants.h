// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_

#include <array>

namespace enterprise_connectors {

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

// All events that the reporting connector supports.
inline constexpr std::array<const char*, 12> kAllReportingEvents = {
    kKeyPasswordReuseEvent,
    kKeyPasswordChangedEvent,
    kKeyDangerousDownloadEvent,
    kKeyInterstitialEvent,
    kKeySensitiveDataEvent,
    kKeyUnscannedFileEvent,
    kKeyLoginEvent,
    kKeyPasswordBreachEvent,
    kKeyUrlFilteringInterstitialEvent,
    kExtensionInstallEvent,
    kExtensionTelemetryEvent,
    kBrowserCrashEvent,
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_REPORTING_CONSTANTS_H_
