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
#include "components/autofill/core/browser/filling/filling_product.h"
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

// Payments field swapping (`kAutofillPaymentsFieldSwapping`) lets the user
// replace a credit card that Autofill already filled into a form by selecting
// a different card from the Autofill popup. Without swapping, Autofill never
// overwrites fields it filled before, so the new card would only be filled into
// the remaining empty fields and the form would mix data from two cards.

// Returns whether `field` holds a credit card value that Autofill filled and
// that a later credit card fill may therefore replace. This is the case if
// payments field swapping is enabled, `field` is a credit card field other than
// a standalone CVC field, and the current, non-empty value of `field` was last
// set by Autofill from a credit card. Values that the user typed or that
// Autofill filled from another source (e.g. an address) are never swapped.
bool AllowPaymentsSwappingOnField(const AutofillField& field);

// Returns whether a fill of `filling_product` triggered from `trigger_field`
// may replace the values of fields for which `AllowPaymentsSwappingOnField()`
// holds. This is the case if payments field swapping is enabled and the fill is
// a credit card fill (not a refill) triggered from a credit card field other
// than a standalone CVC field. A standalone CVC field only asks for the CVC of
// a card the merchant already has on file, so selecting a card there must not
// change other card fields.
bool AllowPaymentsSwappingFill(const AutofillField& trigger_field,
                               bool is_refill,
                               FillingProduct filling_product);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PAYMENTS_FIELD_FILLING_PAYMENTS_UTIL_H_
