// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/nearby_initiator_connection_attempt.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_operation.h"

namespace ash::secure_channel {

// static
NearbyInitiatorConnectionAttempt::Factory*
    NearbyInitiatorConnectionAttempt::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ConnectionAttempt<NearbyInitiatorFailureType>>
NearbyInitiatorConnectionAttempt::Factory::Create(
    NearbyConnectionManager* nearby_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details) {
  if (test_factory_) {
    return test_factory_->CreateInstance(nearby_connection_manager, delegate,
                                         connection_attempt_details);
  }

  return base::WrapUnique(new NearbyInitiatorConnectionAttempt(
      nearby_connection_manager, delegate, connection_attempt_details));
}

// static
void NearbyInitiatorConnectionAttempt::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

NearbyInitiatorConnectionAttempt::Factory::~Factory() = default;

NearbyInitiatorConnectionAttempt::NearbyInitiatorConnectionAttempt(
    NearbyConnectionManager* nearby_connection_manager,
    ConnectionAttemptDelegate* delegate,
    const ConnectionAttemptDetails& connection_attempt_details)
    : ConnectionAttemptBase<NearbyInitiatorFailureType>(
          delegate,
          connection_attempt_details),
      nearby_connection_manager_(nearby_connection_manager) {}

NearbyInitiatorConnectionAttempt::~NearbyInitiatorConnectionAttempt() = default;

std::unique_ptr<ConnectToDeviceOperation<NearbyInitiatorFailureType>>
NearbyInitiatorConnectionAttempt::CreateConnectToDeviceOperation(
    const DeviceIdPair& device_id_pair,
    ConnectionPriority connection_priority,
    ConnectToDeviceOperation<
        NearbyInitiatorFailureType>::ConnectionSuccessCallback success_callback,
    const ConnectToDeviceOperation<NearbyInitiatorFailureType>::
        ConnectionFailedCallback& failure_callback) {
  return NearbyInitiatorOperation::Factory::Create(
      nearby_connection_manager_, std::move(success_callback), failure_callback,
      base::BindRepeating(
          &NearbyInitiatorConnectionAttempt::OnBleDiscoveryStateChanged,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &NearbyInitiatorConnectionAttempt::OnNearbyConnectionStateChanged,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &NearbyInitiatorConnectionAttempt::OnSecureChannelStateChanged,
          weak_ptr_factory_.GetWeakPtr()),
      device_id_pair, connection_priority);
}

}  // namespace ash::secure_channel
