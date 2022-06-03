// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_

class PrefRegistrySimple;

namespace chromeos {
namespace device_sync {

// Register's Device Sync's profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace device_sync
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_
