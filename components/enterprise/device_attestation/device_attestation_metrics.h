// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_METRICS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_METRICS_H_

#include "base/time/time.h"

namespace enterprise {

struct BlobGenerationResult;

// Result of an attestation blob generation request. Do not reorder the
// values. Also change EnterpriseAttestationBlobResult in enums.xml if adding
// new values here.
// LINT.IfChange(AttestationBlobResult)
enum class AttestationBlobResult {
  // A blob was generated.
  kSuccess = 0,
  // Generation was attempted but did not produce a blob.
  kGenerationFailed = 1,
  // Generation could not be attempted because no attestation service was
  // available. No generation work was performed.
  kServiceUnavailable = 2,
  kMaxValue = kServiceUnavailable,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:EnterpriseAttestationBlobResult)

// Records that an attestation blob generation was requested. Must be called
// once at the start of every request, before any early return, so that it can
// act as the denominator for the result metric: requests whose callback never
// runs (e.g. the service is torn down mid-flight) are recorded here but have no
// corresponding result.
void LogAttestationBlobRequested();

// Records the result of an attempted blob generation, along with how long the
// attempt took and, on success, the size of the resulting blob. `start_time` is
// the time at which the generation was requested.
//
// This is platform-agnostic and is meant to be called by every
// `DeviceAttestationService` implementation so that the metrics are comparable
// across platforms.
void LogAttestationBlobGenerated(base::TimeTicks start_time,
                                 const BlobGenerationResult& result);

// Records that a blob generation could not be attempted because no attestation
// service was available. Deliberately records no latency: nothing was
// generated, so a duration here would be meaningless and would skew the
// latency distribution towards zero.
void LogAttestationBlobServiceUnavailable();

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_METRICS_H_
