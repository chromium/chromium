// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_

#include "build/build_config.h"

class PrefRegistrySimple;

namespace autofill_assistant::prefs {

// Boolean indicating whether the user has enabled Autofill Assistant.
// Prefs are not currently synced across devices.
// NOTE: This key needs to be kept in sync with the corresponding key in
// `AutofillAssistantPreferenceManager.java`.
extern const char kAutofillAssistantEnabled[];

// Boolean indicating whether the user has given consent for Autofill
// Assistant to communicate with Assistant servers.
// Prefs are not synced across devices.
// NOTE: This key needs to be kept in sync with the corresponding key in
// `AutofillAssistantPreferenceManager.java`.
extern const char kAutofillAssistantConsent[];

// Boolean indicating whether trigger scripts are enabled. `true` by default.
extern const char kAutofillAssistantTriggerScriptsEnabled[];

// Boolean indicating whether this is the first time a trigger script is run for
// a user. `true` by default.
extern const char kAutofillAssistantTriggerScriptsIsFirstTimeUser[];

// Below are keys of Android `SharedPreferences`. These are deprecated and
// currently being migrated to `PrefService`.
// Migrated to `prefs::kAutofillAssistantConsent`.
extern const char kDeprecatedAutofillAssistantConsent[];
// Migrated to `prefs::kAutofillAssistantEnabled`.
extern const char kDeprecatedAutofillAssistantEnabled[];
// Migrated to `prefs::kAutofillAssistantTriggerScriptsEnabled`.
extern const char kDeprecatedAutofillAssistantTriggerScriptsEnabled[];
// Migrated to `prefs::kAutofillAssistantTriggerScriptsIsFirstTimeUser`.
extern const char kDeprecatedAutofillAssistantTriggerScriptsIsFirstTimeUser[];

// Registers the Autofill Assistant profile prefs that are exposed to
// users of the Autofill Assistant component, i.e. whether Autofill Assistant
// is turned on and whether consent has been given.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace autofill_assistant::prefs

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_
