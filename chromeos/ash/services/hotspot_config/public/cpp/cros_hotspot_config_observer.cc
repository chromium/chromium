// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/hotspot_config/public/cpp/cros_hotspot_config_observer.h"

namespace ash::hotspot_config {

void CrosHotspotConfigObserver::OnHotspotInfoChanged() {}

void CrosHotspotConfigObserver::OnHotspotStateFailed(
    const std::string& error_code) {}

}  // namespace ash::hotspot_config
