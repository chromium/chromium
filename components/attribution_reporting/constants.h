// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_

#include <stddef.h>
#include <stdint.h>

#include "base/time/time.h"

namespace attribution_reporting {

inline constexpr uint64_t kDefaultFilteringId = 0;

inline constexpr size_t kMaxBytesPerFilterString = 25;
inline constexpr size_t kMaxValuesPerFilter = 50;
inline constexpr size_t kMaxFiltersPerSource = 50;

inline constexpr size_t kMaxDestinations = 3;

inline constexpr size_t kMaxEventLevelReportWindows = 5;

inline constexpr size_t kMaxAggregationKeysPerSource = 20;

inline constexpr int kMaxAggregatableValue = 65536;

inline constexpr base::TimeDelta kMinSourceExpiry = base::Days(1);
inline constexpr base::TimeDelta kMaxSourceExpiry = base::Days(30);

static_assert(kMinSourceExpiry < kMaxSourceExpiry);

inline constexpr base::TimeDelta kMinReportWindow = base::Hours(1);

static_assert(kMinReportWindow <= kMinSourceExpiry);

inline constexpr uint8_t kMaxSettableEventLevelAttributionsPerSource = 20;

// https://wicg.github.io/attribution-reporting-api/#max-distinct-trigger-data-per-source
inline constexpr uint8_t kMaxTriggerDataPerSource = 32;

inline constexpr size_t kMaxTriggerContextIdLength = 64;

inline constexpr uint32_t kDefaultMaxEventStates = 3;

inline constexpr size_t kMaxScopesPerSource = 20;
inline constexpr size_t kMaxLengthPerAttributionScope = 50;

inline constexpr char kTriggerDataMatchingExact[] = "exact";
inline constexpr char kTriggerDataMatchingModulus[] = "modulus";

inline constexpr char kSummaryOperatorCount[] = "count";
inline constexpr char kSummaryOperatorValueSum[] = "value_sum";

inline constexpr char kAggregatableFilteringIdsMaxBytes[] =
    "aggregatable_filtering_id_max_bytes";
inline constexpr char kAggregatableReportWindow[] =
    "aggregatable_report_window";
inline constexpr char kAggregationKeys[] = "aggregation_keys";
inline constexpr char kAttributionScopes[] = "attribution_scopes";
inline constexpr char kDestination[] = "destination";
inline constexpr char kDestinationLimitPriority[] =
    "destination_limit_priority";
inline constexpr char kEndTimes[] = "end_times";
inline constexpr char kEventLevelEpsilon[] = "event_level_epsilon";
inline constexpr char kEventReportWindow[] = "event_report_window";
inline constexpr char kEventReportWindows[] = "event_report_windows";
inline constexpr char kExpiry[] = "expiry";
inline constexpr char kFilterData[] = "filter_data";
inline constexpr char kLimit[] = "limit";
inline constexpr char kMaxEventLevelReports[] = "max_event_level_reports";
inline constexpr char kMaxEventStates[] = "max_event_states";
inline constexpr char kPriority[] = "priority";
inline constexpr char kSourceEventId[] = "source_event_id";
inline constexpr char kStartTime[] = "start_time";
inline constexpr char kSummaryBuckets[] = "summary_buckets";
inline constexpr char kSummaryOperator[] = "summary_operator";
inline constexpr char kTriggerData[] = "trigger_data";
inline constexpr char kTriggerDataMatching[] = "trigger_data_matching";
inline constexpr char kTriggerSpecs[] = "trigger_specs";

inline constexpr char kAggregatableDeduplicationKeys[] =
    "aggregatable_deduplication_keys";
inline constexpr char kAggregatableSourceRegistrationTime[] =
    "aggregatable_source_registration_time";
inline constexpr char kAggregatableTriggerData[] = "aggregatable_trigger_data";
inline constexpr char kAggregatableValues[] = "aggregatable_values";
inline constexpr char kAggregationCoordinatorOrigin[] =
    "aggregation_coordinator_origin";
inline constexpr char kDeduplicationKey[] = "deduplication_key";
inline constexpr char kEventTriggerData[] = "event_trigger_data";
inline constexpr char kFilteringId[] = "filtering_id";
inline constexpr char kFilters[] = "filters";
inline constexpr char kKeyPiece[] = "key_piece";
inline constexpr char kSourceKeys[] = "source_keys";
inline constexpr char kTriggerContextId[] = "trigger_context_id";
inline constexpr char kValue[] = "value";
inline constexpr char kValues[] = "values";

inline constexpr char kSourceRegistrationTimeInclude[] = "include";
inline constexpr char kSourceRegistrationTimeExclude[] = "exclude";

inline constexpr char kAttributionReportingRegisterSourceHeader[] =
    "Attribution-Reporting-Register-Source";

inline constexpr char kAttributionReportingRegisterTriggerHeader[] =
    "Attribution-Reporting-Register-Trigger";

inline constexpr char kAttributionReportingRegisterOsSourceHeader[] =
    "Attribution-Reporting-Register-OS-Source";

inline constexpr char kAttributionReportingRegisterOsTriggerHeader[] =
    "Attribution-Reporting-Register-OS-Trigger";

inline constexpr double kNullReportsRateIncludeSourceRegistrationTime = .008;
inline constexpr double kNullReportsRateExcludeSourceRegistrationTime = .05;

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_CONSTANTS_H_
