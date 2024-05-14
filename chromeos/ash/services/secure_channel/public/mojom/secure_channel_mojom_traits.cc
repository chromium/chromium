// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

ash::secure_channel::mojom::ConnectionMedium
EnumTraits<ash::secure_channel::mojom::ConnectionMedium,
           ash::secure_channel::ConnectionMedium>::
    ToMojom(ash::secure_channel::ConnectionMedium input) {
  switch (input) {
    case ash::secure_channel::ConnectionMedium::kBluetoothLowEnergy:
      return ash::secure_channel::mojom::ConnectionMedium::kBluetoothLowEnergy;
    case ash::secure_channel::ConnectionMedium::kNearbyConnections:
      return ash::secure_channel::mojom::ConnectionMedium::kNearbyConnections;
  }

  NOTREACHED_IN_MIGRATION();
  return ash::secure_channel::mojom::ConnectionMedium::kBluetoothLowEnergy;
}

bool EnumTraits<ash::secure_channel::mojom::ConnectionMedium,
                ash::secure_channel::ConnectionMedium>::
    FromMojom(ash::secure_channel::mojom::ConnectionMedium input,
              ash::secure_channel::ConnectionMedium* out) {
  switch (input) {
    case ash::secure_channel::mojom::ConnectionMedium::kBluetoothLowEnergy:
      *out = ash::secure_channel::ConnectionMedium::kBluetoothLowEnergy;
      return true;
    case ash::secure_channel::mojom::ConnectionMedium::kNearbyConnections:
      *out = ash::secure_channel::ConnectionMedium::kNearbyConnections;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

ash::secure_channel::mojom::ConnectionPriority
EnumTraits<ash::secure_channel::mojom::ConnectionPriority,
           ash::secure_channel::ConnectionPriority>::
    ToMojom(ash::secure_channel::ConnectionPriority input) {
  switch (input) {
    case ash::secure_channel::ConnectionPriority::kLow:
      return ash::secure_channel::mojom::ConnectionPriority::LOW;
    case ash::secure_channel::ConnectionPriority::kMedium:
      return ash::secure_channel::mojom::ConnectionPriority::MEDIUM;
    case ash::secure_channel::ConnectionPriority::kHigh:
      return ash::secure_channel::mojom::ConnectionPriority::HIGH;
  }

  NOTREACHED_IN_MIGRATION();
  return ash::secure_channel::mojom::ConnectionPriority::LOW;
}

bool EnumTraits<ash::secure_channel::mojom::ConnectionPriority,
                ash::secure_channel::ConnectionPriority>::
    FromMojom(ash::secure_channel::mojom::ConnectionPriority input,
              ash::secure_channel::ConnectionPriority* out) {
  switch (input) {
    case ash::secure_channel::mojom::ConnectionPriority::LOW:
      *out = ash::secure_channel::ConnectionPriority::kLow;
      return true;
    case ash::secure_channel::mojom::ConnectionPriority::MEDIUM:
      *out = ash::secure_channel::ConnectionPriority::kMedium;
      return true;
    case ash::secure_channel::mojom::ConnectionPriority::HIGH:
      *out = ash::secure_channel::ConnectionPriority::kHigh;
      return true;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
