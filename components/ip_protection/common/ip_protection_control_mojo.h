// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_MOJO_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_MOJO_H_

#include "components/ip_protection/common/ip_protection_control.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "components/ip_protection/mojom/data_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ip_protection {

// Bridges calls from `ip_protection::mojom::CoreControl` to
// `ip_protection::IpProtectionControl`.
class IpProtectionControlMojo final : public ip_protection::mojom::CoreControl {
 public:
  IpProtectionControlMojo(
      mojo::PendingReceiver<ip_protection::mojom::CoreControl>,
      ip_protection::IpProtectionControl* ip_protection_control);
  ~IpProtectionControlMojo() override;

  void VerifyIpProtectionCoreHostForTesting(
      ip_protection::mojom::CoreControl::
          VerifyIpProtectionCoreHostForTestingCallback callback) override;
  void AuthTokensMayBeAvailable() override;
  void SetIpProtectionEnabled(bool enabled) override;
  void IsIpProtectionEnabledForTesting(
      ip_protection::mojom::CoreControl::IsIpProtectionEnabledForTestingCallback
          callback) override;

 private:
  const mojo::Receiver<ip_protection::mojom::CoreControl> receiver_;
  const raw_ptr<ip_protection::IpProtectionControl> ip_protection_control_;
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CONTROL_MOJO_H_
