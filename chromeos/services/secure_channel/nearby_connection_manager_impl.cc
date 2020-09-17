// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/nearby_connection_manager_impl.h"

#include "base/memory/ptr_util.h"

namespace chromeos {

namespace secure_channel {

// static
NearbyConnectionManagerImpl::Factory*
    NearbyConnectionManagerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<NearbyConnectionManager>
NearbyConnectionManagerImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new NearbyConnectionManagerImpl());
}

// static
void NearbyConnectionManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyConnectionManagerImpl::Factory::~Factory() = default;

NearbyConnectionManagerImpl::NearbyConnectionManagerImpl() = default;

NearbyConnectionManagerImpl::~NearbyConnectionManagerImpl() = default;

void NearbyConnectionManagerImpl::PerformAttemptNearbyInitiatorConnection(
    const DeviceIdPair& device_id_pair) {}

void NearbyConnectionManagerImpl::PerformCancelNearbyInitiatorConnectionAttempt(
    const DeviceIdPair& device_id_pair) {}

}  // namespace secure_channel

}  // namespace chromeos
