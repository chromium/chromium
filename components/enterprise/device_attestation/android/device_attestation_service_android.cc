// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/device_attestation_service_android.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/device_attestation/android/android_attestation_client.h"
#include "components/enterprise/device_attestation/android/attestation_utils.h"
#include "components/enterprise/device_attestation/device_attestation_metrics.h"
#include "components/enterprise/device_attestation/device_attestation_utils.h"

namespace enterprise {

DeviceAttestationServiceAndroid::DeviceAttestationServiceAndroid(
    std::unique_ptr<AndroidAttestationClient> client)
    : client_(std::move(client)) {
  CHECK(client_);
}

DeviceAttestationServiceAndroid::~DeviceAttestationServiceAndroid() = default;

void DeviceAttestationServiceAndroid::GetAttestationResponse(
    std::string_view flow_name,
    const enterprise_management::ChromeProfileReportRequest& report,
    std::string_view legacy_request_payload,
    std::string_view timestamp,
    std::string_view nonce,
    DeviceAttestationCallback callback) {
  // Captured before any work is done so that the main-thread content binding
  // and hashing costs below are part of the measured latency.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  LogAttestationBlobRequested();

  std::string request_payload;
  if (enterprise_signals::features::IsContentBindingVersioningEnabled()) {
    request_payload = GenerateV1ContentBindingString(report);
  } else {
    request_payload = std::string(legacy_request_payload);
  }

  AttestationHashes hashes =
      CreateAttestationHashes(request_payload, timestamp, nonce);

  client_->GenerateAttestationBlob(
      flow_name, hashes.request_hash, hashes.timestamp_hash, hashes.nonce_hash,
      base::BindOnce(&DeviceAttestationServiceAndroid::OnAttestationResponse,
                     weak_ptr_factory_.GetWeakPtr(), start_time,
                     std::move(callback)));
}

void DeviceAttestationServiceAndroid::OnAttestationResponse(
    base::TimeTicks start_time,
    DeviceAttestationCallback callback,
    BlobGenerationResult blob_generation_result) {
  LogAttestationBlobGenerated(start_time, blob_generation_result);

  std::move(callback).Run(AttestationResult{
      .blob_generation_result = std::move(blob_generation_result),
      .content_binding_version =
          enterprise_signals::features::IsContentBindingVersioningEnabled()
              ? 1
              : 0});
}

}  // namespace enterprise
