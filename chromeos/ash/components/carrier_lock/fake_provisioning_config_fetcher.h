// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_PROVISIONING_CONFIG_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_PROVISIONING_CONFIG_FETCHER_H_

#include "chromeos/ash/components/carrier_lock/provisioning_config_fetcher.h"

namespace ash::carrier_lock {

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK)
    FakeProvisioningConfigFetcher : public ProvisioningConfigFetcher {
 public:
  FakeProvisioningConfigFetcher() = default;
  ~FakeProvisioningConfigFetcher() override = default;

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

  void SetConfigTopicAndResult(std::string configuration,
                               ::carrier_lock::CarrierRestrictionsMode mode,
                               std::string topic,
                               Result result);

 private:
  ::carrier_lock::CarrierRestrictionsMode mode_;
  std::string configuration_;
  std::string topic_;
  Result result_;
};

}  // namespace ash::carrier_lock

#endif  // CHROMEOS_ASH_COMPONENTS_CARRIER_LOCK_FAKE_PROVISIONING_CONFIG_FETCHER_H_
