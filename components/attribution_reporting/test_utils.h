// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_

#include <iosfwd>
#include <optional>

#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace attribution_reporting {

class AggregatableDebugReportingContribution;
class AggregatableNamedBudgetDefs;
class AggregatableNamedBudgetCandidate;
class AggregatableTriggerConfig;
class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
class AttributionScopesData;
class AttributionScopesSet;
class DestinationSet;
class EventReportWindows;
class RandomizedResponseData;
class SourceAggregatableDebugReportingConfig;
class SuitableOrigin;
class TriggerDataSet;

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

TriggerDataSet TriggerDataSetWithCardinality(int cardinality);

EventReportWindows EventReportWindowsWithCount(int num_report_windows);

std::ostream& operator<<(std::ostream&, const AggregationKeys&);

std::ostream& operator<<(std::ostream&, const FilterData&);

std::ostream& operator<<(std::ostream&, const FilterPair&);

std::ostream& operator<<(std::ostream&, const DestinationSet&);

std::ostream& operator<<(std::ostream&, const EventReportWindows&);

std::ostream& operator<<(std::ostream&, const AttributionScopesSet&);

std::ostream& operator<<(std::ostream&, const AttributionScopesData&);

std::ostream& operator<<(std::ostream&, const AggregatableNamedBudgetDefs&);

std::ostream& operator<<(std::ostream&, const SourceRegistration&);

std::ostream& operator<<(std::ostream&, const AggregatableValues&);

std::ostream& operator<<(std::ostream&, const AggregatableTriggerData&);

std::ostream& operator<<(std::ostream&, const EventTriggerData&);

std::ostream& operator<<(std::ostream&,
                         const AggregatableNamedBudgetCandidate&);

std::ostream& operator<<(std::ostream&, const TriggerRegistration&);

std::ostream& operator<<(std::ostream&, const SuitableOrigin&);

std::ostream& operator<<(std::ostream&, const AggregatableDedupKey&);

std::ostream& operator<<(std::ostream&, const OsRegistrationItem&);

std::ostream& operator<<(std::ostream&, const TriggerDataSet&);

std::ostream& operator<<(std::ostream&, const AggregatableTriggerConfig&);

std::ostream& operator<<(std::ostream&, const ParseError&);

std::ostream& operator<<(std::ostream& out, const FakeEventLevelReport&);

std::ostream& operator<<(std::ostream& out, const RandomizedResponseData&);

std::ostream& operator<<(std::ostream& out,
                         const AggregatableDebugReportingConfig&);

std::ostream& operator<<(std::ostream& out,
                         const SourceAggregatableDebugReportingConfig&);

std::ostream& operator<<(std::ostream& out,
                         const AggregatableDebugReportingContribution&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
