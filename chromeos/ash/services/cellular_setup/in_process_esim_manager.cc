// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/cellular_setup/in_process_esim_manager.h"

#include "base/no_destructor.h"
#include "chromeos/ash/services/cellular_setup/esim_manager.h"

namespace ash::cellular_setup {

void BindToInProcessESimManager(
    mojo::PendingReceiver<mojom::ESimManager> receiver) {
  static base::NoDestructor<ESimManager> instance;
  instance->BindReceiver(std::move(receiver));
}

}  // namespace ash::cellular_setup
