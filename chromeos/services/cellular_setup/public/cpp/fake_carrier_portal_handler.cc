// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/public/cpp/fake_carrier_portal_handler.h"

namespace chromeos {

namespace cellular_setup {

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

}  // namespace cellular_setup

}  // namespace chromeos
