// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/multidevice_setup/multidevice_setup_base.h"

namespace ash {

namespace multidevice_setup {

MultiDeviceSetupBase::MultiDeviceSetupBase() = default;

MultiDeviceSetupBase::~MultiDeviceSetupBase() = default;

void MultiDeviceSetupBase::BindReceiver(
    mojo::PendingReceiver<mojom::MultiDeviceSetup> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MultiDeviceSetupBase::CloseAllReceivers() {
  receivers_.Clear();
}

}  // namespace multidevice_setup

}  // namespace ash
