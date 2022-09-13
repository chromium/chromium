// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_

#include "build/build_config.h"

namespace autofill_assistant::prefs {

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/1359957): Migrate Android prefs to profile prefs and use these
// keys. Once that is done, also consider syncing.

// Boolean indicating whether the user has enabled Autofill Assistant.
// Prefs are not currently synced across devices.
extern const char kAutofillAssistantEnabled[];

// Boolean indicating whether the user has given consent for Autofill
// Assistant to communicate with Assistant servers.
// Prefs are not synced across devices.
extern const char kAutofillAssistantConsent[];
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace autofill_assistant::prefs

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_PREFS_H_
