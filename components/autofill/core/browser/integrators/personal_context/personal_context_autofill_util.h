// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PERSONAL_CONTEXT_PERSONAL_CONTEXT_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PERSONAL_CONTEXT_PERSONAL_CONTEXT_AUTOFILL_UTIL_H_

#include "components/personal_context/core/personal_context_types.h"

class PrefService;

namespace personal_context {
class PersonalContextEnablementService;
}

namespace autofill {

// Returns true if the Personal Context setting should be shown in the
// Autofill settings page.
bool ShouldShowPersonalContextAutofillSetting(
    personal_context::PersonalContextEnablementService* enablement_service);

// Called when the Personal Context setting is flipped to "on" in the
// Autofill settings page. Updates notice-related preferences.
void PersonalContextInAutofillSettingFlippedOn(PrefService* pref_service);

// Returns true if either Autofill Ambient Autofill or Autofill AtMemory is
// enabled.
bool AreAutofillPersonalContextFeaturesSupported();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PERSONAL_CONTEXT_PERSONAL_CONTEXT_AUTOFILL_UTIL_H_
