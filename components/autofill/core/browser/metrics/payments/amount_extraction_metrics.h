// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AMOUNT_EXTRACTION_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AMOUNT_EXTRACTION_METRICS_H_

#include <optional>

#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill::autofill_metrics {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

// Logs the result of the amount extraction process. Its latency is measured
// from when the browser process initiates the search to the timepoint when the
// response is received. If the extraction process times out, latency will be
// nullopt.
void LogAmountExtractionResult(std::optional<base::TimeDelta> latency,
                               AmountExtractionResult result,
                               ukm::SourceId ukm_source_id);

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_PAYMENTS_AMOUNT_EXTRACTION_METRICS_H_
