// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/public/cpp/fake_carrier_portal_handler.h"

namespace ash::cellular_setup {

FakeCarrierPortalHandler::FakeCarrierPortalHandler() = default;

FakeCarrierPortalHandler::~FakeCarrierPortalHandler() = default;

mojo::PendingRemote<mojom::CarrierPortalHandler>
FakeCarrierPortalHandler::GenerateRemote() {
  mojo::PendingRemote<mojom::CarrierPortalHandler> remote;
  receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
  return remote;
}

void FakeCarrierPortalHandler::OnCarrierPortalStatusChange(
    mojom::CarrierPortalStatus carrier_portal_status) {
  status_updates_.push_back(carrier_portal_status);
}

}  // namespace ash::cellular_setup
