// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_ASH_INTERACTIVE_HOTSPOT_HOTSPOT_CONFIG_OBSERVER_H_
#define CHROME_TEST_BASE_ASH_INTERACTIVE_HOTSPOT_HOTSPOT_CONFIG_OBSERVER_H_

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/ash/components/network/hotspot_state_handler.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "ui/base/interaction/state_observer.h"

namespace ash {

// This is a helper class that can be used in tests that use Kombucha to
// observe changes to Hotspot config.
class HotspotConfigObserver : public ShillPropertyChangedObserver {
 public:
  HotspotConfigObserver();
  ~HotspotConfigObserver() override;

 private:
  // ShillPropertyChangedObserver:
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;
  base::ScopedObservation<ShillManagerClient, ShillPropertyChangedObserver>
      observer_{this};
};

}  // namespace ash

#endif  // CHROME_TEST_BASE_ASH_INTERACTIVE_HOTSPOT_HOTSPOT_CONFIG_OBSERVER_H_
