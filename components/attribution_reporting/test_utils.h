// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_

#include <ostream>

namespace attribution_reporting {

class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
struct EventTriggerData;
class FilterData;
class Filters;
class SourceRegistration;

bool operator==(const AggregationKeys&, const AggregationKeys&);

std::ostream& operator<<(std::ostream&, const AggregationKeys&);

bool operator==(const FilterData&, const FilterData&);

std::ostream& operator<<(std::ostream&, const FilterData&);

bool operator==(const Filters&, const Filters&);

std::ostream& operator<<(std::ostream&, const Filters&);

bool operator==(const SourceRegistration&, const SourceRegistration&);

std::ostream& operator<<(std::ostream&, const SourceRegistration&);

bool operator==(const AggregatableValues&, const AggregatableValues&);

std::ostream& operator<<(std::ostream&, const AggregatableValues&);

bool operator==(const AggregatableTriggerData&, const AggregatableTriggerData&);

std::ostream& operator<<(std::ostream&, const AggregatableTriggerData&);

bool operator==(const EventTriggerData&, const EventTriggerData&);

std::ostream& operator<<(std::ostream&, const EventTriggerData&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
