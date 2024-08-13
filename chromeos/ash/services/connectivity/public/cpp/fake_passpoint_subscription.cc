// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/connectivity/public/cpp/fake_passpoint_subscription.h"

namespace ash::connectivity {

FakePasspointSubscription::FakePasspointSubscription(
    const std::string id,
    const std::string friendly_name,
    const std::string provisioning_source,
    std::optional<std::string> trusted_ca,
    int64_t expiration_epoch_ms,
    const std::vector<std::string> domains)
    : id_(id),
      friendly_name_(friendly_name),
      provisioning_source_(provisioning_source),
      trusted_ca_(trusted_ca),
      expiration_epoch_ms_(expiration_epoch_ms),
      domains_(domains) {}

FakePasspointSubscription::~FakePasspointSubscription() = default;

FakePasspointSubscription::FakePasspointSubscription(
    const FakePasspointSubscription&) = default;

FakePasspointSubscription& FakePasspointSubscription::operator=(
    const FakePasspointSubscription&) = default;

void FakePasspointSubscription::AddDomain(const std::string& domain) {
  domains_.push_back(domain);
}

}  // namespace ash::connectivity
