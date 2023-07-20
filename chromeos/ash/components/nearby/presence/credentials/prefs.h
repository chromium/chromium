// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_PREFS_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_PREFS_H_

class PrefRegistrySimple;

namespace ash::nearby::presence {

namespace prefs {

extern const char kNearbyPresenceFirstTimeRegistrationComplete[];
extern const char kNearbyPresenceDeviceIdPrefName[];
extern const char kNearbyPresenceUserNamePrefName[];
extern const char kNearbyPresenceProfileUrlPrefName[];
extern const char kNearbyPresenceSharedCredentialIdListPrefName[];
extern const char kNearbyPresenceSchedulingFirstTimeRegistrationPrefName[];
extern const char kNearbyPresenceSchedulingUploadPrefName[];
extern const char kNearbyPresenceSchedulingDownloadPrefName[];
extern const char kNearbyPresenceSchedulingCredentialDailySyncPrefName[];

}  // namespace prefs

void RegisterNearbyPresenceCredentialPrefs(PrefRegistrySimple* registry);

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_PREFS_H_
