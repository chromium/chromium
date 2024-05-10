// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_METRICS_TESTER_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_METRICS_TESTER_H_

#include <memory>
#include <string_view>

#include "components/feedback/redaction_tool/redaction_tool_metrics_recorder.h"

namespace redaction {

// A helper class for testing metric collection on Chromium and ChromiumOS.
class MetricsTester {
 public:
  // Create a new instance of the default implementation for this platform.
  static std::unique_ptr<MetricsTester> Create();

  MetricsTester() = default;
  MetricsTester(const MetricsTester&) = delete;
  MetricsTester& operator=(const MetricsTester&) = delete;
  virtual ~MetricsTester() = default;

  // Get the number of times a histogram value was recorded while this instance
  // exists and the values are recorded with the recorder from
  // `SetupRecorder()`.
  virtual size_t GetBucketCount(std::string_view histogram_name,
                                int histogram_value) = 0;

  // Get the number of entries for the `histogram_name`.
  virtual size_t GetNumBucketEntries(std::string_view histogram_name) = 0;

  // Create a `RedactionToolMetricsRecorder` that is configured so it can return
  // metric values in `GetBucketCount()`.
  virtual std::unique_ptr<RedactionToolMetricsRecorder> SetupRecorder() = 0;
};

}  // namespace redaction

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_METRICS_TESTER_H_
