// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_

#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::secure_channel {

// Provides Nearby Connections functionality to the SecureChannel service.
class NearbyConnector : public mojom::NearbyConnector {
 public:
  NearbyConnector();
  ~NearbyConnector() override;

  mojo::PendingRemote<mojom::NearbyConnector> GeneratePendingRemote();

 private:
  mojo::ReceiverSet<mojom::NearbyConnector> receiver_set_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_
