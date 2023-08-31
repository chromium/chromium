// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace privacy_sandbox::tracking_protection {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kBlockAll3pcToggleEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kTrackingProtectionLevel,
      static_cast<int>(TrackingProtectionLevel::kStandard),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace privacy_sandbox::tracking_protection
