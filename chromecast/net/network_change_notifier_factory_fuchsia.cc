// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/network_change_notifier_factory_fuchsia.h"

#include <fuchsia/hardware/ethernet/cpp/fidl.h>

#include "base/command_line.h"
#include "chromecast/base/chromecast_switches.h"
#include "net/base/network_change_notifier_fuchsia.h"

namespace chromecast {

std::unique_ptr<net::NetworkChangeNotifier>
NetworkChangeNotifierFactoryFuchsia::CreateInstance() {
  uint32_t required_features =
      GetSwitchValueBoolean(switches::kRequireWlan, false)
          ? fuchsia::hardware::ethernet::INFO_FEATURE_WLAN
          : 0;

  // Caller assumes ownership.
  return std::make_unique<net::NetworkChangeNotifierFuchsia>(required_features);
}

NetworkChangeNotifierFactoryFuchsia::NetworkChangeNotifierFactoryFuchsia() =
    default;
NetworkChangeNotifierFactoryFuchsia::~NetworkChangeNotifierFactoryFuchsia() =
    default;

}  // namespace chromecast
