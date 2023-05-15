// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PRESENCE_H_
#define CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PRESENCE_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_presence.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "testing/gmock/include/gmock/gmock.h"

using NearbyPresenceMojom = ::ash::nearby::presence::mojom::NearbyPresence;

namespace ash::nearby::presence {

class FakeNearbyPresence : public mojom::NearbyPresence {
 public:
  FakeNearbyPresence();
  FakeNearbyPresence(const FakeNearbyPresence&) = delete;
  FakeNearbyPresence& operator=(const FakeNearbyPresence&) = delete;
  ~FakeNearbyPresence() override;

  const mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>&
  shared_remote() const {
    return shared_remote_;
  }

  void BindInterface(
      mojo::PendingReceiver<ash::nearby::presence::mojom::NearbyPresence>
          pending_receiver);

 private:
  mojo::ReceiverSet<::ash::nearby::presence::mojom::NearbyPresence>
      receiver_set_;
  mojo::SharedRemote<::ash::nearby::presence::mojom::NearbyPresence>
      shared_remote_;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_SERVICES_NEARBY_PUBLIC_CPP_FAKE_NEARBY_PRESENCE_H_
