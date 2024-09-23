// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/hotspot/hotspot_state_observer.h"

#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash {

HotspotStateObserver::HotspotStateObserver()
    : ObservationStateObserver(NetworkHandler::Get()->hotspot_state_handler()) {
}

HotspotStateObserver::~HotspotStateObserver() = default;

void HotspotStateObserver::OnHotspotStatusChanged() {
  OnStateObserverStateChanged(/*state=*/GetHotspotState());
}

hotspot_config::mojom::HotspotState
HotspotStateObserver::GetStateObserverInitialState() const {
  return GetHotspotState();
}

hotspot_config::mojom::HotspotState HotspotStateObserver::GetHotspotState()
    const {
  return NetworkHandler::Get()->hotspot_state_handler()->GetHotspotState();
}

}  // namespace ash
