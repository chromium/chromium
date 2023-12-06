// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_NAME_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_NAME_UTIL_H_

#include <optional>
#include <string>

#include "base/component_export.h"

namespace ash {

class CellularESimProfileHandler;
class NetworkState;

namespace network_name_util {

// Returns eSIM profile name for  a given |network_state|.
// Returns null if |cellular_esim_profile_handler| is null, or network is not
// an eSIM network.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::optional<std::string> GetESimProfileName(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    const NetworkState* network_state);

// Returns network name for a given |network_state|. If network
// is eSIM it calls GetESimProfileName and uses |cellular_esim_profile_handler|
// to get the eSIM profile name. If |cellular_esim_profile_handler| is null,
// this function returns |network_state->name|.
COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GetNetworkName(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    const NetworkState* network_state);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
bool HasNickName(CellularESimProfileHandler* cellular_esim_profile_handler,
                 const NetworkState* network_state);

COMPONENT_EXPORT(CHROMEOS_NETWORK)
std::string GetServiceProvider(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    const NetworkState* network_state);
}  // namespace network_name_util
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_NAME_UTIL_H_
