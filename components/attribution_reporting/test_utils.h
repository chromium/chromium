// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_

#include <iosfwd>

#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"
#include "components/attribution_reporting/trigger_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace attribution_reporting {

class AggregatableTriggerConfig;
class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
class DestinationSet;
class EventReportWindows;
class SuitableOrigin;
class SummaryBuckets;

struct AggregatableDedupKey;
struct EventTriggerData;
struct OsRegistrationItem;
struct SourceRegistration;
struct TriggerRegistration;

FiltersDisjunction FiltersForSourceType(
    mojom::SourceType,
    absl::optional<base::TimeDelta> lookback_window = absl::nullopt);

std::ostream& operator<<(std::ostream&, const AggregationKeys&);

std::ostream& operator<<(std::ostream&, const FilterData&);

std::ostream& operator<<(std::ostream&, const FilterPair&);

std::ostream& operator<<(std::ostream&, const DestinationSet&);

std::ostream& operator<<(std::ostream&, const EventReportWindows&);

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

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
