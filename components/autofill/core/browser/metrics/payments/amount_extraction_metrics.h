// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AMOUNT_EXTRACTION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AMOUNT_EXTRACTION_METRICS_H_

#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"

namespace autofill::autofill_metrics {

enum class AmountExtractionComponentInstallationResult {
  // The component installation was successful.
  kSuccessful = 0,
  // The installation path is invalid.
  kInvalidInstallationPath = 1,
  // The binary file is empty.
  kEmptyBinaryFile = 2,
  // Reading from the binary file failed.
  kReadingBinaryFileFailed = 3,
  // The raw regex string was unable to be parsed into the proto.
  kParsingToProtoFailed = 4,
  // The generic details proto is empty.
  kEmptyGenericDetails = 5,
  kMaxValue = kEmptyGenericDetails,
};

enum class AmountExtractionResult {
  // The amount extraction was successful.
  kSuccessful = 0,
  // The amount extraction result was empty.
  kAmountNotFound = 1,
  // The amount extraction reached the timeout.
  kTimeout = 2,
  kMaxValue = kTimeout,
};

void LogAmountExtractionComponentInstallationResult(
    AmountExtractionComponentInstallationResult result);

// Logs the latency from the `AmountExtractionManager`, from the point of amount
// extraction initiation to when it finishes. Logged once a response from amount
// extraction is received.
void LogAmountExtractionLatency(base::TimeDelta latency, bool is_successful);

void LogAmountExtractionResult(AmountExtractionResult result);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AMOUNT_EXTRACTION_METRICS_H_
