// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/test_utils.h"

#include <ostream>

#include "components/attribution_reporting/aggregation_keys.h"
#include "components/attribution_reporting/filters.h"

namespace attribution_reporting {

namespace {

std::ostream& WriteFilterValues(std::ostream& out,
                                const FilterValues& filter_values) {
  out << "{";

  const char* outer_separator = "";
  for (const auto& [filter, values] : filter_values) {
    out << outer_separator << filter << "=[";

    const char* inner_separator = "";
    for (const auto& value : values) {
      out << inner_separator << value;
      inner_separator = ", ";
    }

    out << "]";
    outer_separator = ", ";
  }

  return out << "}";
}

}  // namespace

bool operator==(const AggregationKeys& a, const AggregationKeys& b) {
  return a.keys() == b.keys();
}

std::ostream& operator<<(std::ostream& out,
                         const AggregationKeys& aggregation_keys) {
  out << "{";

  const char* separator = "";
  for (const auto& [key_id, key] : aggregation_keys.keys()) {
    out << separator << key_id << ":" << key;
    separator = ", ";
  }
  return out << "}";
}

bool operator==(const FilterData& a, const FilterData& b) {
  return a.filter_values() == b.filter_values();
}

std::ostream& operator<<(std::ostream& out, const FilterData& filter_data) {
  return WriteFilterValues(out, filter_data.filter_values());
}

bool operator==(const Filters& a, const Filters& b) {
  return a.filter_values() == b.filter_values();
}

std::ostream& operator<<(std::ostream& out, const Filters& filters) {
  return WriteFilterValues(out, filters.filter_values());
}

}  // namespace attribution_reporting
