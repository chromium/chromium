// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_OBSERVER_H_
#define CHROMEOS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_OBSERVER_H_

#include <stdint.h>
#include <vector>

namespace chromeos {

// This is a base class for observers which handle signals sent by the
// ThirdPartyVpnAdaptor in Shill.
class ShillThirdPartyVpnObserver {
 public:
  virtual void OnPacketReceived(const std::vector<char>& data) = 0;
  virtual void OnPlatformMessage(uint32_t message) = 0;

 protected:
  virtual ~ShillThirdPartyVpnObserver() {}

 private:
  DISALLOW_ASSIGN(ShillThirdPartyVpnObserver);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_OBSERVER_H_
