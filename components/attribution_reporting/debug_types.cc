// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/debug_types.h"

#include <string_view>

#include "components/attribution_reporting/debug_types.mojom.h"
#include "base/notreached.h"

namespace attribution_reporting {

namespace {
using ::attribution_reporting::mojom::DebugDataType;
}  // namespace

std::string_view SerializeDebugDataType(DebugDataType data_type) {
  switch (data_type) {
    case DebugDataType::kSourceDestinationLimit:
      return "source-destination-limit";
    case DebugDataType::kSourceNoised:
      return "source-noised";
    case DebugDataType::kSourceStorageLimit:
      return "source-storage-limit";
    case DebugDataType::kSourceSuccess:
      return "source-success";
    case DebugDataType::kSourceDestinationRateLimit:
      return "source-destination-rate-limit";
    case DebugDataType::kSourceUnknownError:
      return "source-unknown-error";
    case DebugDataType::kTriggerNoMatchingSource:
      return "trigger-no-matching-source";
    case DebugDataType::kTriggerEventAttributionsPerSourceDestinationLimit:
      return "trigger-event-attributions-per-source-destination-limit";
    case DebugDataType::kTriggerAggregateAttributionsPerSourceDestinationLimit:
      return "trigger-aggregate-attributions-per-source-destination-limit";
    case DebugDataType::kTriggerNoMatchingFilterData:
      return "trigger-no-matching-filter-data";
    case DebugDataType::kTriggerReportingOriginLimit:
      return "trigger-reporting-origin-limit";
    case DebugDataType::kTriggerEventDeduplicated:
      return "trigger-event-deduplicated";
    case DebugDataType::kTriggerEventNoMatchingConfigurations:
      return "trigger-event-no-matching-configurations";
    case DebugDataType::kTriggerEventNoise:
      return "trigger-event-noise";
    case DebugDataType::kTriggerEventLowPriority:
      return "trigger-event-low-priority";
    case DebugDataType::kTriggerEventExcessiveReports:
      return "trigger-event-excessive-reports";
    case DebugDataType::kTriggerEventStorageLimit:
      return "trigger-event-storage-limit";
    case DebugDataType::kTriggerEventReportWindowNotStarted:
      return "trigger-event-report-window-not-started";
    case DebugDataType::kTriggerEventReportWindowPassed:
      return "trigger-event-report-window-passed";
    case DebugDataType::kTriggerEventNoMatchingTriggerData:
      return "trigger-event-no-matching-trigger-data";
    case DebugDataType::kTriggerAggregateDeduplicated:
      return "trigger-aggregate-deduplicated";
    case DebugDataType::kTriggerAggregateNoContributions:
      return "trigger-aggregate-no-contributions";
    case DebugDataType::kTriggerAggregateInsufficientBudget:
      return "trigger-aggregate-insufficient-budget";
    case DebugDataType::kTriggerAggregateStorageLimit:
      return "trigger-aggregate-storage-limit";
    case DebugDataType::kTriggerAggregateReportWindowPassed:
      return "trigger-aggregate-report-window-passed";
    case DebugDataType::kTriggerAggregateExcessiveReports:
      return "trigger-aggregate-excessive-reports";
    case DebugDataType::kTriggerUnknownError:
      return "trigger-unknown-error";
    case DebugDataType::kOsSourceDelegated:
      return "os-source-delegated";
    case DebugDataType::kOsTriggerDelegated:
      return "os-trigger-delegated";
    case DebugDataType::kHeaderParsingError:
      return "header-parsing-error";
    case DebugDataType::kSourceReportingOriginPerSiteLimit:
      return "source-reporting-origin-per-site-limit";
    case DebugDataType::kSourceChannelCapacityLimit:
      return "source-channel-capacity-limit";
    case DebugDataType::kSourceTriggerStateCardinalityLimit:
      return "source-trigger-state-cardinality-limit";
  }
  NOTREACHED_NORETURN();
}

}  // namespace attribution_reporting
