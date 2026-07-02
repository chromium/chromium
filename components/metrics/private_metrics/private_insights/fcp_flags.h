// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_FLAGS_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_FLAGS_H_

#include <cstdint>

#include "third_party/federated_compute/src/fcp/client/flags.h"

namespace private_insights {

// Implementation of fcp::client::Flags for Private Insights in Chromium.
// This class provides configurable runtime flags for the Federated Compute
// Platform (FCP) client.
class FcpFlags : public fcp::client::Flags {
 public:
  FcpFlags();
  ~FcpFlags() override;

  FcpFlags(const FcpFlags&) = delete;
  FcpFlags& operator=(const FcpFlags&) = delete;

  // Non configurable flags.
  int64_t max_resource_cache_size_bytes() const override;
  bool enable_lightweight_client_report_wire_format() const override;
  bool enable_confidential_aggregation() const override;
  bool enable_relative_uri_prefix() const override;
  bool enable_private_logger() const override;
  bool drop_out_based_data_availability() const override;
  bool enable_attestation_transparency_verifier() const override;

  // Configurable flags.
  int64_t condition_polling_period_millis() const override;
  bool log_tensorflow_error_messages() const override;
  int64_t tf_execution_teardown_grace_period_millis() const override;
  int64_t tf_execution_teardown_extended_period_millis() const override;
  int32_t http_retry_max_attempts() const override;
  int32_t http_retry_delay_ms() const override;
  bool disable_http_request_body_compression() const override;
  int32_t waiting_period_sec_for_cancellation() const override;
  int64_t federated_training_transient_errors_retry_delay_secs() const override;
  int64_t federated_training_permanent_errors_retry_delay_secs() const override;
  bool enable_privacy_id_generation() const override;
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_FCP_FLAGS_H_
