// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/shared/connection_medium.h"

#include "base/check.h"

namespace chromeos {

namespace secure_channel {

std::ostream& operator<<(std::ostream& stream, const ConnectionMedium& medium) {
  switch (medium) {
    case ConnectionMedium::kBluetoothLowEnergy:
      stream << "[BLE]";
      break;
    case ConnectionMedium::kNearbyConnections:
      stream << "[Nearby Connections]";
      break;
  }
  return stream;
}

}  // namespace secure_channel

}  // namespace chromeos
