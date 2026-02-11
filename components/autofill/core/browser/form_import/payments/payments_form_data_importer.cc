// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_import/payments/payments_form_data_importer.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill::payments {

PaymentsFormDataImporter::PaymentsFormDataImporter(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

PaymentsFormDataImporter::~PaymentsFormDataImporter() = default;

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

}  // namespace autofill::payments
