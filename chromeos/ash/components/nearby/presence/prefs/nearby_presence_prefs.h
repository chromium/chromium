// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_PREFS_NEARBY_PRESENCE_PREFS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_PREFS_NEARBY_PRESENCE_PREFS_H_

class PrefRegistrySimple;

namespace ash::nearby::presence {

void RegisterNearbyPresencePrefs(PrefRegistrySimple* registry);

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_PREFS_NEARBY_PRESENCE_PREFS_H_
