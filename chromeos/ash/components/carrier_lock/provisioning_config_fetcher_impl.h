// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PROVISIONING_CONFIG_FETCHER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PROVISIONING_CONFIG_FETCHER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/carrier_lock/carrier_lock.pb.h"
#include "chromeos/ash/components/carrier_lock/common.h"
#include "chromeos/ash/components/carrier_lock/provisioning_config_fetcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ash::carrier_lock {

// This class handles communication with Carrier Lock (SimLock) provisioning
// server managed by Pixel team in order to receive configuration for modem.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    ProvisioningConfigFetcherImpl : public ProvisioningConfigFetcher {
 public:
  explicit ProvisioningConfigFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory>);
  ProvisioningConfigFetcherImpl() = delete;
  ~ProvisioningConfigFetcherImpl() override;

  // ProvisioningConfigFetcher
  void RequestConfig(const std::string& serial,
                     const std::string& imei,
                     const std::string& manufacturer,
                     const std::string& model,
                     const std::string& fcm_token,
                     Callback callback) override;
  std::string GetFcmTopic() override;
  std::string GetSignedConfig() override;
  ::carrier_lock::CarrierRestrictionsMode GetRestrictionMode() override;
  RestrictedNetworks GetNumberOfNetworks() override;

 private:
  void OnDownloadToStringComplete(std::unique_ptr<std::string> response_body);
  void ReturnError(Result);

  ::carrier_lock::DeviceProvisioningConfig provision_config_;
  Callback config_callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::WeakPtrFactory<ProvisioningConfigFetcherImpl> weak_ptr_factory_{this};
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_PROVISIONING_CONFIG_FETCHER_IMPL_H_
