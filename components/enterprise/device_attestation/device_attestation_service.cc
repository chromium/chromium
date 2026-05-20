// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/device_attestation_service.h"

#include <utility>

namespace enterprise {

DeviceAttestationService::DeviceAttestationService() = default;
DeviceAttestationService::~DeviceAttestationService() = default;

void DeviceAttestationService::GetAttestationResponse(
    std::string_view flow_name,
    const enterprise_management::ChromeProfileReportRequest& report,
    std::string_view legacy_request_payload,
    std::string_view timestamp,
    std::string_view nonce,
    DeviceAttestationCallback callback) {
  // no-op
  std::move(callback).Run(
      AttestationResult{{"", "Device Attestation is unsupported"}, 0});
}

}  // namespace enterprise
