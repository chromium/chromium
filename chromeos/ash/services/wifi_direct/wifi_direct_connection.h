// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_WIFI_DIRECT_WIFI_DIRECT_CONNECTION_H_
#define CHROMEOS_ASH_SERVICES_WIFI_DIRECT_WIFI_DIRECT_CONNECTION_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_controller.h"
#include "chromeos/ash/components/wifi_p2p/wifi_p2p_group.h"
#include "chromeos/ash/services/wifi_direct/public/mojom/wifi_direct_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::wifi_direct {

// Implementation of mojom::WifiDirectConnection. This class represents an
// active Wifi direct connection on the device. It could either be created by
// a group owner or connected by a group client.
class WifiDirectConnection : public mojom::WifiDirectConnection {
 public:
  using InstanceWithPendingRemotePair =
      std::pair<std::unique_ptr<WifiDirectConnection>,
                mojo::PendingRemote<mojom::WifiDirectConnection>>;

  static InstanceWithPendingRemotePair Create(
      const WifiP2PGroup& group_metadata,
      base::OnceClosure disconnect_handler);

  WifiDirectConnection(const WifiDirectConnection&) = delete;
  WifiDirectConnection& operator=(const WifiDirectConnection&) = delete;
  ~WifiDirectConnection() override;

  // Returns a boolean indicating if the current device is the owner or not.
  bool IsOwner() const;

  // mojom::WifiDirectConnection
  void GetProperties(GetPropertiesCallback callback) override;
  void AssociateSocket(mojo::PlatformHandle socket,
                       AssociateSocketCallback callback) override;

  void FlushForTesting();

 private:
  WifiDirectConnection(const WifiP2PGroup& group_metadata);
  mojo::PendingRemote<mojom::WifiDirectConnection> CreateRemote(
      base::OnceClosure disconnect_handler);

  base::ElapsedTimer duration_timer_;
  mojo::Receiver<mojom::WifiDirectConnection> receiver_{this};
  WifiP2PGroup group_metadata_;
};

}  // namespace ash::wifi_direct

#endif  // CHROMEOS_ASH_SERVICES_WIFI_DIRECT_WIFI_DIRECT_CONNECTION_H_
