// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/addresses/account_name_email_strike_manager.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/suggestions/addresses/address_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill {

AccountNameEmailStrikeManager::AccountNameEmailStrikeManager(
    AutofillManager& autofill_manager)
    : client_(autofill_manager.client()) {
  autofill_manager_observation_.Observe(&autofill_manager);
}

AccountNameEmailStrikeManager::~AccountNameEmailStrikeManager() {
  PrefService* pref_service = client_->GetPrefs();
  if (!pref_service || !was_name_email_profile_suggestion_shown_) {
    return;
  }
  if (pref_service->GetBoolean(prefs::kAutofillWasNameAndEmailProfileUsed)) {
    return;
  }

  if (was_name_email_profile_filled_) {
    pref_service->SetBoolean(prefs::kAutofillWasNameAndEmailProfileUsed, true);
    return;
  }

  int not_selected_count = pref_service->GetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter);
  pref_service->SetInteger(
      prefs::kAutofillNameAndEmailProfileNotSelectedCounter,
      not_selected_count + 1);
  // Record metric if the implicit removal will happen.
  // It is intentionally recorded here, since recording in
  // `AccountNameEmailStore::OnCounterPrefUpdated()` or
  // `AccountNameEmailStore::ApplyChange()` would lead to overrecording because
  // those two methods are also called as the result of a profile being removed
  // on a different device using the same account.
  if (not_selected_count ==
      features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get()) {
    base::UmaHistogramBoolean(
        "Autofill.ProfileDeleted.ImplicitAccountNameEmail", true);
  } else if (not_selected_count <
             features::kAutofillNameAndEmailProfileNotSelectedThreshold.Get()) {
    base::UmaHistogramBoolean(
        "Autofill.ProfileDeleted.ImplicitAccountNameEmail", false);
  }
}

void AccountNameEmailStrikeManager::OnSuggestionsShown(
    autofill::AutofillManager& manager,
    base::span<const autofill::Suggestion> suggestions) {
  was_name_email_profile_suggestion_shown_ =
      was_name_email_profile_suggestion_shown_ ||
      ContainsProfileSuggestionWithRecordType(
          suggestions, client_->GetPersonalDataManager().address_data_manager(),
          AutofillProfile::RecordType::kAccountNameEmail);
}

void AccountNameEmailStrikeManager::OnFillOrPreviewForm(
    AutofillManager& manager,
    FormGlobalId form_id,
    mojom::ActionPersistence action_persistence,
    const base::flat_set<FieldGlobalId>& filled_field_ids,
    const FillingPayload& filling_payload) {
  if (action_persistence != mojom::ActionPersistence::kFill) {
    return;
  }

  const AutofillProfile* const* profile_payload_ptr =
      std::get_if<const AutofillProfile*>(&filling_payload);
  if (!profile_payload_ptr) {
    return;
  }
  const AutofillProfile* profile_payload = *profile_payload_ptr;
  CHECK(profile_payload);

  if (profile_payload->record_type() ==
      AutofillProfile::RecordType::kAccountNameEmail) {
    was_name_email_profile_filled_ = true;
  }
}

}  // namespace autofill
