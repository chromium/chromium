// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"

namespace enterprise_management {
class ChromeProfileReportRequest;
}

namespace enterprise {

// Class to provide attestation service for devices, implementation varies
// depending on the platform.
class DeviceAttestationService {
 public:
  using DeviceAttestationCallback =
      base::OnceCallback<void(const AttestationResult&)>;

  DeviceAttestationService();
  DeviceAttestationService(const DeviceAttestationService&) = delete;
  DeviceAttestationService& operator=(const DeviceAttestationService&) = delete;
  virtual ~DeviceAttestationService();

  // Asynchronously generates the attestation blob that will bind the profile
  // `report` to the device, using the `timestamp` and `nonce` to strengthen the
  // bound. `flow_name` is the name registered with the attestation library by
  // the calling flow. Note: when ContentBindingVersioningEnabled is true,
  // `report` is used, otherwise `legacy_request_payload` is used to represent
  // the report.
  // Invokes `callback` upon completion with the generated AttestationResult.
  virtual void GetAttestationResponse(
      std::string_view flow_name,
      const enterprise_management::ChromeProfileReportRequest& report,
      std::string_view legacy_request_payload,
      std::string_view timestamp,
      std::string_view nonce,
      DeviceAttestationCallback callback);
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_H_
