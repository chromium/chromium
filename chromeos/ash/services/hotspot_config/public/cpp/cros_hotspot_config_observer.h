// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_OBSERVER_H_

#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"

namespace ash::hotspot_config {

// This class allows derived observers to override only the methods that they
// need.
class CrosHotspotConfigObserver : public mojom::CrosHotspotConfigObserver {
 public:
  ~CrosHotspotConfigObserver() override = default;

  void OnHotspotInfoChanged() override;
};

}  // namespace ash::hotspot_config

#endif  // CHROMEOS_ASH_SERVICES_HOTSPOT_CONFIG_PUBLIC_CPP_CROS_HOTSPOT_CONFIG_OBSERVER_H_
