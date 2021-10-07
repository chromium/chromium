// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"

#include <cstddef>
#include <map>
#include <string>

#include "base/check.h"

namespace chromeos {

namespace {

using ChromeOSSystemExtensionInfos =
    std::map<std::string, const ChromeOSSystemExtensionInfo>;

const ChromeOSSystemExtensionInfos& getMap() {
  static const ChromeOSSystemExtensionInfos kExtensionIdToExtensionInfoMap{
      // TODO(b/200920331): replace google.com with OEM-specific origin.
      {/*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
       {/*manufacturer=*/"HP", /*pwa_origin=*/"*://www.google.com/*",
        /*host=*/"www.google.com"}}};

  return kExtensionIdToExtensionInfoMap;
}

}  // namespace

ChromeOSSystemExtensionInfo::ChromeOSSystemExtensionInfo(
    const std::string& manufacturer,
    const std::string& pwa_origin,
    const std::string& host)
    : manufacturer(manufacturer), pwa_origin(pwa_origin), host(host) {}
ChromeOSSystemExtensionInfo::ChromeOSSystemExtensionInfo(
    const ChromeOSSystemExtensionInfo& other) = default;
ChromeOSSystemExtensionInfo::~ChromeOSSystemExtensionInfo() = default;

size_t GetChromeOSSystemExtensionInfosSize() {
  return getMap().size();
}

const ChromeOSSystemExtensionInfo& GetChromeOSExtensionInfoForId(
    const std::string& id) {
  CHECK(IsChromeOSSystemExtension(id));

  return getMap().at(id);
}

bool IsChromeOSSystemExtension(const std::string& id) {
  const auto& extension_info_map = getMap();
  return extension_info_map.find(id) != extension_info_map.end();
}

}  // namespace chromeos
