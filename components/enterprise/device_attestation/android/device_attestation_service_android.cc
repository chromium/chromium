// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/device_attestation_service_android.h"

#include "base/task/thread_pool.h"
#include "components/enterprise/device_attestation/android/attestation_utils.h"

namespace enterprise {

DeviceAttestationServiceAndroid::DeviceAttestationServiceAndroid() = default;
DeviceAttestationServiceAndroid::~DeviceAttestationServiceAndroid() = default;

void DeviceAttestationServiceAndroid::GetAttestationResponse(
    std::string_view flow_name,
    std::string_view request_payload,
    std::string_view timestamp,
    std::string_view nonce,
    DeviceAttestationCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GenerateAttestationBlob, flow_name, request_payload,
                     timestamp, nonce),
      base::BindOnce(&DeviceAttestationServiceAndroid::OnAttestationResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceAttestationServiceAndroid::OnAttestationResponse(
    DeviceAttestationCallback callback,
    const BlobGenerationResult& blob_generation_result) {
  std::move(callback).Run(blob_generation_result);
}

}  // namespace enterprise
