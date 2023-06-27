// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/redaction_tool/inprocess_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"

namespace redaction {

std::unique_ptr<RedactionToolMetricsRecorder>
RedactionToolMetricsRecorder::Create() {
  return std::make_unique<InprocessMetricsRecorder>();
}

void InprocessMetricsRecorder::RecordPIIRedactedHistogram(PIIType pii_type) {
  UMA_HISTOGRAM_ENUMERATION(kPIIRedactedHistogram, pii_type);
}

void InprocessMetricsRecorder::RecordCreditCardRedactionHistogram(
    CreditCardDetection step) {
  UMA_HISTOGRAM_ENUMERATION(kCreditCardRedactionHistogram, step);
}

}  // namespace redaction
