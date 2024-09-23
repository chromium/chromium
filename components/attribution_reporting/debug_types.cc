// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/debug_types.h"

#include <string_view>

#include "base/containers/enum_set.h"
#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/debug_types.mojom.h"
#include "components/attribution_reporting/parsing_utils.h"

namespace attribution_reporting {

namespace {

using ::attribution_reporting::mojom::DebugDataType;

#define SOURCE_DEBUG_DATA_TYPES(X)                                             \
  X(kSourceChannelCapacityLimit, "source-channel-capacity-limit")              \
  X(kSourceDestinationGlobalRateLimit, "source-destination-global-rate-limit") \
  X(kSourceDestinationLimit, "source-destination-limit")                       \
  X(kSourceDestinationLimitReplaced, "source-destination-limit-replaced")      \
  X(kSourceDestinationPerDayRateLimit,                                         \
    "source-destination-per-day-rate-limit")                                   \
  X(kSourceDestinationRateLimit, "source-destination-rate-limit")              \
  X(kSourceMaxEventStatesLimit, "source-max-event-states-limit")               \
  X(kSourceNoised, "source-noised")                                            \
  X(kSourceReportingOriginLimit, "source-reporting-origin-limit")              \
  X(kSourceReportingOriginPerSiteLimit,                                        \
    "source-reporting-origin-per-site-limit")                                  \
  X(kSourceScopesChannelCapacityLimit, "source-scopes-channel-capacity-limit") \
  X(kSourceStorageLimit, "source-storage-limit")                               \
  X(kSourceSuccess, "source-success")                                          \
  X(kSourceTriggerStateCardinalityLimit,                                       \
    "source-trigger-state-cardinality-limit")                                  \
  X(kSourceUnknownError, "source-unknown-error")

#define TRIGGER_DEBUG_DATA_TYPES(X)                                           \
  X(kTriggerAggregateAttributionsPerSourceDestinationLimit,                   \
    "trigger-aggregate-attributions-per-source-destination-limit")            \
  X(kTriggerAggregateDeduplicated, "trigger-aggregate-deduplicated")          \
  X(kTriggerAggregateExcessiveReports, "trigger-aggregate-excessive-reports") \
  X(kTriggerAggregateInsufficientBudget,                                      \
    "trigger-aggregate-insufficient-budget")                                  \
  X(kTriggerAggregateNoContributions, "trigger-aggregate-no-contributions")   \
  X(kTriggerAggregateReportWindowPassed,                                      \
    "trigger-aggregate-report-window-passed")                                 \
  X(kTriggerAggregateStorageLimit, "trigger-aggregate-storage-limit")         \
  X(kTriggerEventAttributionsPerSourceDestinationLimit,                       \
    "trigger-event-attributions-per-source-destination-limit")                \
  X(kTriggerEventDeduplicated, "trigger-event-deduplicated")                  \
  X(kTriggerEventExcessiveReports, "trigger-event-excessive-reports")         \
  X(kTriggerEventLowPriority, "trigger-event-low-priority")                   \
  X(kTriggerEventNoMatchingConfigurations,                                    \
    "trigger-event-no-matching-configurations")                               \
  X(kTriggerEventNoMatchingTriggerData,                                       \
    "trigger-event-no-matching-trigger-data")                                 \
  X(kTriggerEventNoise, "trigger-event-noise")                                \
  X(kTriggerEventReportWindowNotStarted,                                      \
    "trigger-event-report-window-not-started")                                \
  X(kTriggerEventReportWindowPassed, "trigger-event-report-window-passed")    \
  X(kTriggerEventStorageLimit, "trigger-event-storage-limit")                 \
  X(kTriggerNoMatchingFilterData, "trigger-no-matching-filter-data")          \
  X(kTriggerNoMatchingSource, "trigger-no-matching-source")                   \
  X(kTriggerReportingOriginLimit, "trigger-reporting-origin-limit")           \
  X(kTriggerUnknownError, "trigger-unknown-error")

#define OTHER_DEBUG_DATA_TYPES(X)                \
  X(kHeaderParsingError, "header-parsing-error") \
  X(kOsSourceDelegated, "os-source-delegated")   \
  X(kOsTriggerDelegated, "os-trigger-delegated")

}  // namespace

std::string_view SerializeDebugDataType(DebugDataType data_type) {
#define TYPE_TO_STR(name, str) \
  case DebugDataType::name:    \
    return str;

  switch (data_type) {
    SOURCE_DEBUG_DATA_TYPES(TYPE_TO_STR)
    TRIGGER_DEBUG_DATA_TYPES(TYPE_TO_STR)
    OTHER_DEBUG_DATA_TYPES(TYPE_TO_STR)
  }

#undef TYPE_TO_STR

  NOTREACHED();
}

#define STR_TO_TYPE(name, str) {str, DebugDataType::name},

base::expected<DebugDataType, ParseError> ParseSourceDebugDataType(
    std::string_view str) {
  static constexpr auto kTypes =
      base::MakeFixedFlatMap<std::string_view, DebugDataType>(
          {SOURCE_DEBUG_DATA_TYPES(STR_TO_TYPE)});

  auto it = kTypes.find(str);
  if (it == kTypes.end()) {
    return base::unexpected(ParseError());
  }
  return it->second;
}

base::expected<DebugDataType, ParseError> ParseTriggerDebugDataType(
    std::string_view str) {
  static constexpr auto kTypes =
      base::MakeFixedFlatMap<std::string_view, DebugDataType>(
          {TRIGGER_DEBUG_DATA_TYPES(STR_TO_TYPE)});

  auto it = kTypes.find(str);
  if (it == kTypes.end()) {
    return base::unexpected(ParseError());
  }
  return it->second;
}

#undef STR_TO_TYPE

#define ENUM_NAME(name, str) DebugDataType::name,

DebugDataTypes SourceDebugDataTypes() {
  return {SOURCE_DEBUG_DATA_TYPES(ENUM_NAME)};
}

DebugDataTypes TriggerDebugDataTypes() {
  return {TRIGGER_DEBUG_DATA_TYPES(ENUM_NAME)};
}

#undef ENUM_NAME

#undef OTHER_DEBUG_DATA_TYPES
#undef TRIGGER_DEBUG_DATA_TYPES
#undef SOURCE_DEBUG_DATA_TYPES

}  // namespace attribution_reporting
