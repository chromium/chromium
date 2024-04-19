// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/bluetooth_utils.h"

namespace nearby::chrome {

nearby::Uuid BluetoothUuidToNearbyUuid(
    const device::BluetoothUUID& bluetooth_service_uuid) {
  auto uint_bytes = bluetooth_service_uuid.GetBytes();
  uint64_t most_sig_bits = 0;
  uint64_t least_sig_bits = 0;
  for (int i = 0; i < 8; i++) {
    most_sig_bits |= static_cast<uint64_t>(uint_bytes[i]) << ((7 - i) * 8);
    least_sig_bits |= static_cast<uint64_t>(uint_bytes[i + 8]) << ((7 - i) * 8);
  }
  return nearby::Uuid{most_sig_bits, least_sig_bits};
}

}  // namespace nearby::chrome
