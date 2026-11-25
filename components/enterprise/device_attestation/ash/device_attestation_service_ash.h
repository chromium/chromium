// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ASH_DEVICE_ATTESTATION_SERVICE_ASH_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ASH_DEVICE_ATTESTATION_SERVICE_ASH_H_

#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/enterprise/device_attestation/device_attestation_service.h"

namespace attestation {
class SignSimpleChallengeReply;
}  // namespace attestation

namespace enterprise {

class DeviceAttestationServiceAsh : public DeviceAttestationService {
 public:
  DeviceAttestationServiceAsh();
  DeviceAttestationServiceAsh(const DeviceAttestationServiceAsh&) = delete;
  DeviceAttestationServiceAsh& operator=(const DeviceAttestationServiceAsh&) =
      delete;
  ~DeviceAttestationServiceAsh() override;

  // DeviceAttestationService:
  void GetAttestationResponse(
      std::string_view flow_name,
      const enterprise_management::ChromeProfileReportRequest& report,
      std::string_view legacy_request_payload,
      std::string_view timestamp,
      std::string_view nonce,
      DeviceAttestationCallback callback) override;

 private:
  friend class DeviceAttestationServiceAshTest;

  void OnChallengeResponseReady(
      base::TimeTicks start_time,
      DeviceAttestationCallback callback,
      const ::attestation::SignSimpleChallengeReply& reply);

  base::WeakPtrFactory<DeviceAttestationServiceAsh> weak_ptr_factory_{this};
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_ASH_DEVICE_ATTESTATION_SERVICE_ASH_H_
