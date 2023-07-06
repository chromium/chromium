// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace quick_answers::prefs {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kQuickAnswersNoticed, false);
  registry->RegisterBooleanPref(kQuickAnswersEnabled, false);
  registry->RegisterIntegerPref(kQuickAnswersConsentStatus,
                                ConsentStatus::kUnknown);
  registry->RegisterBooleanPref(kQuickAnswersDefinitionEnabled, true);
  registry->RegisterBooleanPref(kQuickAnswersTranslationEnabled, true);
  registry->RegisterBooleanPref(kQuickAnswersUnitConversionEnabled, true);
  registry->RegisterIntegerPref(kQuickAnswersNoticeImpressionCount, 0);
  registry->RegisterIntegerPref(kQuickAnswersNoticeImpressionDuration, 0);
}

}  // namespace quick_answers::prefs
