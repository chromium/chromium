// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_

#include <stddef.h>

#include <ostream>
#include <vector>

#include "components/attribution_reporting/bounded_list.h"

namespace attribution_reporting {

class AggregatableTriggerData;
class AggregatableValues;
class AggregationKeys;
class DestinationSet;
class FilterData;
class Filters;
class SuitableOrigin;

struct AggregatableDedupKey;
struct EventTriggerData;
struct FilterPair;
struct SourceRegistration;
struct TriggerRegistration;

bool operator==(const AggregationKeys&, const AggregationKeys&);

std::ostream& operator<<(std::ostream&, const AggregationKeys&);

bool operator==(const FilterData&, const FilterData&);

std::ostream& operator<<(std::ostream&, const FilterData&);

bool operator==(const FilterPair&, const FilterPair&);

std::ostream& operator<<(std::ostream&, const FilterPair&);

bool operator==(const Filters&, const Filters&);

std::ostream& operator<<(std::ostream&, const Filters&);

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

template <typename T, size_t kMaxSize>
bool operator==(const BoundedList<T, kMaxSize>& a,
                const BoundedList<T, kMaxSize>& b) {
  return a.vec() == b.vec();
}

template <typename T, size_t kMaxSize>
std::ostream& operator<<(std::ostream& out,
                         const BoundedList<T, kMaxSize>& list) {
  out << "[";

  const char* separator = "";
  for (const auto& item : list.vec()) {
    out << separator << item;
    separator = ", ";
  }

  return out << "]";
}

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_TEST_UTILS_H_
