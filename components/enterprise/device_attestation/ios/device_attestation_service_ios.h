// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_DEVICE_ATTESTATION_SERVICE_IOS_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_DEVICE_ATTESTATION_SERVICE_IOS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
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
  // Note: `report`, `timestamp`, and `nonce` are used to construct content
  // bindings for the snapshot.
  // Other arguments (`flow_name`, `legacy_request_payload`) are required by the
  // base interface but are not used on iOS.
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
  // Holds the Objective-C objects owned by this class. They cannot be members,
  // because this header is also included from C++ files.
  struct ObjCStorage;

  // `start_time` is the time at which the snapshot was requested, and is used
  // to record the generation latency. `background_time_at_start` is the value
  // of `GetTimeInBackground()` at that time, and is used to leave the time the
  // app spent in the background out of the foreground latency.
  void OnAttestationResponse(
      uint64_t request_id,
      base::TimeTicks start_time,
      base::TimeDelta background_time_at_start,
      DeviceAttestationCallback callback,
      base::expected<std::string, AttestationServiceIOS::AttestationError>
          result);

  // Called when the app has entered the background, and when it is about to
  // enter the foreground again.
  void OnAppDidEnterBackground();
  void OnAppWillEnterForeground();

  // Returns the total time the app has spent in the background since this
  // object was created, up to `now`.
  base::TimeDelta GetTimeInBackground(base::TimeTicks now) const;

  std::unique_ptr<AttestationServiceIOS> attestation_service_;
  uint64_t next_request_id_ = 0;
  base::flat_map<uint64_t, base::CallbackListSubscription>
      attestation_subscriptions_;

  // When the app entered the background, if it is in the background now.
  std::optional<base::TimeTicks> background_start_time_;
  // Total duration of the background periods that have ended.
  base::TimeDelta completed_background_time_;
  std::unique_ptr<ObjCStorage> objc_storage_;

  base::WeakPtrFactory<DeviceAttestationServiceIOS> weak_ptr_factory_{this};
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_IOS_DEVICE_ATTESTATION_SERVICE_IOS_H_
