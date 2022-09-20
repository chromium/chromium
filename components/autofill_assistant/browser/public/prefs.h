// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_

#include "build/build_config.h"

class PrefRegistrySimple;

namespace autofill_assistant::prefs {

// TODO(crbug.com/1359957): Migrate from Android to Desktop.
// Boolean indicating whether the user has enabled Autofill Assistant.
// Prefs are not currently synced across devices.
extern const char kAutofillAssistantEnabled[];

// TODO(crbug.com/1359957): Migrate from Android to Desktop.
// Boolean indicating whether the user has given consent for Autofill
// Assistant to communicate with Assistant servers.
// Prefs are not synced across devices.
extern const char kAutofillAssistantConsent[];

// Boolean indicating whether a user has seen a trigger script before or if they
// are first time users. `true` by default. Reset to default on clearing
// all-time browser history.
extern const char kAutofillAssistantTriggerScriptsIsFirstTimeUser[];

// Below are keys of Android `SharedPreferences`. These are deprecated and
// currently being migrated to `PrefService`.
// Migrated to `prefs::kAutofillAssistantTriggerScriptsIsFirstTimeUser`.
extern const char kDeprecatedAutofillAssistantTriggerScriptsIsFirstTimeUser[];

// Registers the Autofill Assistant profile prefs that are exposed to
// users of the Autofill Assistant component, i.e. whether Autofill Assistant
// is turned on and whether consent has been given.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace autofill_assistant::prefs

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_
