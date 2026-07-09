// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PERSONAL_CONTEXT_PERSONAL_CONTEXT_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PERSONAL_CONTEXT_PERSONAL_CONTEXT_AUTOFILL_UTIL_H_

#include "build/build_config.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/personal_context/core/personal_context_types.h"

namespace personal_context {
class PersonalContextEligibilityService;
}

class GoogleGroupsManager;
class PrefService;

namespace subscription_eligibility {
class SubscriptionEligibilityService;
}

namespace signin {
class IdentityManager;
}

namespace syncer {
class SyncService;
}

namespace autofill {

class AutofillClient;
class EntityDataManager;

// Returns true if the Personal Context setting should be shown in the
// Autofill settings page.
bool ShouldShowPersonalContextAutofillSetting(
    const AutofillClient& client,
    personal_context::PersonalContextEligibilityService* eligibility_service);

bool ShouldShowPersonalContextAutofillSetting(
#if !BUILDFLAG(IS_FUCHSIA)
    const GoogleGroupsManager* google_groups_manager,
#endif
    const PrefService* prefs,
    const EntityDataManager* edm,
    const signin::IdentityManager* identity_manager,
    const syncer::SyncService* sync_service,
    bool is_wallet_public_pass_storage_enabled,
    bool is_off_the_record,
    const GeoIpCountryCode& country_code,
    personal_context::PersonalContextEligibilityService* eligibility_service,
    const subscription_eligibility::SubscriptionEligibilityService*
        subscription_service);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_PERSONAL_CONTEXT_PERSONAL_CONTEXT_AUTOFILL_UTIL_H_
