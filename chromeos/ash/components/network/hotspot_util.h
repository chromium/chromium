// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_UTIL_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-forward.h"

namespace base {
class Value;
}  // namespace base

namespace ash {

// Convert shill tethering state string value to mojom::HotspotState enum
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::HotspotState ShillTetheringStateToMojomState(
    const std::string& shill_state);

// Convert shill security mode string value to mojom::WiFiSecurityMode enum
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::WiFiSecurityMode ShillSecurityToMojom(
    const std::string& shill_security);

// Convert shill tethering config dictionary value to mojom::HotspotConfigPtr
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::HotspotConfigPtr ShillTetheringConfigToMojomConfig(
    const base::Value& shill_tethering_config);

// Convert mojom::HotspotConfigPtr to the corresponding shill tethering config
// value
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value MojomConfigToShillConfig(
    const hotspot_config::mojom::HotspotConfigPtr mojom_config);

// Convert enable or disable tethering result string from shill to
// mojom::HotspotControlResult
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::HotspotControlResult SetTetheringEnabledResultToMojom(
    const std::string& shill_enabled_result);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_UTIL_H_
