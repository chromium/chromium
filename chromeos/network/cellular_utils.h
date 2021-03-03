// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_UTILS_H_
#define CHROMEOS_NETWORK_CELLULAR_UTILS_H_

#include <vector>

#include "base/component_export.h"
#include "chromeos/network/device_state.h"

namespace chromeos {

class CellularESimProfile;

// Generates a list of CellularESimProfile objects for all Hermes esim profile
// objects available through its dbus clients.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::vector<CellularESimProfile> GenerateProfilesFromHermes();

// Generates a list of CellularSIMSlotInfo objects with missing EIDs
// populated by EIDs known by Hermes.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
const DeviceState::CellularSIMSlotInfos GetSimSlotInfosWithUpdatedEid(
    const DeviceState* device);
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_UTILS_H_