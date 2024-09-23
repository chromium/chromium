// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/standalone_browser/channel_util.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/logging.h"
#include "base/version.h"
#include "chromeos/ash/components/channel/channel_info.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"
#include "components/component_updater/component_updater_service.h"

namespace ash::standalone_browser {

namespace {

// Resolves the Lacros stateful channel in the following order:
//   1. From the kLacrosStabilitySwitch command line flag if present.
//   2. From the current ash channel.
version_info::Channel GetStatefulLacrosChannel() {
  static constexpr auto kStabilitySwitchToChannelMap =
      base::MakeFixedFlatMap<std::string_view, version_info::Channel>({
          {kLacrosStabilityChannelCanary, version_info::Channel::CANARY},
          {kLacrosStabilityChannelDev, version_info::Channel::DEV},
          {kLacrosStabilityChannelBeta, version_info::Channel::BETA},
          {kLacrosStabilityChannelStable, version_info::Channel::STABLE},
      });
  std::string stability_switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kLacrosStabilitySwitch);
  if (!stability_switch_value.empty()) {
    if (auto it = kStabilitySwitchToChannelMap.find(stability_switch_value);
        it != kStabilitySwitchToChannelMap.end()) {
      return it->second;
    }
  }
  return GetChannel();
}

}  // namespace

ComponentInfo GetLacrosComponentInfoForChannel(version_info::Channel channel) {
  // We default to the Dev component for UNKNOWN channels.
  static constexpr auto kChannelToComponentInfoMap =
      base::MakeFixedFlatMap<version_info::Channel, const ComponentInfo*>({
          {version_info::Channel::UNKNOWN, &kLacrosDogfoodDevInfo},
          {version_info::Channel::CANARY, &kLacrosDogfoodCanaryInfo},
          {version_info::Channel::DEV, &kLacrosDogfoodDevInfo},
          {version_info::Channel::BETA, &kLacrosDogfoodBetaInfo},
          {version_info::Channel::STABLE, &kLacrosDogfoodStableInfo},
      });
  return *kChannelToComponentInfoMap.at(channel);
}

ComponentInfo GetLacrosComponentInfo() {
  return GetLacrosComponentInfoForChannel(GetStatefulLacrosChannel());
}

version_info::Channel GetLacrosSelectionUpdateChannel(
    standalone_browser::LacrosSelection selection) {
  switch (selection) {
    case standalone_browser::LacrosSelection::kRootfs:
      // For 'rootfs' Lacros use the same channel as ash/OS. Obtained from
      // the LSB's release track property.
      return GetChannel();
    case standalone_browser::LacrosSelection::kStateful:
      // For 'stateful' Lacros directly check the channel of stateful-lacros
      // that the user is on.
      return GetStatefulLacrosChannel();
    case standalone_browser::LacrosSelection::kDeployedLocally:
      // For locally deployed Lacros there is no channel so return unknown.
      return version_info::Channel::UNKNOWN;
  }
}

base::Version GetInstalledLacrosComponentVersion(
    const component_updater::ComponentUpdateService* component_update_service) {
  DCHECK(component_update_service);

  const std::vector<component_updater::ComponentInfo>& components =
      component_update_service->GetComponents();
  const std::string& lacros_component_id = GetLacrosComponentInfo().crx_id;

  LOG(WARNING) << "Looking for lacros-chrome component with id: "
               << lacros_component_id;
  auto it =
      std::find_if(components.begin(), components.end(),
                   [&](const component_updater::ComponentInfo& component_info) {
                     return component_info.id == lacros_component_id;
                   });

  return it == components.end() ? base::Version() : it->version;
}

}  // namespace ash::standalone_browser
