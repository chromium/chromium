// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/telemetry_extension_ui/system_events_service.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

SystemEventsService::SystemEventsService(
    mojo::PendingReceiver<health::mojom::SystemEventsService> receiver)
    : receiver_(this, std::move(receiver)) {}

SystemEventsService::~SystemEventsService() = default;

void SystemEventsService::AddBluetoothObserver(
    mojo::PendingRemote<health::mojom::BluetoothObserver> observer) {
  bluetooth_observer_.AddObserver(std::move(observer));
}

void SystemEventsService::AddLidObserver(
    mojo::PendingRemote<health::mojom::LidObserver> observer) {
  lid_observer_.AddObserver(std::move(observer));
}

void SystemEventsService::AddPowerObserver(
    mojo::PendingRemote<health::mojom::PowerObserver> observer) {
  power_observer_.AddObserver(std::move(observer));
}

void SystemEventsService::FlushForTesting() {
  bluetooth_observer_.FlushForTesting();
  lid_observer_.FlushForTesting();
  power_observer_.FlushForTesting();
}

}  // namespace chromeos
