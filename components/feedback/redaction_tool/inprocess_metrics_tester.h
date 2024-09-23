// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_INPROCESS_METRICS_TESTER_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_INPROCESS_METRICS_TESTER_H_

#include <string_view>

#include "base/test/metrics/histogram_tester.h"
#include "components/feedback/redaction_tool/metrics_tester.h"

namespace redaction {

// A class used for testing to retrieve bucket values for histograms.
// This is the default implementation for the `MetricsTester` in Chromium.
class InprocessMetricsTester : public MetricsTester {
 public:
  InprocessMetricsTester() = default;
  InprocessMetricsTester(const InprocessMetricsTester&) = delete;
  InprocessMetricsTester& operator=(const InprocessMetricsTester&) = delete;
  ~InprocessMetricsTester() override = default;

  // redaction::MetricsTester:
  size_t GetBucketCount(std::string_view histogram_name,
                        int histogram_value) override;
  size_t GetNumBucketEntries(std::string_view histogram_name) override;
  std::unique_ptr<RedactionToolMetricsRecorder> SetupRecorder() override;

 private:
  const base::HistogramTester histogram_tester_;
};

}  // namespace redaction

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_INPROCESS_METRICS_TESTER_H_
