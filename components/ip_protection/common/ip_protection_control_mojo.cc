// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_control_mojo.h"

#include "components/ip_protection/common/ip_protection_control.h"

namespace ip_protection {

IpProtectionControlMojo::IpProtectionControlMojo(
    mojo::PendingReceiver<network::mojom::IpProtectionControl> pending_receiver,
    ip_protection::IpProtectionControl* ip_protection_control)
    : receiver_(this, std::move(pending_receiver)),
      ip_protection_control_(ip_protection_control) {}

IpProtectionControlMojo::~IpProtectionControlMojo() = default;

void IpProtectionControlMojo::VerifyIpProtectionConfigGetterForTesting(
    network::mojom::IpProtectionControl::
        VerifyIpProtectionConfigGetterForTestingCallback callback) {
  return ip_protection_control_
      ->VerifyIpProtectionConfigGetterForTesting(  // IN-TEST
          std::move(callback));
}

void IpProtectionControlMojo::
    InvalidateIpProtectionConfigCacheTryAgainAfterTime() {
  return ip_protection_control_
      ->InvalidateIpProtectionConfigCacheTryAgainAfterTime();
}

void IpProtectionControlMojo::SetIpProtectionEnabled(bool enabled) {
  return ip_protection_control_->SetIpProtectionEnabled(enabled);
}

void IpProtectionControlMojo::IsIpProtectionEnabledForTesting(
    network::mojom::IpProtectionControl::IsIpProtectionEnabledForTestingCallback
        callback) {
  std::move(callback).Run(
      ip_protection_control_->IsIpProtectionEnabledForTesting());  // IN-TEST
}

}  // namespace ip_protection
