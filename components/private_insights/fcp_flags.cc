// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/fcp_flags.h"

#include "components/private_insights/private_insights_features.h"

namespace private_insights {

FcpFlags::FcpFlags() = default;
FcpFlags::~FcpFlags() = default;

int64_t FcpFlags::max_resource_cache_size_bytes() const {
  // Chromium doesn't support FileBackedResourceCache.
  return 0;
}

bool FcpFlags::enable_lightweight_client_report_wire_format() const {
  return true;
}

bool FcpFlags::enable_confidential_aggregation() const {
  return true;
}

bool FcpFlags::enable_relative_uri_prefix() const {
  return true;
}

bool FcpFlags::enable_private_logger() const {
  return true;
}

bool FcpFlags::drop_out_based_data_availability() const {
  return true;
}

// This flag is tied to whether AttestationTransparencyVerifier
// is used in `fcp_simple_task_environment.cc`.
bool FcpFlags::enable_attestation_transparency_verifier() const {
  return kFcpUseAttestationTransparencyVerifier.Get();
}

int64_t FcpFlags::condition_polling_period_millis() const {
  return kFcpConditionPollingPeriod.Get().InMilliseconds();
}

bool FcpFlags::log_tensorflow_error_messages() const {
  return kFcpLogTensorflowErrorMessages.Get();
}

int64_t FcpFlags::tf_execution_teardown_grace_period_millis() const {
  return kFcpExecutionTeardownGracePeriod.Get().InMilliseconds();
}

int64_t FcpFlags::tf_execution_teardown_extended_period_millis() const {
  return kFcpExecutionTeardownExtendedPeriod.Get().InMilliseconds();
}

int32_t FcpFlags::http_retry_max_attempts() const {
  return kFcpHttpRetryMaxAttempts.Get();
}

int32_t FcpFlags::http_retry_delay_ms() const {
  return static_cast<int32_t>(kFcpHttpRetryDelay.Get().InMilliseconds());
}

bool FcpFlags::disable_http_request_body_compression() const {
  return kFcpDisableHttpRequestBodyCompression.Get();
}

int32_t FcpFlags::waiting_period_sec_for_cancellation() const {
  return static_cast<int32_t>(
      kFcpWaitingPeriodForCancellation.Get().InSeconds());
}

int64_t FcpFlags::federated_training_transient_errors_retry_delay_secs() const {
  return kFcpTransientErrorsRetryDelay.Get().InSeconds();
}

int64_t FcpFlags::federated_training_permanent_errors_retry_delay_secs() const {
  return kFcpPermanentErrorsRetryDelay.Get().InSeconds();
}

bool FcpFlags::enable_privacy_id_generation() const {
  return kFcpEnablePrivacyIdGeneration.Get();
}

}  // namespace private_insights
