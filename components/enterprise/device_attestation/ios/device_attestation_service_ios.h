// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_DEVICE_ATTESTATION_SERVICE_IOS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_DEVICE_ATTESTATION_SERVICE_IOS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/enterprise/device_attestation/device_attestation_service.h"
#include "components/enterprise/device_attestation/ios/attestation_service_ios.h"

namespace enterprise {

// iOS implementation of `DeviceAttestationService`.
class DeviceAttestationServiceIOS : public DeviceAttestationService {
 public:
  explicit DeviceAttestationServiceIOS(
      std::unique_ptr<AttestationServiceIOS> attestation_service);
  DeviceAttestationServiceIOS(const DeviceAttestationServiceIOS&) = delete;
  DeviceAttestationServiceIOS& operator=(const DeviceAttestationServiceIOS&) =
      delete;
  ~DeviceAttestationServiceIOS() override;

  // `DeviceAttestationService`:
  // Note: `report` is used to construct content bindings for the snapshot.
  // Other arguments (`flow_name`, `legacy_request_payload`, `timestamp`,
  // `nonce`) are required by the base interface but are not used on iOS.
  // TODO(crbug.com/550216758): Refactor GetAttestationResponse to use a
  // delegate or parameter object to keep the signature uniform without passing
  // unused platform-specific arguments.
  void GetAttestationResponse(
      std::string_view flow_name,
      const enterprise_management::ChromeProfileReportRequest& report,
      std::string_view legacy_request_payload,
      std::string_view timestamp,
      std::string_view nonce,
      DeviceAttestationCallback callback) override;

  size_t GetNumberOfActiveSubscriptionsForTesting() const {
    return attestation_subscriptions_.size();
  }

 private:
  void OnAttestationResponse(
      uint64_t request_id,
      DeviceAttestationCallback callback,
      base::expected<std::string, AttestationServiceIOS::AttestationError>
          result);

  std::unique_ptr<AttestationServiceIOS> attestation_service_;
  uint64_t next_request_id_ = 0;
  base::flat_map<uint64_t, base::CallbackListSubscription>
      attestation_subscriptions_;
  base::WeakPtrFactory<DeviceAttestationServiceIOS> weak_ptr_factory_{this};
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_DEVICE_ATTESTATION_SERVICE_IOS_H_