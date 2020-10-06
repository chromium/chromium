// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace chromeos {
namespace quick_answers {
namespace prefs {

// A preference that indicates the user has allowed the Quick Answers to access
// the "selected content".
const char kQuickAnswersConsented[] = "settings.quick_answers.consented";

// A preference to keep track of the number of Quick Answers consent impression.
const char kQuickAnswersNoticeImpressionCount[] =
    "settings.quick_answers.consent.count";

// A preference to keep track of how long (in seconds) the Quick Answers consent
// has shown to the user.
const char kQuickAnswersNoticeImpressionDuration[] =
    "settings.quick_answers.consent.duration";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kQuickAnswersConsented, false);
  registry->RegisterIntegerPref(kQuickAnswersNoticeImpressionCount, 0);
  registry->RegisterIntegerPref(kQuickAnswersNoticeImpressionDuration, 0);
}

}  // namespace prefs
}  // namespace quick_answers
}  // namespace chromeos
