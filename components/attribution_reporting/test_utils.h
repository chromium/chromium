// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_

#include <iosfwd>

#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_type.mojom-forward.h"

namespace attribution_reporting {

class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
class DestinationSet;
class SuitableOrigin;

struct AggregatableDedupKey;
struct EventTriggerData;
struct SourceRegistration;
struct TriggerRegistration;

FiltersDisjunction FiltersForSourceType(mojom::SourceType);

bool operator==(const AggregationKeys&, const AggregationKeys&);

std::ostream& operator<<(std::ostream&, const AggregationKeys&);

bool operator==(const FilterData&, const FilterData&);

std::ostream& operator<<(std::ostream&, const FilterData&);

bool operator==(const FilterPair&, const FilterPair&);

std::ostream& operator<<(std::ostream&, const FilterPair&);

bool operator==(const DestinationSet&, const DestinationSet&);

std::ostream& operator<<(std::ostream&, const DestinationSet&);

bool operator==(const SourceRegistration&, const SourceRegistration&);

std::ostream& operator<<(std::ostream&, const SourceRegistration&);

bool operator==(const AggregatableValues&, const AggregatableValues&);

std::ostream& operator<<(std::ostream&, const AggregatableValues&);

bool operator==(const AggregatableTriggerData&, const AggregatableTriggerData&);

std::ostream& operator<<(std::ostream&, const AggregatableTriggerData&);

bool operator==(const EventTriggerData&, const EventTriggerData&);

std::ostream& operator<<(std::ostream&, const EventTriggerData&);

bool operator==(const TriggerRegistration&, const TriggerRegistration&);

std::ostream& operator<<(std::ostream&, const TriggerRegistration&);

bool operator==(const SuitableOrigin&, const SuitableOrigin&);

std::ostream& operator<<(std::ostream&, const SuitableOrigin&);

bool operator==(const AggregatableDedupKey&, const AggregatableDedupKey&);

std::ostream& operator<<(std::ostream&, const AggregatableDedupKey&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
