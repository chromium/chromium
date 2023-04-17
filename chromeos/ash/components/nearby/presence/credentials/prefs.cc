// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/credentials/prefs.h"

#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::nearby::presence {

namespace prefs {

const char kNearbyPresenceDeviceIdPrefName[] =
    "nearby_presence.local_device_id";
const char kNearbyPresenceUserNamePrefName[] = "nearby_presence.user_name";
const char kNearbyPresenceProfileUrlPrefName[] = "nearby_presence.profile_url";
const char kNearbyPresenceSharedCredentialIdListPrefName[] =
    "nearby_presence.shared_credential_id_list";

}  // namespace prefs

void RegisterNearbyPresenceCredentialPrefs(PrefRegistrySimple* registry) {
  // These prefs are not synced across devices on purpose.
  registry->RegisterStringPref(prefs::kNearbyPresenceDeviceIdPrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbyPresenceUserNamePrefName,
                               /*default_value=*/std::string());
  registry->RegisterStringPref(prefs::kNearbyPresenceProfileUrlPrefName,
                               /*default_value=*/std::string());
  registry->RegisterListPref(
      prefs::kNearbyPresenceSharedCredentialIdListPrefName);
}

}  // namespace ash::nearby::presence
