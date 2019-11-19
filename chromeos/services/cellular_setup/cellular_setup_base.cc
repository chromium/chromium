// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/cellular_setup_base.h"

namespace chromeos {

namespace cellular_setup {

CellularSetupBase::CellularSetupBase() = default;

CellularSetupBase::~CellularSetupBase() = default;

void CellularSetupBase::BindReceiver(
    mojo::PendingReceiver<mojom::CellularSetup> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace cellular_setup

}  // namespace chromeos
