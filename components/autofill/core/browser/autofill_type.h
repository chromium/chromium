// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TYPE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TYPE_H_

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_types.h"

namespace autofill {

class AutofillField;

// Represents which types of data an AutofillField may accept. These types are
// encoded either as a set of FieldTypes.
//
// AutofillTypes are subject to constraints that govern which FieldTypes may
// occur together. See TestConstraints() for details.
//
// For example, every AutofillType must hold at most one address-related
// FieldType (e.g., it must not hold ADDRESS_HOME_LINE1 and ADDRESS_HOME_LINE2
// at once), which can be retrieved using GetAddressType().
//
// TODO(crbug.com/436013479): Remove the hack that represents country codes.
// TODO(crbug.com/432645177): Move ServerPredictions to AutofillField?
class AutofillType {
 public:
  // `TestConstraints(field_types)` must be true.
  //
  // `is_country_code` is a hack to work around the fact that FieldType does not
  // distinguish between country names and country codes. If `is_country_code`
  // is true and `field_types.contains(ADDRESS_HOME_COUNTRY)`, it indicates
  // that the ADDRESS_HOME_COUNTRY is a country code, not a country name.
  //
  // TODO(crbug.com/436013479): Remove `is_country_code`.
  explicit AutofillType(FieldTypeSet field_types, bool is_country_code);
  explicit AutofillType(FieldTypeSet field_types);
  explicit AutofillType(FieldType field_type, bool is_country_code);
  explicit AutofillType(FieldType field_type);
  AutofillType(const AutofillType& autofill_type) = default;
  AutofillType& operator=(const AutofillType& autofill_type) = default;
  ~AutofillType() = default;

  // Checks that the given FieldTypeSet satisfies the AutofillType constraints.
  //
  // Each of these constraints specifies a set of FieldTypes, and `s` must
  // contain at most one of these FieldTypes. For each of these constraints,
  // there is a getter that returns the unique type or UNKNOWN_TYPE.
  //
  // `AutofillType(s)` is admissible iff `TestConstraints(s)` is true.
  static bool TestConstraints(const FieldTypeSet& s);

  // Returns the FieldTypes held by this AutofillType.
  FieldTypeSet GetTypes() const;

  // Indicates that the `ADDRESS_HOME_COUNTRY` in GetTypes() represents country
  // code. If GetTypes() does not contain `ADDRESS_HOME_COUNTRY`, it is false.
  // TODO(crbug.com/436013479): Remove this hack.
  bool is_country_code() const { return is_country_code_; }

  // Returns the FieldTypeGroups of the types in GetTypes().
  //
  // Beware that every FieldType is mapped to at most one FieldTypeGroup by
  // GroupTypeOfFieldType()
  //
  // For example, NAME_FIRST is both an address-related FieldType and an
  // Autofill AI FieldType, but GetGroups() does not reflect that:
  // For `t = AutofillType(NAME_FIRST)`, it is true that
  //   `has_autofill_ai_type && !has_autofill_ai_group`
  // where
  //   `bool has_autofill_ai_type = !t.GetAutofillAiTypes().empty()`
  //   `bool has_autofill_ai_group = t.GetGroups().contains(kAutofillAi)`
  //
  // Similarly, EMAIL_ADDRESS is simultaneously an address-related FieldType and
  // a loyalty-card FieldType, but GetGroups() does not reflect that:
  // For `t = AutofillType(EMAIL_ADDRESS)`, it is true that
  //   `has_loyalty_type && !has_loyalty_group`
  // where
  //   `bool has_loyalty_type = !t.GetLoyaltyCardType().empty()`
  //   `bool has_loyalty_group = t.GetGroups().contains(kLoyaltyCard)`
  FieldTypeGroupSet GetGroups() const;

  // Returns the FormTypes of the groups in GetGroups().
  //
  // Beware that every FieldType is mapped to at most one FormType by
  // FieldTypeGroupToFormType().
  //
  // For example, EMAIL_ADDRESS is a loyalty card type but the FormType does not
  // reflect that:
  // For `t = AutofillType(EMAIL_ADDRESS)`, the following is both true:
  //   `t.GetLoyaltyCardType() == EMAIL_ADDRESS`
  //   `!t.GetFormTypes().contains(kLoyaltyCardForm)`
  //
  // And for some FieldTypes there is no FormType at all.
  // For `t = AutofillType(PASSPORT_NUMBER)`, the following is both true:
  //   `GetAutofillAiTypes() == {PASSPORT_NUMBER}`
  //   `GetFormTypes().empty()`
  DenseSet<FormType> GetFormTypes() const;

  // The AutofillType constraints guarantee that AutofillType contains at most
  // one FieldType of certain kinds. For example, an AutofillType may hold at
  // most one address-related FieldType.
  //
  // If this AutofillType holds none of those FieldTypes, returns UNKNOWN_TYPE.
  FieldType GetAddressType() const;
  FieldType GetAutofillAiType(EntityType entity) const;
  FieldType GetCreditCardType() const;
  FieldType GetIdentityCredentialType() const;
  FieldType GetLoyaltyCardType() const;
  FieldType GetPasswordManagerType() const;

  // GetAutofillAiTypes() is the union of GetAutofillAiType() over all
  // EntityTypes. That is, it includes all FieldTypes supported by Autofill AI,
  // including the dynamically assigned types (name types).
  //
  // GetStaticAutofillAiTypes() is like GetAutofillAiTypes() except that it
  // excludes the dynamically assigned types (name types).
  FieldTypeSet GetAutofillAiTypes() const;
  FieldTypeSet GetStaticAutofillAiTypes() const;

  std::string ToString() const;

 private:
  FieldTypeSet types_;
  bool is_country_code_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TYPE_H_
