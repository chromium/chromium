// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/preference_manager.h"

#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill_assistant {

PreferenceManager::PreferenceManager(PrefService* pref_service)
    : pref_service_(pref_service) {}

PreferenceManager::~PreferenceManager() = default;

bool PreferenceManager::GetIsFirstTimeTriggerScriptUser() const {
  return pref_service_->GetBoolean(
      prefs::kAutofillAssistantTriggerScriptsIsFirstTimeUser);
}

void PreferenceManager::SetIsFirstTimeTriggerScriptUser(bool first_time_user) {
  pref_service_->SetBoolean(
      prefs::kAutofillAssistantTriggerScriptsIsFirstTimeUser, first_time_user);
}

}  // namespace autofill_assistant
