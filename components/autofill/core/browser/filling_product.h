// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

// Denotes the entity that is responsible for an Autofill behavior.
enum class FillingProduct {
  // kNone is used for the suggestions that do not identify any Autofill entity.
  kNone,
  kAddress,
  kCreditCard,
  kMerchantPromoCode,
  kIban,
  kAutocomplete,
  kPassword,
  kCompose,
  kPlusAddresses,
  kStandaloneCvc,
  kPredictionImprovements,
  kMaxValue = kPredictionImprovements
};

FillingProduct GetFillingProductFromSuggestionType(SuggestionType type);

FillingProduct GetFillingProductFromFieldTypeGroup(
    FieldTypeGroup field_type_group);

// Returns the filling product likely to be used for suggestions given
// `trigger_field_type` and `suggestion_trigger_source`. This might not be the
// definitive product used because for example the product could not yield any
// suggestion and we'd fallback to another product.
FillingProduct GetPreferredSuggestionFillingProduct(
    FieldType trigger_field_type,
    AutofillSuggestionTriggerSource suggestion_trigger_source);

// Returns a string representation of `filling_product`.
std::string FillingProductToString(FillingProduct filling_product);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_PRODUCT_H_
