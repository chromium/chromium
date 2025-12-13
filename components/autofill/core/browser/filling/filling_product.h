// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FILLING_PRODUCT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FILLING_PRODUCT_H_

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill

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
  kAutofillAi,
  kLoyaltyCard,
  kIdentityCredential,
  kDataList,
  kOneTimePassword,
  kPasskey,
  kMaxValue = kPasskey
};

FillingProduct GetFillingProductFromSuggestionType(SuggestionType type);

FillingProduct GetFillingProductFromFieldTypeGroup(
    FieldTypeGroup field_type_group);

// Returns a string representation of `filling_product`.
std::string FillingProductToString(FillingProduct filling_product);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FILLING_PRODUCT_H_
