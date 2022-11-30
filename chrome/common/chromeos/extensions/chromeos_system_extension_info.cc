// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"

#include <cstddef>
#include <map>
#include <string>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"

namespace chromeos {

namespace switches {

// Overrides |manufacturer| field of the ChromeOSSystemExtensionInfo structure.
// Used for development/testing.
const char kTelemetryExtensionManufacturerOverrideForTesting[] =
    "telemetry-extension-manufacturer-override-for-testing";

// Overrides |pwa_origin| field of the ChromeOSSystemExtensionInfo structure.
// Used for development/testing.
const char kTelemetryExtensionPwaOriginOverrideForTesting[] =
    "telemetry-extension-pwa-origin-override-for-testing";

}  // namespace switches

namespace {

using ChromeOSSystemExtensionInfos =
    std::map<std::string, const ChromeOSSystemExtensionInfo>;

const ChromeOSSystemExtensionInfos& getMap() {
  static const ChromeOSSystemExtensionInfos kExtensionIdToExtensionInfoMap{
      {/*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
       {/*manufacturers=*/{"HP", "ASUS"},
        /*pwa_origin=*/"*://googlechromelabs.github.io/*"}},
      {/*extension_id=*/"alnedpmllcfpgldkagbfbjkloonjlfjb",
       {/*manufacturers=*/{"HP"},
        /*pwa_origin=*/"https://hpcs-appschr.hpcloud.hp.com/*"}},
      {/*extension_id=*/"hdnhcpcfohaeangjpkcjkgmgmjanbmeo",
       {/*manufacturers=*/{"ASUS"},
        /*pwa_origin=*/"https://dlcdnccls.asus.com/*"}}};

  return kExtensionIdToExtensionInfoMap;
}

}  // namespace

ChromeOSSystemExtensionInfo::ChromeOSSystemExtensionInfo(
    base::flat_set<std::string> manufacturers,
    const std::string& pwa_origin)
    : manufacturers(std::move(manufacturers)), pwa_origin(pwa_origin) {}
ChromeOSSystemExtensionInfo::ChromeOSSystemExtensionInfo(
    const ChromeOSSystemExtensionInfo& other) = default;
ChromeOSSystemExtensionInfo::~ChromeOSSystemExtensionInfo() = default;

size_t GetChromeOSSystemExtensionInfosSize() {
  return getMap().size();
}

ChromeOSSystemExtensionInfo GetChromeOSExtensionInfoForId(
    const std::string& id) {
  CHECK(IsChromeOSSystemExtension(id));

  auto info = getMap().at(id);

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kTelemetryExtensionPwaOriginOverrideForTesting)) {
    info.pwa_origin = command_line->GetSwitchValueASCII(
        switches::kTelemetryExtensionPwaOriginOverrideForTesting);
  }

  if (command_line->HasSwitch(
          switches::kTelemetryExtensionManufacturerOverrideForTesting)) {
    info.manufacturers = {command_line->GetSwitchValueASCII(
        switches::kTelemetryExtensionManufacturerOverrideForTesting)};
  }

  return info;
}

bool IsChromeOSSystemExtension(const std::string& id) {
  const auto& extension_info_map = getMap();
  return extension_info_map.find(id) != extension_info_map.end();
}

}  // namespace chromeos
