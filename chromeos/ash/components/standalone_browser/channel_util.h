// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_CHANNEL_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_CHANNEL_UTIL_H_

#include "base/component_export.h"
#include "base/version_info/channel.h"

namespace base {
class Version;
}  // namespace base

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace ash::standalone_browser {
enum class LacrosSelection;

// The default update channel to leverage for Lacros when the channel is
// unknown.
inline constexpr version_info::Channel kLacrosDefaultChannel =
    version_info::Channel::DEV;

// A command-line switch that can also be set from chrome://flags for selecting
// the channel for Lacros updates.
inline constexpr char kLacrosStabilitySwitch[] = "lacros-stability";
inline constexpr char kLacrosStabilityChannelCanary[] = "canary";
inline constexpr char kLacrosStabilityChannelDev[] = "dev";
inline constexpr char kLacrosStabilityChannelBeta[] = "beta";
inline constexpr char kLacrosStabilityChannelStable[] = "stable";

struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
    ComponentInfo {
  // The client-side component name.
  const char* const name;
  // The CRX "extension" ID for component updater.
  // Must match the Omaha console.
  const char* const crx_id;
};

// NOTE: If you change the lacros component names, you must also update
// chrome/browser/component_updater/cros_component_installer_chromeos.cc
inline constexpr ComponentInfo kLacrosDogfoodCanaryInfo = {
    "lacros-dogfood-canary", "hkifppleldbgkdlijbdfkdpedggaopda"};
inline constexpr ComponentInfo kLacrosDogfoodDevInfo = {
    "lacros-dogfood-dev", "ldobopbhiamakmncndpkeelenhdmgfhk"};
inline constexpr ComponentInfo kLacrosDogfoodBetaInfo = {
    "lacros-dogfood-beta", "hnfmbeciphpghlfgpjfbcdifbknombnk"};
inline constexpr ComponentInfo kLacrosDogfoodStableInfo = {
    "lacros-dogfood-stable", "ehpjbaiafkpkmhjocnenjbbhmecnfcjb"};

// Returns the lacros ComponentInfo for a given channel.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
ComponentInfo GetLacrosComponentInfoForChannel(version_info::Channel channel);

// Returns the ComponentInfo associated with the stateful lacros instance.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
ComponentInfo GetLacrosComponentInfo();

// Returns the update channel associated with the given loaded lacros selection.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
version_info::Channel GetLacrosSelectionUpdateChannel(
    standalone_browser::LacrosSelection selection);

// Returns the currently installed version of lacros-chrome managed by the
// component updater. Will return an empty / invalid version if no lacros
// component is found.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER)
base::Version GetInstalledLacrosComponentVersion(
    const component_updater::ComponentUpdateService* component_update_service);

}  // namespace ash::standalone_browser

#endif  // CHROMEOS_ASH_COMPONENTS_STANDALONE_BROWSER_CHANNEL_UTIL_H_
