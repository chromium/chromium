// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/field_filler.h"

#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_filling_payments_util.h"

namespace autofill {

FieldFiller::FieldFiller(const std::string& app_locale,
                         AddressNormalizer* address_normalizer)
    : app_locale_(app_locale), address_normalizer_(address_normalizer) {}

FieldFiller::~FieldFiller() = default;

std::u16string FieldFiller::GetValueForFilling(
    const AutofillField& field,
    absl::variant<const AutofillProfile*, const CreditCard*>
        profile_or_credit_card,
    const FormFieldData& field_data,
    const std::u16string& cvc,
    mojom::ActionPersistence action_persistence,
    std::string* failure_to_fill) {
  if (absl::holds_alternative<const CreditCard*>(profile_or_credit_card)) {
    const CreditCard* credit_card =
        absl::get<const CreditCard*>(profile_or_credit_card);
    return GetValueForCreditCard(*credit_card, cvc, app_locale_,
                                 action_persistence, field, failure_to_fill)
        .value_or(u"");
  }
  // Grab AutofillProfile data.
  CHECK(
      absl::holds_alternative<const AutofillProfile*>(profile_or_credit_card));
  const AutofillProfile* profile =
      absl::get<const AutofillProfile*>(profile_or_credit_card);
  return GetValueForProfile(*profile, app_locale_, field.Type(), field_data,
                            address_normalizer_, failure_to_fill)
      .value_or(u"");
}

}  // namespace autofill
