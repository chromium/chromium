// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_ACTIVE_CONNECTION_MANAGER_IMPL_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_ACTIVE_CONNECTION_MANAGER_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromeos/services/secure_channel/active_connection_manager.h"
#include "chromeos/services/secure_channel/connection_details.h"
#include "chromeos/services/secure_channel/multiplexed_channel.h"
#include "chromeos/services/secure_channel/public/cpp/shared/connection_medium.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace chromeos {

namespace secure_channel {

// Concrete ActiveConnectionManager implementation, which utilizes
// MultiplexedChannel instances to share individual connected channels with
// multiple clients.
class ActiveConnectionManagerImpl : public ActiveConnectionManager,
                                    public MultiplexedChannel::Delegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<ActiveConnectionManager> Create(
        ActiveConnectionManager::Delegate* delegate);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ActiveConnectionManager> CreateInstance(
        ActiveConnectionManager::Delegate* delegate) = 0;

   private:
    static Factory* test_factory_;
  };

  ~ActiveConnectionManagerImpl() override;

 private:
  ActiveConnectionManagerImpl(ActiveConnectionManager::Delegate* delegate);

  // ActiveConnectionManager:
  ConnectionState GetConnectionState(
      const ConnectionDetails& connection_details) const override;
  void PerformAddActiveConnection(
      std::unique_ptr<AuthenticatedChannel> authenticated_channel,
      std::vector<std::unique_ptr<ClientConnectionParameters>> initial_clients,
      const ConnectionDetails& connection_details) override;
  void PerformAddClientToChannel(
      std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
      const ConnectionDetails& connection_details) override;

  // MultiplexedChannel::Delegate:
  void OnDisconnected(const ConnectionDetails& connection_details) override;

  base::flat_map<ConnectionDetails, std::unique_ptr<MultiplexedChannel>>
      details_to_channel_map_;

  DISALLOW_COPY_AND_ASSIGN(ActiveConnectionManagerImpl);
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_ACTIVE_CONNECTION_MANAGER_IMPL_H_
