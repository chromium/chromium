// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/personal_context/personal_context_autofill_util.h"

#include "components/autofill/core/browser/at_memory/at_memory_enablement_utils.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/personal_context/core/personal_context_eligibility_service.h"
#include "components/personal_context/core/personal_context_types.h"

namespace autofill {

bool ShouldShowPersonalContextAutofillSetting(
    const AutofillClient& client,
    personal_context::PersonalContextEligibilityService* eligibility_service) {
  return ShouldShowPersonalContextAutofillSetting(
#if !BUILDFLAG(IS_FUCHSIA)
      client.GetGoogleGroupsManager(),
#endif
      client.GetPrefs(), client.GetEntityDataManager(),
      client.GetIdentityManager(), client.GetSyncService(),
      client.IsWalletPublicPassStorageEnabled(), client.IsOffTheRecord(),
      client.GetVariationConfigCountryCode(), eligibility_service,
      client.GetSubscriptionEligibilityService());
}

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
        subscription_service) {
  if (!eligibility_service) {
    return false;
  }

  const bool ambient_autofill_enabled = MayPerformAutofillAiAction(
#if !BUILDFLAG(IS_FUCHSIA)
      google_groups_manager,
#endif
      prefs, edm, identity_manager, sync_service,
      is_wallet_public_pass_storage_enabled, is_off_the_record, country_code,
      subscription_service, eligibility_service->GetEligibilityState(),
      AutofillAiAction::kShowAmbientAutofillInSettings);

  const bool at_memory_enabled =
      MayPerformAtMemoryAction(AtMemoryAction::kShowAtMemoryInSettings,
                               eligibility_service, subscription_service, prefs,
#if !BUILDFLAG(IS_FUCHSIA)
                               google_groups_manager,
#else
                               /*google_groups_manager=*/nullptr,
#endif
                               /*decider=*/nullptr, /*url=*/std::nullopt);

  return ambient_autofill_enabled || at_memory_enabled;
}

}  // namespace autofill
