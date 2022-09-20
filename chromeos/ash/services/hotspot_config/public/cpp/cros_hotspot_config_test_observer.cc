// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_test_observer.h"

namespace ash::hotspot_config {

CrosHotspotConfigTestObserver::CrosHotspotConfigTestObserver() = default;
CrosHotspotConfigTestObserver::~CrosHotspotConfigTestObserver() = default;

mojo::PendingRemote<mojom::CrosHotspotConfigObserver>
CrosHotspotConfigTestObserver::GenerateRemote() {
  return receiver().BindNewPipeAndPassRemote();
}

void CrosHotspotConfigTestObserver::OnHotspotInfoChanged() {
  hotspot_info_changed_count_++;
}

void CrosHotspotConfigTestObserver::OnHotspotStateFailed(
    const std::string& error_code) {
  hotspot_state_failed_count_++;
  last_hotspot_failed_error_ = error_code;
}

}  // namespace ash::hotspot_config
