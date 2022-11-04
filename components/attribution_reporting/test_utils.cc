// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/test_utils.h"

#include <ostream>

#include "components/attribution_reporting/aggregation_keys.h"

namespace attribution_reporting {

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

}  // namespace attribution_reporting
