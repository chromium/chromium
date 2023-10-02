// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_bubble_experiment.h"

#include <string>

#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_bubble_experiment {

int GetSmartBubbleDismissalThreshold() {
  return 3;
}

bool ShouldShowAutoSignInPromptFirstRunExperience(PrefService* prefs) {
  return !prefs->GetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown);
}

void RecordAutoSignInPromptFirstRunExperienceWasShown(PrefService* prefs) {
  prefs->SetBoolean(
      password_manager::prefs::kWasAutoSignInFirstRunExperienceShown, true);
}

}  // namespace password_bubble_experiment
