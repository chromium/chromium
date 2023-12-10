// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

class AutofillField;
class CreditCard;
struct FormFieldData;

// Returns the appropriate `credit_card` value based on `field_type` to fill
// into `field`, and nullopt if no value could be found for the given `field`.
std::optional<std::u16string> GetValueForCreditCard(
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    std::string* failure_to_fill = nullptr);

// Determines whether the `autofill_fields` contains a credit card number
// field, which is empty and hasn't been autofilled before. `form_fields`
// represents the fields obtained from the renderer. They are the most up to
// date version of the form and can be different from the
// `autofill_fields`. `form_fields` are used to check if the cached field
// is still present in the form on the renderer side.
// TODO(crbug.com/1331312): Remove FormFieldData parameter.
bool WillFillCreditCardNumber(
    base::span<const FormFieldData> form_fields,
    base::span<const std::unique_ptr<AutofillField>> autofill_fields,
    const AutofillField& triggered_autofill_field);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_
