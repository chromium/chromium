// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "components/enterprise/device_attestation/common/device_attestation_types.h"

namespace enterprise {

// Class to provide attestation service for devices, implementation varies
// depending on the platform.
class DeviceAttestationService {
 public:
  using DeviceAttestationCallback =
      base::OnceCallback<void(const BlobGenerationResult&)>;

  DeviceAttestationService();
  DeviceAttestationService(const DeviceAttestationService&) = delete;
  DeviceAttestationService& operator=(const DeviceAttestationService&) = delete;
  virtual ~DeviceAttestationService();

  virtual void GetAttestationResponse(std::string_view flow_name,
                                      std::string_view request_payload,
                                      std::string_view timestamp,
                                      std::string_view nonce,
                                      DeviceAttestationCallback callback);
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_H_
