// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FILLING_PRODUCT_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FILLING_PRODUCT_H_

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.autofill

// These values are persisted in UMA logs. Entries should not be renumbered.
// Denotes the entity that is responsible for an Autofill behavior.
// LINT.IfChange(FillingProduct)
enum class FillingProduct {
  // kNone is used for the suggestions that do not identify any Autofill entity.
  kNone = 0,
  kAddress = 1,
  kCreditCard = 2,
  kMerchantPromoCode = 3,
  kIban = 4,
  kAutocomplete = 5,
  kPassword = 6,
  kCompose = 7,
  // DEPRECATED 8,
  kAutofillAi = 9,
  kLoyaltyCard = 10,
  kIdentityCredential = 11,
  kDataList = 12,
  kOneTimePassword = 13,
  kPasskey = 14,
  kAtMemory = 15,
  kMaxValue = kAtMemory
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillFillingProduct)

// Returns `raw_value` if it is a known FillingProduct constant.
constexpr std::optional<FillingProduct> ToSafeFillingProduct(
    std::underlying_type_t<FillingProduct> raw_value);

template <>
struct DenseSetTraits<FillingProduct>
    : EnumDenseSetTraits<FillingProduct,
                         FillingProduct::kNone,
                         FillingProduct::kMaxValue> {
  static constexpr bool is_valid(FillingProduct x) {
    return ToSafeFillingProduct(std::to_underlying(x)).has_value();
  }
};

using FillingProductSet = DenseSet<FillingProduct>;

FillingProduct GetFillingProductFromSuggestionType(SuggestionType type);

// Returns the `FillingProduct` responsible for generating suggestions with a
// certain `SuggestionDataSource`.
FillingProduct GetFillingProductFromSuggestionDataSource(
    SuggestionGenerator::SuggestionDataSource source);

FillingProduct GetFillingProductFromFieldTypeGroup(
    FieldTypeGroup field_type_group);

// Returns a string representation of `filling_product`.
std::string FillingProductToString(FillingProduct filling_product);

constexpr std::optional<FillingProduct> ToSafeFillingProduct(
    std::underlying_type_t<FillingProduct> raw_value) {
  auto is_invalid = [](std::underlying_type_t<FillingProduct> t) {
    return t < std::to_underlying(FillingProduct::kNone) ||
           t > std::to_underlying(FillingProduct::kMaxValue) ||
           // 8 was deprecated.
           t == 8;
  };
  if (is_invalid(raw_value)) {
    return std::nullopt;
  }
  return static_cast<FillingProduct>(raw_value);  // nocheck
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_FILLING_PRODUCT_H_
