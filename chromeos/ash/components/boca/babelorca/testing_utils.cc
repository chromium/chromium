// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/testing_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "chromeos/ash/components/boca/babelorca/pref_names.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"

namespace ash::babelorca {

void RegisterPrefsForTesting(TestingPrefServiceSimple* pref_service) {
  pref_service->registry()->RegisterStringPref(
      ::prefs::kAccessibilityCaptionsTextSize, kCaptionsTextSize);
  pref_service->registry()->RegisterStringPref(
      ::prefs::kAccessibilityCaptionsTextFont, kCaptionsTextFont);
  pref_service->registry()->RegisterStringPref(
      ::prefs::kAccessibilityCaptionsTextColor, kCaptionsTextColor);
  pref_service->registry()->RegisterIntegerPref(
      ::prefs::kAccessibilityCaptionsTextOpacity, kCaptionsTextOpacity);
  pref_service->registry()->RegisterStringPref(
      ::prefs::kAccessibilityCaptionsBackgroundColor, kCaptionsBackgroundColor);
  pref_service->registry()->RegisterStringPref(
      ::prefs::kAccessibilityCaptionsTextShadow, kCaptionsTextShadow);
  pref_service->registry()->RegisterIntegerPref(
      ::prefs::kAccessibilityCaptionsBackgroundOpacity,
      kCaptionsBackgroundOpacity);
  pref_service->registry()->RegisterStringPref(
      ash::babelorca::prefs::kTranslateTargetLanguageCode,
      kTranslationTargetLocale);
}

void RegisterSodaPrefsForTesting(TestingPrefServiceSimple* pref_service) {
  // During Soda installer's init method it checks these prefs, if none are
  // enabled and `kClassManagementToolsAvailabilitySetting` is not set to
  // teacher then it will not install SODA.  By setting everything except for
  // the classroom management setting to false we ensure that SODA will
  // install even if just school tools teacher is enabled on the current
  // profile.
  pref_service->registry()->RegisterBooleanPref(::prefs::kLiveCaptionEnabled,
                                                false);
  pref_service->registry()->RegisterBooleanPref(
      ::ash::prefs::kAccessibilityDictationEnabled, false);
  pref_service->registry()->RegisterBooleanPref(
      ::ash::prefs::kProjectorCreationFlowEnabled, false);
}

MockSodaInstaller::MockSodaInstaller() = default;
MockSodaInstaller::~MockSodaInstaller() = default;

}  // namespace ash::babelorca
