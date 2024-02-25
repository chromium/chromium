// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_INPROCESS_METRICS_RECORDER_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_INPROCESS_METRICS_RECORDER_H_

#include "components/feedback/redaction_tool/redaction_tool_metrics_recorder.h"

namespace redaction {

// Record the metrics and store them in a memory location of the program.
// This is the default implementation for the `RedactionToolMetricsRecorder` in
// Chromium.
class InprocessMetricsRecorder : public RedactionToolMetricsRecorder {
 public:
  InprocessMetricsRecorder() = default;
  InprocessMetricsRecorder(const InprocessMetricsRecorder&) = delete;
  InprocessMetricsRecorder& operator=(const InprocessMetricsRecorder&) = delete;
  ~InprocessMetricsRecorder() override = default;

  // redaction::RedactionToolMetricsRecorder:
  void RecordPIIRedactedHistogram(PIIType pii_type) override;
  void RecordCreditCardRedactionHistogram(CreditCardDetection step) override;
  void RecordRedactionToolCallerHistogram(RedactionToolCaller step) override;
  void RecordTimeSpentRedactingHistogram(base::TimeDelta time_spent) override;
};

}  // namespace redaction

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_INPROCESS_METRICS_RECORDER_H_
