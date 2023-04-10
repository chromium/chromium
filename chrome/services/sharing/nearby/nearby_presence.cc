// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_presence.h"

namespace ash::nearby::presence {

NearbyPresence::NearbyPresence(
    mojo::PendingReceiver<mojom::NearbyPresence> nearby_presence,
    base::OnceClosure on_disconnect)
    : nearby_presence_(this, std::move(nearby_presence)) {
  nearby_presence_.set_disconnect_handler(std::move(on_disconnect));
}

NearbyPresence::~NearbyPresence() = default;

}  // namespace ash::nearby::presence
