// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/reporting_event_mappings.h"

#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/strings/strcat.h"
#include "components/enterprise/connectors/core/reporting_constants.h"

namespace enterprise_connectors {

namespace {

// Mapping from event case to UMA metric name.
constexpr auto kEventCaseToUmaMetricNameMap =
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
          kExtensionTelemetryUmaMetricName},
         {EventCase::kSaasUsageReportEvent, kSaasUsageUmaMetricName},
         {EventCase::kBrowserLaunchEvent, kBrowserLaunchUmaMetricName}});

// Mapping from event case to UMA metric name.
constexpr auto kEventCaseToEventNameMap =
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
         {EventCase::kExtensionTelemetryEvent, kExtensionTelemetryEvent},
         {EventCase::kSaasUsageReportEvent, kKeySaasUsageEvent},
         {EventCase::kBrowserLaunchEvent, kKeyBrowserLaunchEvent}});

}  // namespace

std::string GetPayloadSizeUmaMetricName(EventCase event_case) {
  auto* metric_name =
      base::FindOrNull(kEventCaseToUmaMetricNameMap, event_case);
  return metric_name ? base::StrCat({*metric_name, "UploadSize"})
                     : base::StrCat({kUnknownUmaMetricName, "UploadSize"});
}

std::string GetEventName(EventCase event_case) {
  const std::string_view* event_name =
      base::FindOrNull(kEventCaseToEventNameMap, event_case);

  return event_name ? std::string(*event_name) : std::string("UNKNOWN");
}

}  // namespace enterprise_connectors
