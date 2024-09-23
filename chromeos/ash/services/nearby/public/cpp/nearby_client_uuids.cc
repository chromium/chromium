// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/nearby/public/cpp/nearby_client_uuids.h"

#include <set>
#include <string>

#include "base/containers/contains.h"
#include "base/no_destructor.h"

namespace ash {
namespace nearby {

namespace {
const char kDataMigrationUuid[] = "60c68e7e-5acc-3ac1-a505-5d3beb02fec4";
const char kNearbySharingUuid[] = "a82efa21-ae5c-3dde-9bbc-f16da7b16c5a";
const char kSecureChannelUuid[] = "a384bd4f-41ea-3b02-8901-8c2ed9a79970";
const char kQuickStartUuid[] = "5e164731-1999-3405-9061-08a610fd3787";
}  // namespace

const std::vector<device::BluetoothUUID>& GetNearbyClientUuids() {
  static const base::NoDestructor<std::vector<device::BluetoothUUID>>
      kAllowedUuids([] {
        // This literal initialization unfortunately does not work with
        // base::NoDestructor.
        std::vector<device::BluetoothUUID> allowed_uuids{
            device::BluetoothUUID(kDataMigrationUuid),
            device::BluetoothUUID(kNearbySharingUuid),
            device::BluetoothUUID(kSecureChannelUuid),
            device::BluetoothUUID(kQuickStartUuid)};
        return allowed_uuids;
      }());

  return *kAllowedUuids;
}

bool IsNearbyClientUuid(const device::BluetoothUUID& uuid) {
  static const base::NoDestructor<std::set<device::BluetoothUUID>>
      kAllowedUuidSet(std::begin(GetNearbyClientUuids()),
                      std::end(GetNearbyClientUuids()));
  return base::Contains(*kAllowedUuidSet, uuid);
}

}  // namespace nearby
}  // namespace ash
