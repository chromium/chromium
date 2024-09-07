// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ash/interactive/hotspot/hotspot_config_observer.h"

#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {

HotspotConfigObserver::HotspotConfigObserver() {
  observer_.Observe(ShillManagerClient::Get());
}

HotspotConfigObserver::~HotspotConfigObserver() = default;

void HotspotConfigObserver::OnPropertyChanged(const std::string& key,
                                              const base::Value& value) {
  if (key != shill::kTetheringConfigProperty) {
    return;
  }

  ShillManagerClient::Get()->GetTestInterface()->RestartTethering();
}

}  // namespace ash
