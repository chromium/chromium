// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/redaction_tool/inprocess_metrics_tester.h"

#include <string_view>

#include "components/feedback/redaction_tool/inprocess_metrics_recorder.h"

namespace redaction {

std::unique_ptr<MetricsTester> MetricsTester::Create() {
  return std::make_unique<InprocessMetricsTester>();
}

size_t InprocessMetricsTester::GetBucketCount(std::string_view histogram_name,
                                              int histogram_value) {
  return histogram_tester_.GetBucketCount(histogram_name, histogram_value);
}

size_t InprocessMetricsTester::GetNumBucketEntries(
    std::string_view histogram_name) {
  return histogram_tester_.GetAllSamples(histogram_name).size();
}

std::unique_ptr<RedactionToolMetricsRecorder>
InprocessMetricsTester::SetupRecorder() {
  return std::make_unique<InprocessMetricsRecorder>();
}

}  // namespace redaction
