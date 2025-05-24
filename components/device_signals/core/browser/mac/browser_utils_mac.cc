// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/browser_utils.h"

#include <CoreFoundation/CoreFoundation.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/mac/mac_util.h"
#include "base/process/launch.h"
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
  if (base::mac::MacOSMajorVersion() < 15) {
    // There is no official Apple documentation on how to obtain the enabled
    // status of the firewall (System Preferences> Security & Privacy> Firewall)
    // prior to MacOS versions 15. Reading globalstate from com.apple.alf is the
    // closest way to get such an API in Chrome without delegating to
    // potentially unstable commands. Values of "globalstate":
    //   0 = de-activated
    //   1 = on for specific services
    //   2 = on for essential services
    // You can get 2 by, e.g., enabling the "Block all incoming connections"
    // firewall functionality.
    Boolean key_exists_with_valid_format = false;
    CFIndex globalstate = CFPreferencesGetAppIntegerValue(
        CFSTR("globalstate"), CFSTR("com.apple.alf"),
        &key_exists_with_valid_format);

    if (!key_exists_with_valid_format) {
      return SettingValue::UNKNOWN;
    }

    switch (globalstate) {
      case 0:
        return SettingValue::DISABLED;
      case 1:
      case 2:
        return SettingValue::ENABLED;
      default:
        return SettingValue::UNKNOWN;
    }
  }

  // Based on this recommendation from Apple:
  // https://developer.apple.com/documentation/macos-release-notes/macos-15-release-notes/#Application-Firewall
  base::FilePath fw_util("/usr/libexec/ApplicationFirewall/socketfilterfw");
  if (!base::PathExists(fw_util)) {
    return SettingValue::UNKNOWN;
  }

  base::CommandLine command(fw_util);
  command.AppendSwitch("getglobalstate");
  std::string output;
  if (!base::GetAppOutput(command, &output)) {
    return SettingValue::UNKNOWN;
  }

  // State 1 is when the Firewall is simply enabled.
  // State 2 is when the Firewall is enabled and all incoming connections are
  // blocked.
  if (output.find("(State = 1)") != std::string::npos ||
      output.find("(State = 2)") != std::string::npos) {
    return SettingValue::ENABLED;
  }
  if (output.find("(State = 0)") != std::string::npos) {
    return SettingValue::DISABLED;
  }

  return SettingValue::UNKNOWN;
}
}  // namespace device_signals
