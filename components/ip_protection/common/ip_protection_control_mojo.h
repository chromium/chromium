// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_MOJO_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_MOJO_H_

#include "components/ip_protection/common/ip_protection_control.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ip_protection {

// Bridges calls from `network::mojom::IpProtectionProxyDelegate` to
// `IpProtectionControl`.
class IpProtectionControlMojo final
    : public network::mojom::IpProtectionProxyDelegate {
 public:
  IpProtectionControlMojo(
      mojo::PendingReceiver<network::mojom::IpProtectionProxyDelegate>,
      IpProtectionControl* ip_protection_control);
  ~IpProtectionControlMojo() override;

  void VerifyIpProtectionConfigGetterForTesting(
      network::mojom::IpProtectionProxyDelegate::
          VerifyIpProtectionConfigGetterForTestingCallback callback) override;
  void InvalidateIpProtectionConfigCacheTryAgainAfterTime() override;
  void SetIpProtectionEnabled(bool enabled) override;
  void IsIpProtectionEnabledForTesting(
      network::mojom::IpProtectionProxyDelegate::
          IsIpProtectionEnabledForTestingCallback callback) override;

 private:
  const mojo::Receiver<network::mojom::IpProtectionProxyDelegate> receiver_;
  const raw_ptr<IpProtectionControl> ip_protection_control_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_MOJO_H_
