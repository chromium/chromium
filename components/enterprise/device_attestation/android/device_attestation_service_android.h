// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_DEVICE_ATTESTATION_SERVICE_ANDROID_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_DEVICE_ATTESTATION_SERVICE_ANDROID_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "components/enterprise/device_attestation/device_attestation_service.h"

namespace enterprise {

class AndroidAttestationClient;

// Class to provide attestation service for Android devices.
class DeviceAttestationServiceAndroid : public DeviceAttestationService {
 public:
  explicit DeviceAttestationServiceAndroid(
      std::unique_ptr<AndroidAttestationClient> client);
  DeviceAttestationServiceAndroid(const DeviceAttestationServiceAndroid&) =
      delete;
  DeviceAttestationServiceAndroid& operator=(
      const DeviceAttestationServiceAndroid&) = delete;
  ~DeviceAttestationServiceAndroid() override;

  // DeviceAttestationService:
  void GetAttestationResponse(
      std::string_view flow_name,
      const enterprise_management::ChromeProfileReportRequest& report,
      std::string_view legacy_request_payload,
      std::string_view timestamp,
      std::string_view nonce,
      DeviceAttestationCallback callback) override;

 private:
  void OnAttestationResponse(DeviceAttestationCallback callback,
                             BlobGenerationResult blob_generation_result);

  std::string GenerateV1ContentBindingString(
      const enterprise_management::ChromeProfileReportRequest& report);

  std::unique_ptr<AndroidAttestationClient> client_;
  base::WeakPtrFactory<DeviceAttestationServiceAndroid> weak_ptr_factory_{this};
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ANDROID_DEVICE_ATTESTATION_SERVICE_ANDROID_H_
