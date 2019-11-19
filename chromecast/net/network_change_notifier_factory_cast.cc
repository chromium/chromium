// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/net/network_change_notifier_factory_cast.h"

#include "chromecast/net/net_util_cast.h"
#include "net/base/network_change_notifier_linux.h"

namespace chromecast {

std::unique_ptr<net::NetworkChangeNotifier>
NetworkChangeNotifierFactoryCast::CreateInstance() {
  // Caller assumes ownership.
  return std::make_unique<net::NetworkChangeNotifierLinux>(
      GetIgnoredInterfaces());
}

NetworkChangeNotifierFactoryCast::~NetworkChangeNotifierFactoryCast() {
}

}  // namespace chromecast
