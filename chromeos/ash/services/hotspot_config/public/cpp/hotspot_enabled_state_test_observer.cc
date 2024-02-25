// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/public/cpp/hotspot_enabled_state_test_observer.h"

namespace ash::hotspot_config {

HotspotEnabledStateTestObserver::HotspotEnabledStateTestObserver() = default;
HotspotEnabledStateTestObserver::~HotspotEnabledStateTestObserver() = default;

mojo::PendingRemote<mojom::HotspotEnabledStateObserver>
HotspotEnabledStateTestObserver::GenerateRemote() {
  return receiver().BindNewPipeAndPassRemote();
}

void HotspotEnabledStateTestObserver::OnHotspotTurnedOn() {
  hotspot_turned_on_count_++;
}

void HotspotEnabledStateTestObserver::OnHotspotTurnedOff(
    mojom::DisableReason reason) {
  hotspot_turned_off_count_++;
  last_disable_reason_ = reason;
}

}  // namespace ash::hotspot_config