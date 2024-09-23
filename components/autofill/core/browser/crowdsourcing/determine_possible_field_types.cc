// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/disambiguate_possible_field_types.h"
#include "components/autofill/core/browser/data_model/address.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"

namespace autofill {

namespace {

// Finds the first field in |form_structure| with |field.value|=|value|.
AutofillField* FindFirstFieldWithValue(const FormStructure& form_structure,
                                       const std::u16string& value) {
  for (const auto& field : form_structure) {
    std::u16string trimmed_value;
    base::TrimWhitespace(field->value_for_import(), base::TRIM_ALL,
                         &trimmed_value);
    if (trimmed_value == value) {
      return field.get();
    }
  }
  return nullptr;
}

// Heuristically identifies all possible credit card verification fields.
AutofillField* HeuristicallyFindCVCFieldForUpload(
    const FormStructure& form_structure) {
  // Stores a pointer to the explicitly found expiration year.
  bool found_explicit_expiration_year_field = false;

  // The first pass checks the existence of an explicitly marked field for the
  // credit card expiration year.
  for (const auto& field : form_structure) {
    const FieldTypeSet& type_set = field->possible_types();
    if (type_set.find(CREDIT_CARD_EXP_2_DIGIT_YEAR) != type_set.end() ||
        type_set.find(CREDIT_CARD_EXP_4_DIGIT_YEAR) != type_set.end()) {
      found_explicit_expiration_year_field = true;
      break;
    }
  }

  // Keeps track if a credit card number field was found.
  bool credit_card_number_found = false;

  // In the second pass, the CVC field is heuristically searched for.
  // A field is considered a CVC field, iff:
  // * it appears after the credit card number field;
  // * it has the |UNKNOWN_TYPE| prediction;
  // * it does not look like an expiration year or an expiration year was
  //   already found;
  // * it is filled with a 3-4 digit number;
  for (const auto& field : form_structure) {
    const FieldTypeSet& type_set = field->possible_types();

    // Checks if the field is of |CREDIT_CARD_NUMBER| type.
    if (type_set.find(CREDIT_CARD_NUMBER) != type_set.end()) {
      credit_card_number_found = true;
      continue;
    }
    // Skip the field if no credit card number was found yet.
    if (!credit_card_number_found) {
      continue;
    }

    // Don't consider fields that already have any prediction.
    if (type_set.find(UNKNOWN_TYPE) == type_set.end()) {
      continue;
    }
    // |UNKNOWN_TYPE| should come alone.
    DCHECK_EQ(1u, type_set.size());

    std::u16string trimmed_value;
    base::TrimWhitespace(field->value_for_import(), base::TRIM_ALL,
                         &trimmed_value);

    // Skip the field if it can be confused with a expiration year.
    if (!found_explicit_expiration_year_field &&
        IsPlausible4DigitExpirationYear(trimmed_value)) {
      continue;
    }

    // Skip the field if its value does not like a CVC value.
    if (!IsPlausibleCreditCardCVCNumber(trimmed_value)) {
      continue;
    }

    return field.get();
  }
  return nullptr;
}

// Iff the CVC of the credit card is known, find the first field with this
// value (also set |properties_mask| to |kKnownValue|). Otherwise, heuristically
// search for the CVC field if any.
AutofillField* GetBestPossibleCVCFieldForUpload(
    const FormStructure& form_structure,
    std::u16string last_unlocked_credit_card_cvc) {
  if (!last_unlocked_credit_card_cvc.empty()) {
    AutofillField* result =
        FindFirstFieldWithValue(form_structure, last_unlocked_credit_card_cvc);
    if (result) {
      result->set_properties_mask(FieldPropertiesFlags::kKnownValue);
    }
    return result;
  }

  return HeuristicallyFindCVCFieldForUpload(form_structure);
}

// Extracts the value from `field`. Then for each profile or credit card,
// identify any stored types that match the value. Runs additional heuristics
// for increased accuracy. Defaults to `{UNKNOWN_TYPE}` if no types could be
// found.
void FindAndSetPossibleFieldTypesForField(
    AutofillField& field,
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::string& app_locale) {
  std::u16string value = field.value_for_import();
  base::TrimWhitespace(value, base::TRIM_ALL, &value);

  if (!field.possible_types().empty() && value.empty()) {
    // This is a password field in a sign-in form. Skip checking its type
    // since |field->value| is not set.
    DCHECK_EQ(1u, field.possible_types().size());
    DCHECK_EQ(PASSWORD, *field.possible_types().begin());
    return;
  }
  FieldTypeSet matching_types;

  for (const AutofillProfile& profile : profiles) {
    profile.GetMatchingTypesWithProfileSources(value, app_locale,
                                               &matching_types, nullptr);
  }
  for (const CreditCard& card : credit_cards) {
    card.GetMatchingTypesWithProfileSources(value, app_locale, &matching_types,
                                            nullptr);
  }

  if (field.state_is_a_matching_type()) {
    matching_types.insert(ADDRESS_HOME_STATE);
  }
  if (matching_types.empty()) {
    matching_types.insert(UNKNOWN_TYPE);
  }
  field.set_possible_types(matching_types);
}

// For each `form` field, searches for the field value in profiles and credit
// cards and sets the field's possible types accordingly. Special heuristics are
// run for finding the CVC field.
void FindAndSetPossibleFieldTypes(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::u16string& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure& form) {
  for (size_t i = 0; i < form.field_count(); ++i) {
    FindAndSetPossibleFieldTypesForField(*form.field(i), profiles, credit_cards,
                                         app_locale);
  }

  // As CVCs are not stored, run special heuristics to detect CVC-like values.
  AutofillField* cvc_field =
      GetBestPossibleCVCFieldForUpload(form, last_unlocked_credit_card_cvc);
  if (cvc_field) {
    FieldTypeSet possible_types = cvc_field->possible_types();
    possible_types.erase(UNKNOWN_TYPE);
    possible_types.insert(CREDIT_CARD_VERIFICATION_CODE);
    cvc_field->set_possible_types(possible_types);
  }
}

}  // namespace

void DeterminePossibleFieldTypesForUpload(
    const std::vector<AutofillProfile>& profiles,
    const std::vector<CreditCard>& credit_cards,
    const std::u16string& last_unlocked_credit_card_cvc,
    const std::string& app_locale,
    FormStructure* form) {
  for (const std::unique_ptr<AutofillField>& field : *form) {
    // DeterminePossibleFieldTypesForUpload may be called multiple times. Reset
    // the values so that the first call does not affect later calls.
    field->set_possible_types({});
  }
  FindAndSetPossibleFieldTypes(
      profiles, credit_cards, last_unlocked_credit_card_cvc, app_locale, *form);
  DisambiguatePossibleFieldTypes(*form);
}

}  // namespace autofill
