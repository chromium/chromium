// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_H_

#include "services/network/public/mojom/network_context.mojom.h"

namespace ip_protection {

// Interface for controlling IP Protection behavior.
//
// TODO: https://crbug.com/352005196 - rip out the remaining Mojo dependencies
// from this interface so that it can be used in environments that can't depend
// on Mojo (e.g. Cronet).
class IpProtectionControl {
 public:
  // Used to facilitate cross-process testing of the IP Protection feature. This
  // method will:
  //  - Disable active cache management and reset the IP Protection cache to a
  //    no-tokens and no-cooldown state, and,
  //  - Return the current cooldown if one has been set, or,
  //  - Initiate an IP Protection token request to the browser process and
  //    return either a returned token or the returned cooldown time
  virtual void VerifyIpProtectionConfigGetterForTesting(
      network::mojom::IpProtectionProxyDelegate::
          VerifyIpProtectionConfigGetterForTestingCallback callback) = 0;

  // Indicates that the IP Protection config cache in the Network Service should
  // no longer wait before requesting tokens from the browser process (called in
  // response to user account status changes that allow IP Protection to start
  // working as expected).
  virtual void InvalidateIpProtectionConfigCacheTryAgainAfterTime() = 0;

  // Indicates that the state of the IP Protection feature has changed and the
  // network service should update its state accordingly (including tearing
  // down existing proxied connections, if `value` is false).
  virtual void SetIpProtectionEnabled(bool enabled) = 0;

  // Returns the Network Service's state regarding whether IP Protection is
  // enabled, for testing.
  virtual void IsIpProtectionEnabledForTesting(
      network::mojom::IpProtectionProxyDelegate::
          IsIpProtectionEnabledForTestingCallback callback) = 0;

 protected:
  ~IpProtectionControl() = default;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_H_
