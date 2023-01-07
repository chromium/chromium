// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_connection_preserver.h"

namespace ash {

namespace tether {

FakeConnectionPreserver::FakeConnectionPreserver() = default;

FakeConnectionPreserver::~FakeConnectionPreserver() = default;

void FakeConnectionPreserver::HandleSuccessfulTetherAvailabilityResponse(
    const std::string& device_id) {
  last_requested_preserved_connection_device_id_ = device_id;
}

}  // namespace tether

}  // namespace ash
