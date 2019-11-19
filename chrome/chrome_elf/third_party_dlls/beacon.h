// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_ELF_THIRD_PARTY_DLLS_BEACON_H_
#define CHROME_CHROME_ELF_THIRD_PARTY_DLLS_BEACON_H_

namespace third_party_dlls {

// Attempts to leave a beacon in the current user's registry hive. If the beacon
// doesn't say it is enabled or there are any other errors when creating the
// beacon, returns false. Otherwise returns true. The intent of the beacon is to
// act as an extra failure mode protection whereby if Chrome repeatedly fails to
// start during the initialization of third-party DLL blocking, it will skip
// blocking on the subsequent run.
bool LeaveSetupBeacon();

// Looks for the setup running beacon that LeaveSetupBeacon() creates and resets
// it to to show the setup was successful.
// Returns true if the beacon was successfully set to BLACKLIST_ENABLED.
bool ResetBeacon();

}  // namespace third_party_dlls

#endif  // CHROME_CHROME_ELF_THIRD_PARTY_DLLS_BEACON_H_
