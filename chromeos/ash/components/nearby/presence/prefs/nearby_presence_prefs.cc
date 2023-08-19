// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/prefs/nearby_presence_prefs.h"

#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::nearby::presence {

void RegisterNearbyPresencePrefs(PrefRegistrySimple* registry) {
  RegisterNearbyPresenceCredentialPrefs(registry);
}

}  // namespace ash::nearby::presence
