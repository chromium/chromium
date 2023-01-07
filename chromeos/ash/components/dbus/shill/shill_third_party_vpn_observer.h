// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_OBSERVER_H_

#include <stdint.h>
#include <vector>

namespace ash {

// This is a base class for observers which handle signals sent by the
// ThirdPartyVpnAdaptor in Shill.
class ShillThirdPartyVpnObserver {
 public:
  ShillThirdPartyVpnObserver& operator=(const ShillThirdPartyVpnObserver&) =
      delete;

  virtual void OnPacketReceived(const std::vector<char>& data) = 0;
  virtual void OnPlatformMessage(uint32_t message) = 0;

 protected:
  virtual ~ShillThirdPartyVpnObserver() {}
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_THIRD_PARTY_VPN_OBSERVER_H_
