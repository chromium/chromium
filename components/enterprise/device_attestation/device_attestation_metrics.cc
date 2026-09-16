// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/device_attestation_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"

namespace enterprise {

namespace {

constexpr char kRequestedHistogram[] =
    "Enterprise.DeviceSignals.Attestation.Requested";
constexpr char kResultHistogram[] =
    "Enterprise.DeviceSignals.Attestation.Result";
constexpr char kBlobSizeHistogram[] =
    "Enterprise.DeviceSignals.Attestation.BlobSize";
constexpr char kSuccessLatencyHistogram[] =
    "Enterprise.DeviceSignals.Attestation.Success.Latency";
constexpr char kFailureLatencyHistogram[] =
    "Enterprise.DeviceSignals.Attestation.Failure.Latency";

// Blobs are expected to be in the low kilobytes. The range is wide enough that
// realistic growth stays resolvable rather than piling into the overflow
// bucket, at roughly 33% relative bucket width.
constexpr int kMinBlobSizeBytes = 1;
constexpr int kMaxBlobSizeBytes = 1000000;
constexpr int kBlobSizeBucketCount = 50;

}  // namespace

void LogAttestationBlobRequested() {
  base::UmaHistogramBoolean(kRequestedHistogram, true);
}

void LogAttestationBlobGenerated(base::TimeTicks start_time,
                                 const BlobGenerationResult& result) {
  const base::TimeDelta latency = base::TimeTicks::Now() - start_time;
  const bool success = result.IsSuccess();

  base::UmaHistogramEnumeration(
      kResultHistogram, success ? AttestationBlobResult::kSuccess
                                : AttestationBlobResult::kGenerationFailed);

  // Blob generation can involve a network round-trip, so use medium times
  // (up to 3 minutes) rather than the default 10 second maximum.
  base::UmaHistogramMediumTimes(
      success ? kSuccessLatencyHistogram : kFailureLatencyHistogram, latency);

  if (success) {
    base::UmaHistogramCustomCounts(
        kBlobSizeHistogram,
        base::saturated_cast<int>(result.attestation_blob.size()),
        kMinBlobSizeBytes, kMaxBlobSizeBytes, kBlobSizeBucketCount);
  }
}

void LogAttestationBlobServiceUnavailable() {
  base::UmaHistogramEnumeration(kResultHistogram,
                                AttestationBlobResult::kServiceUnavailable);
}

}  // namespace enterprise
