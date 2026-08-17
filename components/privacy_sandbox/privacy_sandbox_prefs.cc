// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"

namespace privacy_sandbox {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1TopicsEnabled, false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1FledgeEnabled, false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1AdMeasurementEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kPrivacySandboxM1Restricted, false);

  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxRelatedWebsiteSetsDataAccessAllowedInitialized,
      false);
  registry->RegisterBooleanPref(
      prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void MaybeClearAdPrivacyPrefs(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(kPrivacySandboxAdPrivacyUxDeprecation)) {
    return;
  }
  prefs->ClearPref(prefs::kPrivacySandboxM1TopicsEnabled);
  prefs->ClearPref(prefs::kPrivacySandboxM1FledgeEnabled);
  prefs->ClearPref(prefs::kPrivacySandboxM1AdMeasurementEnabled);
}

}  // namespace privacy_sandbox
