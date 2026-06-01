// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/personal_context/personal_context_autofill_util.h"

#include "components/personal_context/core/personal_context_enablement_service.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/personal_context/core/personal_context_types.h"
#include "components/prefs/pref_service.h"

namespace autofill {

bool ShouldShowPersonalContextAutofillSetting(
    personal_context::PersonalContextEnablementService* enablement_service) {
  if (!enablement_service) {
    return false;
  }

  using enum personal_context::PersonalContextEnablementState;
  switch (enablement_service->GetEnablementState()) {
    case kDisabledNotEligible:
    case kDisabledNeedsOptIn:
      return false;
    case kDisabledShouldShowNotice:
    case kEnabledShouldShowNotice:
    case kDisabledViaPersonalIntelligenceInAutofillToggle:
    case kEnabled:
      return true;
  }
}

void PersonalContextInAutofillSettingFlippedOn(PrefService* pref_service) {
  if (pref_service) {
    pref_service->SetBoolean(
        personal_context::prefs::kPersonalContextInAutofillNoticeShouldBeShown,
        false);
    pref_service->SetBoolean(
        personal_context::prefs::kPersonalContextInAutofillNoticeHasBeenShown,
        true);
  }
}

}  // namespace autofill
