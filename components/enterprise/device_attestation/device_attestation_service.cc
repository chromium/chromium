// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/device_attestation_service.h"

namespace enterprise {

DeviceAttestationService::DeviceAttestationService() = default;
DeviceAttestationService::~DeviceAttestationService() = default;

void DeviceAttestationService::GetAttestationResponse(
    std::string_view flow_name,
    std::string_view request_payload,
    std::string_view timestamp,
    std::string_view nonce,
    DeviceAttestationCallback callback) {
  // no-op
  std::move(callback).Run({"", "Device Attestation is unsupported"});
}

}  // namespace enterprise
