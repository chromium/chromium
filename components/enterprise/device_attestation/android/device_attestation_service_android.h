// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_DEVICE_ATTESTATION_SERVICE_ANDROID_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_DEVICE_ATTESTATION_SERVICE_ANDROID_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/enterprise/device_attestation/device_attestation_service.h"

namespace enterprise {

// Class to provide attestation service for devices, implementation varies
// depending on the platform.
class DeviceAttestationServiceAndroid : public DeviceAttestationService {
 public:
  DeviceAttestationServiceAndroid();
  DeviceAttestationServiceAndroid(const DeviceAttestationServiceAndroid&) =
      delete;
  DeviceAttestationServiceAndroid& operator=(
      const DeviceAttestationServiceAndroid&) = delete;
  ~DeviceAttestationServiceAndroid() override;

  void GetAttestationResponse(std::string_view flow_name,
                              std::string_view request_payload,
                              std::string_view timestamp,
                              std::string_view nonce,
                              DeviceAttestationCallback callback) override;

 private:
  void OnAttestationResponse(
      DeviceAttestationCallback callback,
      const BlobGenerationResult& blob_generation_result);

  base::WeakPtrFactory<DeviceAttestationServiceAndroid> weak_ptr_factory_{this};
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_DEVICE_ATTESTATION_SERVICE_ANDROID_H_
