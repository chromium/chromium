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

void SystemEventsService::AddLidObserver(
    mojo::PendingRemote<health::mojom::LidObserver> observer) {
  lid_observer_.AddObserver(std::move(observer));
}

void SystemEventsService::FlushForTesting() {
  lid_observer_.FlushForTesting();
}

}  // namespace chromeos
