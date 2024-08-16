// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_

#include <iosfwd>
#include <optional>
#include <vector>

#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_config.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace attribution_reporting {

class AggregatableTriggerConfig;
class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
class AttributionScopesData;
class AttributionScopesSet;
class DestinationSet;
class EventReportWindows;
class MaxEventLevelReports;
class RandomizedResponseData;
class SourceAggregatableDebugReportingConfig;
class SuitableOrigin;
class SummaryBuckets;

struct AggregatableDebugReportingConfig;
struct AggregatableDedupKey;
struct EventTriggerData;
struct FakeEventLevelReport;
struct OsRegistrationItem;
struct ParseError;
struct SourceRegistration;
struct TriggerRegistration;

FiltersDisjunction FiltersForSourceType(
    mojom::SourceType,
    std::optional<base::TimeDelta> lookback_window = std::nullopt);

// Creates test data where each spec has daily windows (starting from 1 day).
// `collapse_into_single_spec` will collapse the vector into a single spec,
// assuming it is possible (i.e. `windows_per_type` contains a single distinct
// value).
TriggerSpecs SpecsFromWindowList(const std::vector<int>& windows_per_type,
                                 bool collapse_into_single_spec,
                                 MaxEventLevelReports);

std::ostream& operator<<(std::ostream&, const AggregationKeys&);

std::ostream& operator<<(std::ostream&, const FilterData&);

std::ostream& operator<<(std::ostream&, const FilterPair&);

std::ostream& operator<<(std::ostream&, const DestinationSet&);

std::ostream& operator<<(std::ostream&, const EventReportWindows&);

std::ostream& operator<<(std::ostream&, const AttributionScopesSet&);

std::ostream& operator<<(std::ostream&, const AttributionScopesData&);

std::ostream& operator<<(std::ostream&, const SourceRegistration&);

std::ostream& operator<<(std::ostream&, const AggregatableValues&);

std::ostream& operator<<(std::ostream&, const AggregatableTriggerData&);

std::ostream& operator<<(std::ostream&, const EventTriggerData&);

std::ostream& operator<<(std::ostream&, const TriggerRegistration&);

std::ostream& operator<<(std::ostream&, const SuitableOrigin&);

std::ostream& operator<<(std::ostream&, const AggregatableDedupKey&);

std::ostream& operator<<(std::ostream&, const OsRegistrationItem&);

std::ostream& operator<<(std::ostream&, const SummaryBuckets&);

std::ostream& operator<<(std::ostream&, const TriggerSpec&);

std::ostream& operator<<(std::ostream&, const TriggerSpecs&);

std::ostream& operator<<(std::ostream&, const TriggerSpecs::const_iterator&);

std::ostream& operator<<(std::ostream&, const AggregatableTriggerConfig&);

std::ostream& operator<<(std::ostream&, const ParseError&);

std::ostream& operator<<(std::ostream& out, const FakeEventLevelReport&);

std::ostream& operator<<(std::ostream& out, const RandomizedResponseData&);

std::ostream& operator<<(std::ostream& out,
                         const AggregatableDebugReportingConfig&);

std::ostream& operator<<(std::ostream& out,
                         const SourceAggregatableDebugReportingConfig&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
