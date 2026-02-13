// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"

#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill::payments {

PaymentsFormDataImporter::PaymentsFormDataImporter(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

PaymentsFormDataImporter::~PaymentsFormDataImporter() = default;

std::optional<Iban> PaymentsFormDataImporter::ExtractIban(
    const FormStructure& form) {
  Iban candidate_iban = ExtractIbanFromForm(form);
  if (candidate_iban.value().empty()) {
    return std::nullopt;
  }

  // Sets the `kAutofillHasSeenIban` pref to `true` indicating that the user has
  // submitted a form with an IBAN, which indicates that the user is familiar
  // with IBANs as a concept. We set the pref so that even if the user travels
  // to a country where IBAN functionality is not typically used, they will
  // still be able to save new IBANs from the settings page using this pref.
  payments_data_manager().SetAutofillHasSeenIban();

  return candidate_iban;
}

void PaymentsFormDataImporter::CacheFetchedVirtualCard(
    const std::u16string& last_four) {
  fetched_virtual_cards_.insert(last_four);
}

Iban PaymentsFormDataImporter::ExtractIbanFromForm(const FormStructure& form) {
  // Creates an IBAN candidate with `kUnknown` record type as it is currently
  // unknown if this IBAN already exists locally or on the server.
  Iban candidate_iban;
  for (const auto& field : form) {
    const std::u16string& value = field->value_for_import();
    if (!field->IsFieldFillable() || value.empty()) {
      continue;
    }
    if (field->Type().GetTypes().contains(IBAN_VALUE) && Iban::IsValid(value)) {
      candidate_iban.set_value(value);
      break;
    }
  }
  return candidate_iban;
}

PaymentsDataManager& PaymentsFormDataImporter::payments_data_manager() {
  return client_->GetPersonalDataManager().payments_data_manager();
}

}  // namespace autofill::payments
