// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/bluetooth_config/fake_system_properties_observer.h"

#include <utility>

#include "base/run_loop.h"

namespace chromeos {
namespace bluetooth_config {

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

}  // namespace bluetooth_config
}  // namespace chromeos
