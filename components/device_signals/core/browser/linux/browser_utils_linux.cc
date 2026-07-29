// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/browser_utils.h"

#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#endif
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/platform_utils.h"
#include "net/base/network_interfaces.h"
#include "net/dns/public/resolv_reader.h"
#include "net/dns/public/scoped_res_state.h"

namespace device_signals {

std::string GetHostName() {
  return net::GetHostName();
}

std::vector<std::string> GetSystemDnsServers() {
  std::vector<std::string> dns_addresses;
  std::unique_ptr<net::ScopedResState> res = net::ResolvReader().GetResState();
  if (res) {
    std::optional<std::vector<net::IPEndPoint>> nameservers =
        net::GetNameservers(res->state());
    if (nameservers) {
      // If any name server is 0.0.0.0, assume the configuration is invalid.
      for (const net::IPEndPoint& nameserver : nameservers.value()) {
        if (nameserver.address().IsZero()) {
          return std::vector<std::string>();
        } else {
          dns_addresses.push_back(nameserver.ToString());
        }
      }
    }
  }
  return dns_addresses;
}

SettingValue GetOSFirewall() {
#if BUILDFLAG(IS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSystemDevMode)) {
    return SettingValue::UNKNOWN;
  }
  return SettingValue::ENABLED;
#else
  base::FilePath path(*GetUfwConfigPath());
  std::string file_content;
  base::StringPairs values;

  if (!base::PathExists(path) || !base::PathIsReadable(path) ||
      !base::ReadFileToString(path, &file_content)) {
    return SettingValue::UNKNOWN;
  }
  base::SplitStringIntoKeyValuePairs(file_content, '=', '\n', &values);
  auto is_ufw_enabled = std::ranges::find(
      values, "ENABLED", &std::pair<std::string, std::string>::first);
  if (is_ufw_enabled == values.end()) {
    return SettingValue::UNKNOWN;
  }

  if (is_ufw_enabled->second == "yes") {
    return SettingValue::ENABLED;
  } else if (is_ufw_enabled->second == "no") {
    return SettingValue::DISABLED;
  } else {
    return SettingValue::UNKNOWN;
  }
#endif
}

#if BUILDFLAG(IS_LINUX)
const char** GetUfwConfigPath() {
  static const char* path = "/etc/ufw/ufw.conf";
  return &path;
}
#endif

}  // namespace device_signals
