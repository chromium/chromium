// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/public/prefs.h"

#include "build/build_config.h"
#include "components/prefs/pref_registry_simple.h"

namespace autofill_assistant::prefs {

#if !BUILDFLAG(IS_ANDROID)
const char kAutofillAssistantEnabled[] = "autofill_assistant.enabled";

const char kAutofillAssistantConsent[] = "autofill_assistant.consent";
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace autofill_assistant::prefs
