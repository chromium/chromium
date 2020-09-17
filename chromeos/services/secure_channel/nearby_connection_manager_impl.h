// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_IMPL_H_

#include "chromeos/services/secure_channel/nearby_connection_manager.h"

namespace chromeos {

namespace secure_channel {

// TODO(https://crbug.com/1106937): Add real implementation.
class NearbyConnectionManagerImpl : public NearbyConnectionManager {
 public:
  class Factory {
   public:
    static std::unique_ptr<NearbyConnectionManager> Create();
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<NearbyConnectionManager> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  NearbyConnectionManagerImpl(const NearbyConnectionManagerImpl&) = delete;
  NearbyConnectionManagerImpl& operator=(const NearbyConnectionManagerImpl&) =
      delete;
  ~NearbyConnectionManagerImpl() override;

 private:
  NearbyConnectionManagerImpl();

  // NearbyConnectionManager:
  void PerformAttemptNearbyInitiatorConnection(
      const DeviceIdPair& device_id_pair) override;
  void PerformCancelNearbyInitiatorConnectionAttempt(
      const DeviceIdPair& device_id_pair) override;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_NEARBY_CONNECTION_MANAGER_IMPL_H_
