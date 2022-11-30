// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/autofillable_data.h"

namespace autofill {

AutofillableData::AutofillableData(const AutofillProfile* profile)
    : base(profile) {}

AutofillableData::AutofillableData(const CreditCard* card, std::u16string cvc)
    : base(CreditCardWithCvc{card, std::move(cvc)}) {}

bool AutofillableData::is_profile() const {
  return absl::holds_alternative<const AutofillProfile*>(*this);
}

bool AutofillableData::is_credit_card() const {
  return absl::holds_alternative<CreditCardWithCvc>(*this);
}

const AutofillProfile& AutofillableData::profile() const {
  DCHECK(is_profile());
  return *absl::get<const AutofillProfile*>(*this);
}

const CreditCard& AutofillableData::credit_card() const {
  DCHECK(is_credit_card());
  return *absl::get<CreditCardWithCvc>(*this).credit_card;
}

const std::u16string& AutofillableData::cvc() const {
  DCHECK(is_credit_card());
  return absl::get<CreditCardWithCvc>(*this).cvc;
}

}  // namespace autofill
