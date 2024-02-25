// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/fake_provisioning_config_fetcher.h"

namespace ash::carrier_lock {

void FakeProvisioningConfigFetcher::RequestConfig(
    const std::string& serial,
    const std::string& imei,
    const std::string& manufacturer,
    const std::string& model,
    const std::string& fcm_token,
    Callback callback) {
  std::move(callback).Run(result_);
}

std::string FakeProvisioningConfigFetcher::GetFcmTopic() {
  return topic_;
}

std::string FakeProvisioningConfigFetcher::GetSignedConfig() {
  return configuration_;
}

::carrier_lock::CarrierRestrictionsMode
FakeProvisioningConfigFetcher::GetRestrictionMode() {
  return mode_;
}

RestrictedNetworks FakeProvisioningConfigFetcher::GetNumberOfNetworks() {
  RestrictedNetworks result = {0, 0};

  if (mode_ == ::carrier_lock::DEFAULT_DISALLOW) {
    result.allowed = 1;
  }

  return result;
}

void FakeProvisioningConfigFetcher::SetConfigTopicAndResult(
    std::string configuration,
    ::carrier_lock::CarrierRestrictionsMode mode,
    std::string topic,
    Result result) {
  configuration_ = configuration;
  mode_ = mode;
  topic_ = topic;
  result_ = result;
}

}  // namespace ash::carrier_lock
