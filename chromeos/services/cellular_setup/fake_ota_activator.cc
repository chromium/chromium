// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/fake_ota_activator.h"

#include <utility>

namespace chromeos {

namespace cellular_setup {

FakeOtaActivator::FakeOtaActivator(base::OnceClosure on_finished_callback)
    : OtaActivator(std::move(on_finished_callback)) {}

FakeOtaActivator::~FakeOtaActivator() = default;

void FakeOtaActivator::OnCarrierPortalStatusChange(
    mojom::CarrierPortalStatus status) {
  fake_carrier_portal_handler_.OnCarrierPortalStatusChange(status);
}

}  // namespace cellular_setup

}  // namespace chromeos
