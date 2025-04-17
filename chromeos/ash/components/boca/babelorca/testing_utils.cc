// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/testing_utils.h"

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

}  // namespace ash::babelorca
