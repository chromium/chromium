// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_
#define CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_

#include <cstddef>
#include <string>

namespace chromeos {

// TODO(b/202273191) Remove |host| from this struct.
struct ChromeOSSystemExtensionInfo {
  ChromeOSSystemExtensionInfo(const std::string& manufacturer,
                              const std::string& pwa_origin,
                              const std::string& host);
  ChromeOSSystemExtensionInfo(const ChromeOSSystemExtensionInfo& other);
  ~ChromeOSSystemExtensionInfo();

  const std::string manufacturer;
  const std::string pwa_origin;
  const std::string host;
};

size_t GetChromeOSSystemExtensionInfosSize();
const ChromeOSSystemExtensionInfo& GetChromeOSExtensionInfoForId(
    const std::string& id);
bool IsChromeOSSystemExtension(const std::string& id);

}  // namespace chromeos

#endif  // CHROME_COMMON_CHROMEOS_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSION_INFO_H_
