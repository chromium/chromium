// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_WEB_PAYMENTS_TELEMETRY_H_
#define COMPONENTS_PAYMENTS_CORE_WEB_PAYMENTS_TELEMETRY_H_

#include <string_view>

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace payments {

// All possible values for the 3D-Secure transaction status field. This is used
// in 3D-Secure Challenge Responses (cRes).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ThreeDSecureTransactionStatus)
enum class ThreeDSecureTransactionStatus {
  kUnknown = 0,
  kJSONEncrypted = 1,
  kSuccess = 2,
  kDenied = 3,
  kCouldNotBePerformed = 4,
  kAttemptsProcessingPerformed = 5,
  kChallengeRequired = 6,
  kChallengeRequiredDecoupled = 7,
  kRejected = 8,
  kInformationalOnly = 9,
  kChallengeUsingSPC = 10,
  kMaxValue = kChallengeUsingSPC,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/payment/enums.xml:ThreeDSecureTransactionStatus)

// Records metrics for 3DS (3D-Secure) Challenge Requests and Challenge
// Responses found in `form_data`, the body of an HTTP POST request from a form
// submission.
//
// `form_data` is parsed as `application/x-www-form-urlencoded` data. Each
// `creq` key (for Challenge Requests) or `cres` key (for Challenge Responses),
// matched case-insensitively, is counted. The `cres` value is also parsed to
// record the outcome of the response from its `transStatus` field. UKM entries
// are recorded against `source_id`.
void RecordThreeDSecureTelemetryFromFormData(std::string_view form_data,
                                             ukm::SourceId source_id);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_WEB_PAYMENTS_TELEMETRY_H_
