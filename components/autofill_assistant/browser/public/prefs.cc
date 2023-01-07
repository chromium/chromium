// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace autofill_assistant::prefs {

const char kAutofillAssistantConsent[] = "autofill_assistant.consent";
const char kAutofillAssistantEnabled[] = "autofill_assistant.enabled";
const char kAutofillAssistantTriggerScriptsEnabled[] =
    "autofill_assistant.trigger_scripts.enabled";
const char kAutofillAssistantTriggerScriptsIsFirstTimeUser[] =
    "autofill_assistant.trigger_scripts.is_first_time_user";

const char kDeprecatedAutofillAssistantConsent[] = "autofill_assistant_switch";
const char kDeprecatedAutofillAssistantEnabled[] =
    "AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED";
const char kDeprecatedAutofillAssistantTriggerScriptsEnabled[] =
    "Chrome.AutofillAssistant.ProactiveHelp";
const char kDeprecatedAutofillAssistantTriggerScriptsIsFirstTimeUser[] =
    "Chrome.AutofillAssistant.LiteScriptFirstTimeUser";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAutofillAssistantEnabled, true);
  registry->RegisterBooleanPref(prefs::kAutofillAssistantConsent, false);
  registry->RegisterBooleanPref(prefs::kAutofillAssistantTriggerScriptsEnabled,
                                true);
  registry->RegisterBooleanPref(
      prefs::kAutofillAssistantTriggerScriptsIsFirstTimeUser, true);
}

}  // namespace autofill_assistant::prefs
