// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_

#include <string_view>
#include <utility>
#include <vector>

#include "base/metrics/metrics_hashes.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_interactions_flow.h"

namespace autofill::autofill_metrics {

FormSignature Collapse(FormSignature sig);

FieldSignature Collapse(FieldSignature sig);

struct UkmMetricNameAndValue {
  template <typename T>
  UkmMetricNameAndValue(std::string_view metric_name, const T& value)
      : metric_name(metric_name), value(static_cast<int64_t>(value)) {}

  friend bool operator==(const UkmMetricNameAndValue&,
                         const UkmMetricNameAndValue&) = default;

  std::string_view metric_name;
  int64_t value = 0;
};

void PrintTo(const UkmMetricNameAndValue& metric, std::ostream* os);

// Helper for UkmEventsAre().
std::vector<std::vector<UkmMetricNameAndValue>> GetUkmEvents(
    const ukm::TestUkmRecorder& ukm_recorder,
    std::string_view event_name);

// Matches `GetUkmEvents(ukm_recorder, event_name)` if the entries for the given
// `event_name` are equal to `expected_events`.
//
// Typical usage:
//   EXPECT_THAT(GetUkmEvents(ukm_recorder, UkmFoo:kEventName),
//               UkmEventsAre({{ /Event 1:*/ {UkmFoo::BarName, 123}, ... },
//                             { /Event 2:*/ {UkmFoo::QuxName, 456}, ... }});
//
// An entry for `event_name` and an `expected_events[i]` are equal if they both
// contain the same metrics and the same values for those metrics. The order
// of the UkmMetricNameAndValue in `expected_events[i]` does not matter.
//
// Metrics whose name is "MillisecondsSinceFormParsed" are given special
// treatment: all non-negative numbers are collapsed into 0.
testing::Matcher<const std::vector<std::vector<UkmMetricNameAndValue>>&>
UkmEventsAre(std::vector<std::vector<UkmMetricNameAndValue>> expected_events);

// Returns the URLs of all entries of the given `event_name`.
std::vector<GURL> GetEventUrls(const ukm::TestUkmRecorder& ukm_recorder,
                               std::string_view event_name);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_UKM_METRICS_TEST_UTILS_H_
