// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_churned_users_manager.h"

#include <algorithm>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notimplemented.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"

namespace autofill::payments {

PaymentsChurnedUsersManager::PaymentsChurnedUsersManager(
    AutofillClient* autofill_client)
    : client_(CHECK_DEREF(autofill_client)) {
  autofill_managers_observation_.Observe(
      autofill_client, ScopedAutofillManagersObservation::InitializationPolicy::
                           kObservePreexistingManagers);
}

PaymentsChurnedUsersManager::~PaymentsChurnedUsersManager() = default;

void PaymentsChurnedUsersManager::OnFieldTypesDetermined(
    AutofillManager& manager,
    FormGlobalId form,
    AutofillManager::Observer::FieldTypeSource source,
    bool small_forms_were_parsed) {
  const FormStructure* form_structure = manager.FindCachedFormById(form);
  if (!form_structure) {
    return;
  }

  bool is_visible_credit_card_form =
      std::ranges::any_of(form_structure->fields(), [](const auto& field) {
        return field->Type().GetGroups().contains(
                   FieldTypeGroup::kCreditCard) &&
               field->is_visible();
      });

  if (!is_visible_credit_card_form) {
    return;
  }

  PrefService* prefs = client_->GetPrefs();
  if (!prefs) {
    return;
  }

  const PrefService::Preference* pref =
      prefs->FindPreference(prefs::kAutofillCreditCardEnabled);
  if (pref && pref->IsUserControlled() && !pref->GetValue()->GetBool() &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableResurrectingPaymentsUsers)) {
    if (payments::PaymentsAutofillClient* payments_client =
            client_->GetPaymentsAutofillClient()) {
      payments_client->ShowPaymentsChurnedUsersUI(
          base::BindOnce(&PaymentsChurnedUsersManager::OnBubbleAccepted,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&PaymentsChurnedUsersManager::OnBubbleCancelled,
                         weak_factory_.GetWeakPtr()));
    }
  }
}

void PaymentsChurnedUsersManager::OnBubbleAccepted() {
  if (PrefService* prefs = client_->GetPrefs()) {
    prefs->SetBoolean(prefs::kAutofillCreditCardEnabled, true);
  }
}

void PaymentsChurnedUsersManager::OnBubbleCancelled() {
  // TODO(crbug.com/524740910): Implement cancel callback for the payments
  // churned users UI.
  NOTIMPLEMENTED();
}

}  // namespace autofill::payments
