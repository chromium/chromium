// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_MOJO_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "components/ip_protection/common/ip_protection_core_impl.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "components/ip_protection/mojom/data_types.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ip_protection {

class IpProtectionCoreHostRemote;
class IpProtectionProxyConfigManaager;
class IpProtectionTokenManaager;

// The Mojo implementation of IpProtectionCore, providing methods for CoreHost
// to call on the core, and supporting initialization.
class IpProtectionCoreImplMojo : public IpProtectionCoreImpl,
                                 public ip_protection::mojom::CoreControl {
 public:
  // If `core_host_remote` is null, no tokens or proxy config will be provided.
  IpProtectionCoreImplMojo(
      mojo::PendingReceiver<ip_protection::mojom::CoreControl> pending_receiver,
      scoped_refptr<IpProtectionCoreHostRemote> core_host_remote,
      MaskedDomainListManager* masked_domain_list_manager,
      bool is_ip_protection_enabled);
  ~IpProtectionCoreImplMojo() override;

  // Create an instance with parameters for IpProtectionCoreImpl and a
  // null core_host_remote.
  static IpProtectionCoreImplMojo CreateForTesting(
      MaskedDomainListManager* masked_domain_list_manager,
      std::unique_ptr<IpProtectionProxyConfigManager>
          ip_protection_proxy_config_manager,
      std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>
          ip_protection_token_managers,
      bool is_ip_protection_enabled);

  // `CoreControl` implementation.
  void VerifyIpProtectionCoreHostForTesting(
      ip_protection::mojom::CoreControl::
          VerifyIpProtectionCoreHostForTestingCallback callback) override;
  void AuthTokensMayBeAvailable() override;
  void SetIpProtectionEnabled(bool enabled) override;
  void IsIpProtectionEnabledForTesting(
      ip_protection::mojom::CoreControl::IsIpProtectionEnabledForTestingCallback
          callback) override;

 private:
  IpProtectionCoreImplMojo(
      MaskedDomainListManager* masked_domain_list_manager,
      std::unique_ptr<IpProtectionProxyConfigManager>
          ip_protection_proxy_config_manager,
      std::map<ProxyLayer, std::unique_ptr<IpProtectionTokenManager>>
          ip_protection_token_managers,
      bool is_ip_protection_enabled);

  void OnIpProtectionConfigAvailableForTesting(
      VerifyIpProtectionCoreHostForTestingCallback callback);

  const mojo::Receiver<ip_protection::mojom::CoreControl> receiver_;

  base::WeakPtrFactory<IpProtectionCoreImplMojo> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_MOJO_H_
