// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_UTIL_H_

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom-forward.h"

namespace ash {

// Convert shill tethering state string value to mojom::HotspotState enum
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::HotspotState ShillTetheringStateToMojomState(
    const std::string& shill_state);

// Converts shill tethering idle reason string value to mojom::DisableReason
// enum
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::DisableReason ShillTetheringIdleReasonToMojomState(
    const std::string& idle_reason);

// Convert shill security mode string value to mojom::WiFiSecurityMode enum
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::WiFiSecurityMode ShillSecurityToMojom(
    const std::string& shill_security);

// Convert shill tethering config dictionary value to mojom::HotspotConfigPtr
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::HotspotConfigPtr ShillTetheringConfigToMojomConfig(
    const base::Value::Dict& shill_tethering_config);

// Convert mojom::HotspotConfigPtr to the corresponding shill tethering config
// value
COMPONENT_EXPORT(CHROMEOS_NETWORK)
base::Value::Dict MojomConfigToShillConfig(
    const hotspot_config::mojom::HotspotConfigPtr mojom_config);

// Convert enable or disable tethering result string from shill to
// mojom::HotspotControlResult
COMPONENT_EXPORT(CHROMEOS_NETWORK)
hotspot_config::mojom::HotspotControlResult SetTetheringEnabledResultToMojom(
    const std::string& shill_enabled_result);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_UTIL_H_
