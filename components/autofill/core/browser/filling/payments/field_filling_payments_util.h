// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PAYMENTS_FIELD_FILLING_PAYMENTS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PAYMENTS_FIELD_FILLING_PAYMENTS_UTIL_H_

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

class AutofillField;
class CreditCard;

// Returns the appropriate `credit_card` value based on `field_type` to fill
// into `field`, and an empty string if no value could be found for the given
// `field`.
FillingValueAndType GetFillingValueAndTypeForCreditCard(
    const CreditCard& credit_card,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    bool is_cvc_filling_supported,
    std::string* failure_to_fill = nullptr);

// Determines whether a CC filling operation triggered on `trigger_field` will
// fill any of `fields` with a CCN or CVC.
bool WillFillCreditCardNumberOrCvc(
    base::span<const std::unique_ptr<AutofillField>> fields,
    const AutofillField& trigger_field,
    AutofillTriggerSource trigger_source,
    bool card_has_cvc,
    AutocompleteUnrecognizedBehavior ac_unrecognized_behavior);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PAYMENTS_FIELD_FILLING_PAYMENTS_UTIL_H_
