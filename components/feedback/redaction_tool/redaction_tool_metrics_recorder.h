// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_REDACTION_TOOL_METRICS_RECORDER_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_REDACTION_TOOL_METRICS_RECORDER_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/feedback/redaction_tool/pii_types.h"

namespace redaction {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "CreditCardDetection" in //tools/metrics/histograms/enums.xml.
enum class CreditCardDetection {
  kRegexMatch = 1,
  kTimestamp = 2,
  kRepeatedChars = 3,
  kDoesntValidate = 4,
  kValidated = 5,
  kMaxValue = kValidated,
};

inline constexpr char kPIIRedactedHistogram[] = "Feedback.RedactionTool";
inline constexpr char kCreditCardRedactionHistogram[] =
    "Feedback.RedactionTool.CreditCardMatch";

// This class is the platform independent interface to record histograms using
// the platform specific libraries.
class RedactionToolMetricsRecorder {
 public:
  // Create a new instance of the platform default implementation.
  static std::unique_ptr<RedactionToolMetricsRecorder> Create();

  RedactionToolMetricsRecorder() = default;
  RedactionToolMetricsRecorder(const RedactionToolMetricsRecorder&) = delete;
  RedactionToolMetricsRecorder& operator=(const RedactionToolMetricsRecorder&) =
      delete;
  virtual ~RedactionToolMetricsRecorder() = default;

  // Record when a bit of PII of the type `pii_type` was redacted by the
  // redaction tool.
  virtual void RecordPIIRedactedHistogram(PIIType pii_type) = 0;

  // Records the `step` that was reached when validating a series of numbers
  // against credit card checks.
  virtual void RecordCreditCardRedactionHistogram(CreditCardDetection step) = 0;

  // Records the `time_spent` in milliseconds for redacting an input text.
  virtual void RecordTimeSpentRedactingHistogram(
      base::TimeDelta time_spent) = 0;

  // Returns the platform specific metric name used to measure how long it took
  // to redact the input.
  static base::StringPiece GetTimeSpentRedactingHistogramNameForTesting();
};

}  // namespace redaction

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_REDACTION_TOOL_METRICS_RECORDER_H_
