// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace prefs {

const char kPrivacySandboxApisEnabled[] = "privacy_sandbox.apis_enabled";

const char kPrivacySandboxManuallyControlled[] =
    "privacy_sandbox.manually_controlled";

const char kPrivacySandboxPreferencesReconciled[] =
    "privacy_sandbox.preferences_reconciled";

const char kPrivacySandboxPageViewed[] = "privacy_sandbox.page_viewed";

const char kPrivacySandboxFlocDataAccessibleSince[] =
    "privacy_sandbox.floc_data_accessible_since";

extern const char kPrivacySandboxFlocEnabled[] = "privacy_sandbox.floc_enabled";

}  // namespace prefs

namespace privacy_sandbox {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxApisEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxManuallyControlled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxPreferencesReconciled,
                                false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxPageViewed, false);
  registry->RegisterTimePref(prefs::kPrivacySandboxFlocDataAccessibleSince,
                             base::Time());
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxFlocEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

}  // namespace privacy_sandbox
