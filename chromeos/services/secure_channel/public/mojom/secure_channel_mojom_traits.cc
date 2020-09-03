// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/mojom/secure_channel_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

chromeos::secure_channel::mojom::ConnectionMedium
EnumTraits<chromeos::secure_channel::mojom::ConnectionMedium,
           chromeos::secure_channel::ConnectionMedium>::
    ToMojom(chromeos::secure_channel::ConnectionMedium input) {
  switch (input) {
    case chromeos::secure_channel::ConnectionMedium::kBluetoothLowEnergy:
      return chromeos::secure_channel::mojom::ConnectionMedium::
          kBluetoothLowEnergy;
    case chromeos::secure_channel::ConnectionMedium::kNearbyConnections:
      return chromeos::secure_channel::mojom::ConnectionMedium::
          kNearbyConnections;
  }

  NOTREACHED();
  return chromeos::secure_channel::mojom::ConnectionMedium::kBluetoothLowEnergy;
}

bool EnumTraits<chromeos::secure_channel::mojom::ConnectionMedium,
                chromeos::secure_channel::ConnectionMedium>::
    FromMojom(chromeos::secure_channel::mojom::ConnectionMedium input,
              chromeos::secure_channel::ConnectionMedium* out) {
  switch (input) {
    case chromeos::secure_channel::mojom::ConnectionMedium::kBluetoothLowEnergy:
      *out = chromeos::secure_channel::ConnectionMedium::kBluetoothLowEnergy;
      return true;
    case chromeos::secure_channel::mojom::ConnectionMedium::kNearbyConnections:
      *out = chromeos::secure_channel::ConnectionMedium::kNearbyConnections;
      return true;
  }

  NOTREACHED();
  return false;
}

chromeos::secure_channel::mojom::ConnectionPriority
EnumTraits<chromeos::secure_channel::mojom::ConnectionPriority,
           chromeos::secure_channel::ConnectionPriority>::
    ToMojom(chromeos::secure_channel::ConnectionPriority input) {
  switch (input) {
    case chromeos::secure_channel::ConnectionPriority::kLow:
      return chromeos::secure_channel::mojom::ConnectionPriority::LOW;
    case chromeos::secure_channel::ConnectionPriority::kMedium:
      return chromeos::secure_channel::mojom::ConnectionPriority::MEDIUM;
    case chromeos::secure_channel::ConnectionPriority::kHigh:
      return chromeos::secure_channel::mojom::ConnectionPriority::HIGH;
  }

  NOTREACHED();
  return chromeos::secure_channel::mojom::ConnectionPriority::LOW;
}

bool EnumTraits<chromeos::secure_channel::mojom::ConnectionPriority,
                chromeos::secure_channel::ConnectionPriority>::
    FromMojom(chromeos::secure_channel::mojom::ConnectionPriority input,
              chromeos::secure_channel::ConnectionPriority* out) {
  switch (input) {
    case chromeos::secure_channel::mojom::ConnectionPriority::LOW:
      *out = chromeos::secure_channel::ConnectionPriority::kLow;
      return true;
    case chromeos::secure_channel::mojom::ConnectionPriority::MEDIUM:
      *out = chromeos::secure_channel::ConnectionPriority::kMedium;
      return true;
    case chromeos::secure_channel::mojom::ConnectionPriority::HIGH:
      *out = chromeos::secure_channel::ConnectionPriority::kHigh;
      return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
