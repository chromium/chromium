// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_

#include <string>
#include <vector>

#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

class AutofillField;
class CreditCard;
class FormFieldData;

// Returns the appropriate `credit_card` value based on `field_type` to fill
// into `field`, and an empty string if no value could be found for the given
// `field`.
std::u16string GetFillingValueForCreditCard(
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    std::string* failure_to_fill = nullptr);

// Determines whether the `autofill_fields` or `trigger_autofill_field` is
// either a credit card number or a CVC field. If either, is the
// `trigger_autofill_field`, then return true otherwise check if the field is
// empty and haven't been autofilled before, to return true.
// `fields` represents the fields obtained from the renderer. They are the most
// up to date version of the form and can be different from the
// `autofill_fields`. `fields` are used to check if the cached field is still
// present in the form on the renderer side. When `card_has_cvc` is false,
// ignore the CVC field.
// TODO(crbug.com/40227496): Remove FormFieldData parameter.
bool WillFillCreditCardNumberOrCvc(
    base::span<const FormFieldData> fields,
    base::span<const std::unique_ptr<AutofillField>> autofill_fields,
    const AutofillField& trigger_autofill_field,
    bool card_has_cvc);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_
