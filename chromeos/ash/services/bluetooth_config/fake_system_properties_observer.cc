// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/bluetooth_config/fake_system_properties_observer.h"

#include <utility>

#include "base/run_loop.h"

namespace ash::bluetooth_config {

FakeSystemPropertiesObserver::FakeSystemPropertiesObserver() = default;

FakeSystemPropertiesObserver::~FakeSystemPropertiesObserver() = default;

mojo::PendingRemote<mojom::SystemPropertiesObserver>
FakeSystemPropertiesObserver::GeneratePendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void FakeSystemPropertiesObserver::DisconnectMojoPipe() {
  receiver_.reset();

  // Allow the disconnection to propagate.
  base::RunLoop().RunUntilIdle();
}

void FakeSystemPropertiesObserver::OnPropertiesUpdated(
    mojom::BluetoothSystemPropertiesPtr properties) {
  received_properties_list_.push_back(std::move(properties));
}

}  // namespace ash::bluetooth_config
