// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_FAKE_OTA_ACTIVATOR_H_
#define CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_FAKE_OTA_ACTIVATOR_H_

#include "base/functional/callback_forward.h"
#include "chromeos/ash/services/cellular_setup/ota_activator.h"
#include "chromeos/ash/services/cellular_setup/public/cpp/fake_carrier_portal_handler.h"

namespace ash::cellular_setup {

// Test OtaActivator implementation.
class FakeOtaActivator : public OtaActivator {
 public:
  explicit FakeOtaActivator(base::OnceClosure on_finished_callback);

  FakeOtaActivator(const FakeOtaActivator&) = delete;
  FakeOtaActivator& operator=(const FakeOtaActivator&) = delete;

  ~FakeOtaActivator() override;

  using OtaActivator::InvokeOnFinishedCallback;

  const std::vector<mojom::CarrierPortalStatus>& status_updates() const {
    return fake_carrier_portal_handler_.status_updates();
  }

 private:
  // mojom::CarrierPortalHandler:
  void OnCarrierPortalStatusChange(mojom::CarrierPortalStatus status) override;

  FakeCarrierPortalHandler fake_carrier_portal_handler_;
};

}  // namespace ash::cellular_setup

#endif  // CHROMEOS_ASH_SERVICES_CELLULAR_SETUP_FAKE_OTA_ACTIVATOR_H_
