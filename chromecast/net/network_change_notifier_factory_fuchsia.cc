// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/network_change_notifier_factory_fuchsia.h"

#include "base/command_line.h"
#include "chromecast/base/chromecast_switches.h"
#include "net/base/network_change_notifier_fuchsia.h"

namespace chromecast {

std::unique_ptr<net::NetworkChangeNotifier>
NetworkChangeNotifierFactoryFuchsia::CreateInstanceWithInitialTypes(
    net::NetworkChangeNotifier::ConnectionType /*initial_type*/,
    net::NetworkChangeNotifier::ConnectionSubtype /*initial_subtype*/) {
  auto require_wlan = GetSwitchValueBoolean(switches::kRequireWlan, false);

  // Caller assumes ownership.
  return std::make_unique<net::NetworkChangeNotifierFuchsia>(require_wlan);
}

NetworkChangeNotifierFactoryFuchsia::NetworkChangeNotifierFactoryFuchsia() =
    default;
NetworkChangeNotifierFactoryFuchsia::~NetworkChangeNotifierFactoryFuchsia() =
    default;

}  // namespace chromecast
