// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_

class PrefRegistrySimple;

namespace ash {
namespace device_sync {

// Register's Device Sync's profile preferences for browser prefs.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace device_sync
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_DEVICE_SYNC_PREFS_H_
