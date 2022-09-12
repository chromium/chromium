// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/fake_ota_activator.h"

#include <utility>

namespace ash::cellular_setup {

FakeOtaActivator::FakeOtaActivator(base::OnceClosure on_finished_callback)
    : OtaActivator(std::move(on_finished_callback)) {}

FakeOtaActivator::~FakeOtaActivator() = default;

void FakeOtaActivator::OnCarrierPortalStatusChange(
    mojom::CarrierPortalStatus status) {
  fake_carrier_portal_handler_.OnCarrierPortalStatusChange(status);
}

}  // namespace ash::cellular_setup
