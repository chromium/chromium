// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/soda_testing_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace ash::babelorca {
void RegisterSodaPrefsForTesting(PrefRegistrySimple* pref_service) {
  // During Soda installer's init method it checks these prefs, if none are
  // enabled and `kClassManagementToolsAvailabilitySetting` is not set to
  // teacher then it will not install SODA.  By setting everything except for
  // the classroom management setting to false we ensure that SODA will
  // install even if just school tools teacher is enabled on the current
  // profile. Callers to this method will need to set the boca pref to kTeacher.
  pref_service->RegisterBooleanPref(::prefs::kLiveCaptionEnabled, false);
  pref_service->RegisterBooleanPref(
      ::ash::prefs::kAccessibilityDictationEnabled, false);
  pref_service->RegisterBooleanPref(::ash::prefs::kProjectorCreationFlowEnabled,
                                    false);
}

MockSodaInstaller::MockSodaInstaller() = default;
MockSodaInstaller::~MockSodaInstaller() = default;
}  // namespace ash::babelorca
