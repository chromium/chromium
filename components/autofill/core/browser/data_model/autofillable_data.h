// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILLABLE_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILLABLE_DATA_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

struct CreditCardWithCvc {
  const CreditCard* credit_card;
  std::u16string cvc;
};

class AutofillableData
    : public absl::variant<const AutofillProfile*, CreditCardWithCvc> {
 public:
  explicit AutofillableData(const AutofillProfile* profile);
  explicit AutofillableData(const CreditCard* card, std::u16string cvc = u"");

  using absl::variant<const AutofillProfile*, CreditCardWithCvc>::variant;

  bool is_profile() const;
  bool is_credit_card() const;
  const AutofillProfile& profile() const;
  const CreditCard& credit_card() const;
  const std::u16string& cvc() const;

 private:
  using base = absl::variant<const AutofillProfile*, CreditCardWithCvc>;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILLABLE_DATA_H_
