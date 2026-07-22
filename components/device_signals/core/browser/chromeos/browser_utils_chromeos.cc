// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/browser_utils.h"

#include <string>
#include <vector>

#include "components/device_signals/core/common/common_types.h"
#include "net/base/network_interfaces.h"

namespace device_signals {

std::string GetHostName() {
  return net::GetHostName();
}

std::vector<std::string> GetSystemDnsServers() {
  return {};
}

SettingValue GetOSFirewall() {
  return SettingValue::UNKNOWN;
}

}  // namespace device_signals
