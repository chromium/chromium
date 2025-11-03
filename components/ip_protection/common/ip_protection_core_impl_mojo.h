// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_MOJO_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_MOJO_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/ip_protection/common/ip_protection_core_impl.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"
#include "components/ip_protection/mojom/core.mojom.h"
#include "components/ip_protection/mojom/core_test.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ip_protection {

class IpProtectionCoreHostRemote;
class IpProtectionProxyConfigManager;

// The Mojo implementation of IpProtectionCore, providing methods for CoreHost
// to call on the core, and supporting initialization.
class IpProtectionCoreImplMojo : public IpProtectionCoreImpl,
                                 public ip_protection::mojom::CoreControl,
                                 public ip_protection::mojom::CoreControlTest {
 public:
  // If `core_host_remote` is null, no tokens or proxy config will be provided.
  IpProtectionCoreImplMojo(
      mojo::PendingReceiver<ip_protection::mojom::CoreControl> pending_receiver,
      scoped_refptr<IpProtectionCoreHostRemote> core_host_remote,
      MaskedDomainListManager* masked_domain_list_manager,
      bool is_ip_protection_enabled,
      bool ip_protection_incognito,
      InitialTokensMap initial_tokens);
  ~IpProtectionCoreImplMojo() override;

  // Create an instance with parameters for IpProtectionCoreImpl and a
  // null core_host_remote.
  static IpProtectionCoreImplMojo CreateForTesting(
      MaskedDomainListManager* masked_domain_list_manager,
      std::unique_ptr<IpProtectionProxyConfigManager>
          ip_protection_proxy_config_manager,
      IpProtectionCoreImpl::ProxyTokenManagerMap ip_protection_token_managers,
      bool is_ip_protection_enabled,
      bool ip_protection_incognito);

  // `CoreControl` implementation.
  void AuthTokensMayBeAvailable() override;
  void SetIpProtectionEnabled(bool enabled) override;
  void BindTestInterfaceForTesting(
      mojo::PendingReceiver<ip_protection::mojom::CoreControlTest> receiver)
      override;

  // `CoreControlTest` implementation.
  void VerifyIpProtectionCoreHostForTesting(
      ip_protection::mojom::CoreControlTest::
          VerifyIpProtectionCoreHostForTestingCallback callback) override;
  void IsIpProtectionEnabledForTesting(
      ip_protection::mojom::CoreControlTest::
          IsIpProtectionEnabledForTestingCallback callback) override;
  void GetAuthTokenForTesting(
      ProxyLayer proxy_layer,
      const std::string& geo_id,
      ip_protection::mojom::CoreControlTest::GetAuthTokenForTestingCallback
          callback) override;
  void HasTrackingProtectionExceptionForTesting(
      const GURL& first_party_url,
      ip_protection::mojom::CoreControlTest::
          HasTrackingProtectionExceptionForTestingCallback callback) override;

 private:
  IpProtectionCoreImplMojo(
      MaskedDomainListManager* masked_domain_list_manager,
      std::unique_ptr<IpProtectionProxyConfigManager>
          ip_protection_proxy_config_manager,
      IpProtectionCoreImpl::ProxyTokenManagerMap ip_protection_token_managers,
      bool is_ip_protection_enabled,
      bool ip_protection_incognito);

  void OnIpProtectionConfigAvailableForTesting(
      VerifyIpProtectionCoreHostForTestingCallback callback);

  const mojo::Receiver<ip_protection::mojom::CoreControl> receiver_;
  mojo::ReceiverSet<ip_protection::mojom::CoreControlTest>
      test_receivers_for_testing_;

  base::WeakPtrFactory<IpProtectionCoreImplMojo> weak_ptr_factory_{this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_CORE_IMPL_MOJO_H_
