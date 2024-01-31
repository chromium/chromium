// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_CONNECTION_ATTEMPT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_CONNECTION_ATTEMPT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/secure_channel/connection_attempt_base.h"
#include "chromeos/ash/services/secure_channel/nearby_initiator_failure_type.h"

namespace ash::secure_channel {

class NearbyConnectionManager;

// Attempts to connect to a remote device over Nearby Connections via the
// initiator role.
class NearbyInitiatorConnectionAttempt
    : public ConnectionAttemptBase<NearbyInitiatorFailureType> {
 public:
  class Factory {
   public:
    static std::unique_ptr<ConnectionAttempt<NearbyInitiatorFailureType>>
    Create(NearbyConnectionManager* nearby_connection_manager,
           ConnectionAttemptDelegate* delegate,
           const ConnectionAttemptDetails& connection_attempt_details);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ConnectionAttempt<NearbyInitiatorFailureType>>
    CreateInstance(
        NearbyConnectionManager* nearby_connection_manager,
        ConnectionAttemptDelegate* delegate,
        const ConnectionAttemptDetails& connection_attempt_details) = 0;

   private:
    static Factory* test_factory_;
  };

  NearbyInitiatorConnectionAttempt(const NearbyInitiatorConnectionAttempt&) =
      delete;
  NearbyInitiatorConnectionAttempt& operator=(
      const NearbyInitiatorConnectionAttempt&) = delete;
  ~NearbyInitiatorConnectionAttempt() override;

 private:
  NearbyInitiatorConnectionAttempt(
      NearbyConnectionManager* nearby_connection_manager,
      ConnectionAttemptDelegate* delegate,
      const ConnectionAttemptDetails& connection_attempt_details);

  std::unique_ptr<ConnectToDeviceOperation<NearbyInitiatorFailureType>>
  CreateConnectToDeviceOperation(
      const DeviceIdPair& device_id_pair,
      ConnectionPriority connection_priority,
      ConnectToDeviceOperation<NearbyInitiatorFailureType>::
          ConnectionSuccessCallback success_callback,
      const ConnectToDeviceOperation<NearbyInitiatorFailureType>::
          ConnectionFailedCallback& failure_callback) override;

  raw_ptr<NearbyConnectionManager> nearby_connection_manager_;

  base::WeakPtrFactory<NearbyInitiatorConnectionAttempt> weak_ptr_factory_{
      this};
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_NEARBY_INITIATOR_CONNECTION_ATTEMPT_H_
