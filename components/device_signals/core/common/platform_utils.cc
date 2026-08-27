// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/platform_utils.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"

namespace device_signals {

bool IsSupportedLocalPath(const base::FilePath& file_path) {
  if (file_path.empty() || file_path.IsNetwork()) {
    return false;
  }

  const auto& path_str = file_path.value();
  if (path_str.length() >= 2 &&
      (base::FilePath::IsSeparator(path_str[0]) ||
       path_str[0] == FILE_PATH_LITERAL('\\')) &&
      (base::FilePath::IsSeparator(path_str[1]) ||
       path_str[1] == FILE_PATH_LITERAL('\\'))) {
    return false;
  }

  if (base::StartsWith(path_str, FILE_PATH_LITERAL("\\??\\"),
                       base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(path_str, FILE_PATH_LITERAL("/??/"),
                       base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(path_str, FILE_PATH_LITERAL("\\Device\\"),
                       base::CompareCase::INSENSITIVE_ASCII) ||
      base::StartsWith(path_str, FILE_PATH_LITERAL("/Device/"),
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return false;
  }

  return true;
}

namespace {

void NormalizeMacAddresses(std::vector<std::string>& mac_addresses) {
  std::erase_if(mac_addresses, [](const std::string& s) {
    return base::TrimWhitespaceASCII(s, base::TrimPositions::TRIM_ALL).empty();
  });

  std::sort(mac_addresses.begin(), mac_addresses.end());
  mac_addresses.erase(std::unique(mac_addresses.begin(), mac_addresses.end()),
                      mac_addresses.end());
}

std::optional<std::vector<std::string>>& GetMacAddressesForTestingStorage() {
  static std::optional<std::vector<std::string>> storage;
  return storage;
}

}  // namespace

std::string GetOsName() {
  return policy::GetOSPlatform();
}

std::string GetOsVersion() {
  return policy::GetOSVersion();
}

std::vector<std::string> GetMacAddresses() {
  std::vector<std::string> mac_addresses;
  const auto& test_addresses = GetMacAddressesForTestingStorage();
  if (test_addresses.has_value()) {
    mac_addresses = test_addresses.value();
  } else {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    mac_addresses = internal::GetMacAddressesImpl();
#endif
  }
  NormalizeMacAddresses(mac_addresses);
  return mac_addresses;
}

namespace internal {

void SetMacAddressesForTesting(const std::vector<std::string>& mac_addresses) {
  GetMacAddressesForTestingStorage().emplace(mac_addresses);
}

void ClearMacAddressesForTesting() {
  GetMacAddressesForTestingStorage().reset();
}

}  // namespace internal

}  // namespace device_signals
