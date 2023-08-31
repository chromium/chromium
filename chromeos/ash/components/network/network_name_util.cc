// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_name_util.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash::network_name_util {

auto profileFinder = [](const auto& cellular_esim_profile_handler,
                        const auto& network) -> CellularESimProfile* {
  std::vector<CellularESimProfile> profiles =
      cellular_esim_profile_handler->GetESimProfiles();
  for (CellularESimProfile& profile : profiles) {
    if (profile.eid() != network->eid() ||
        profile.iccid() != network->iccid()) {
      continue;
    }
    return &profile;
  }
  return nullptr;
};

absl::optional<std::string> GetESimProfileName(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    const NetworkState* network_state) {
  DCHECK(network_state);

  // CellularESimProfileHandler is not available if the relevant flag is
  // disabled.
  if (!cellular_esim_profile_handler)
    return absl::nullopt;

  // Only Cellular networks correspond to eSIM profiles.
  if (network_state->type() != shill::kTypeCellular)
    return absl::nullopt;

  // eSIM profiles have an associated EID and ICCID.
  if (network_state->eid().empty() || network_state->iccid().empty())
    return absl::nullopt;

  std::vector<CellularESimProfile> profiles =
      cellular_esim_profile_handler->GetESimProfiles();
  for (const auto& profile : profiles) {
    if (profile.eid() != network_state->eid() ||
        profile.iccid() != network_state->iccid()) {
      continue;
    }

    // We've found a profile corresponding to the network. If possible, use the
    // profile's nickname, falling back to the service provider or the name.

    if (!profile.nickname().empty())
      return base::UTF16ToUTF8(profile.nickname());

    if (!profile.service_provider().empty())
      return base::UTF16ToUTF8(profile.service_provider());

    if (!profile.name().empty())
      return base::UTF16ToUTF8(profile.name());
  }

  return absl::nullopt;
}

std::string GetNetworkName(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    const NetworkState* network_state) {
  DCHECK(network_state);
  if (!network_state->eid().empty()) {
    absl::optional<std::string> network_name;
    network_name =
        GetESimProfileName(cellular_esim_profile_handler, network_state);
    if (network_name.has_value())
      return *network_name;
  }
  return network_state->name();
}

bool HasNickName(CellularESimProfileHandler* cellular_esim_profile_handler,
                 const NetworkState* network_state) {
  DCHECK(network_state);
  if (!cellular_esim_profile_handler) {
    return false;
  }
  CellularESimProfile* profile =
      profileFinder(cellular_esim_profile_handler, network_state);
  return profile && !profile->nickname().empty();
}

std::string GetServiceProvider(
    CellularESimProfileHandler* cellular_esim_profile_handler,
    const NetworkState* network_state) {
  DCHECK(network_state);
  if (!cellular_esim_profile_handler) {
    return "";
  }
  std::vector<CellularESimProfile> profiles =
      cellular_esim_profile_handler->GetESimProfiles();
  for (const auto& profile : profiles) {
    if (profile.eid() != network_state->eid() ||
        profile.iccid() != network_state->iccid()) {
      continue;
    }
    return base::UTF16ToUTF8(profile.service_provider());
  }
  return "";
}

}  // namespace ash::network_name_util
