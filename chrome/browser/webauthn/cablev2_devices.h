// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CABLEV2_DEVICES_H_
#define CHROME_BROWSER_WEBAUTHN_CABLEV2_DEVICES_H_

#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "device/fido/fido_constants.h"
#include "third_party/icu/source/common/unicode/locid.h"

class Profile;

namespace base {
class Time;
}

namespace syncer {
class DeviceInfo;
}

namespace device::cablev2 {
struct Pairing;
}

namespace cablev2 {

// PairingFromSyncedDevice parses a `Pairing` from Sync's information about a
// device. This is exposed for testing.
std::unique_ptr<device::cablev2::Pairing> PairingFromSyncedDevice(
    const syncer::DeviceInfo* device,
    const base::Time& now);

// KnownDevices reflects the browser's knowledge of known caBLEv2 devices.
// caBLEv2 is the protocol used when phones are acting as security keys. (Except
// for the case where accounts.google.com is using caBLEv1, but Chrome doesn't
// store any state for caBLEv1.)
//
// The only source of caBLEv2 pairing information are DeviceInfo entries in
// Sync.
//
// Call `FromProfile` to load the information from sync data.
// Alternatively, for testing, information can be added directly to the
// vectors.
struct KnownDevices {
  KnownDevices();
  ~KnownDevices();
  KnownDevices(const KnownDevices&) = delete;
  KnownDevices(const KnownDevices&&) = delete;
  KnownDevices& operator=(const KnownDevices&) = delete;

  // FromProfile returns a `KnownDevices` by extracting them from the
  // Sync data of `browser_context`.
  static std::unique_ptr<KnownDevices> FromProfile(Profile* profile);

  // Names returns a list of all names (which may contain duplicates).
  std::vector<std::string_view> Names() const;

  std::vector<std::unique_ptr<device::cablev2::Pairing>> synced_devices;
};

}  // namespace cablev2

#endif  // CHROME_BROWSER_WEBAUTHN_CABLEV2_DEVICES_H_
