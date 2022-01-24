// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_

#include "chromeos/services/secure_channel/public/mojom/nearby_connector.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace secure_channel {

// Provides Nearby Connections functionality to the SecureChannel service.
class NearbyConnector : public mojom::NearbyConnector {
 public:
  NearbyConnector();
  ~NearbyConnector() override;

  mojo::PendingRemote<mojom::NearbyConnector> GeneratePendingRemote();

 private:
  mojo::ReceiverSet<mojom::NearbyConnector> receiver_set_;
};

}  // namespace secure_channel
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
namespace secure_channel {
using ::chromeos::secure_channel::NearbyConnector;
}
}  // namespace ash

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_PUBLIC_CPP_CLIENT_NEARBY_CONNECTOR_H_
