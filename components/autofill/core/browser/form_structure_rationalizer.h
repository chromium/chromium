// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_RATIONALIZER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_RATIONALIZER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/form_field_data.h"
#include "url/origin.h"

namespace autofill {

class LogManager;

// Rationalization is the process of taking a parsed form structure and
// changing the types of fields based on their context based on assumptions
// about how forms should be structured.
class FormStructureRationalizer {
 public:
  // `fields` must outlive the FormStructureRationalizer and must not be null.
  // The rationalizer only modifies elements of `fields`, not the vector itself.
  explicit FormStructureRationalizer(
      std::vector<std::unique_ptr<AutofillField>>* fields);
  ~FormStructureRationalizer();
  FormStructureRationalizer(const FormStructureRationalizer&) = delete;
  FormStructureRationalizer& operator=(const FormStructureRationalizer&) =
      delete;

  // Rationalizes autocomplete attributes like turning a generic
  // autocomplete="cc-exp-year" into a 2 digit or 4 digit year if there are
  // hints like max-length=4.
  void RationalizeAutocompleteAttributes(LogManager* log_manager);

  // Sets the types of all contenteditables to UNKNOWN_TYPE in order to disable
  // autofilling of and importing from contenteditables.
  //
  // Usually, a contenteditable's type is UNKNOWN_TYPE anyway. Let's take a look
  // how a contenteditable may be assigned another type:
  // - Autocomplete: The contenteditable may have an autocomplete attribute.
  // - Heuristics: While the contenteditable is extracted as a separate form
  //   with only a single field by AutofillAgent, it may be flattened into a
  //   larger form (if the contenteditable lives in an iframe and the user
  //   interacted with it) that qualifies for heuristic type detection
  //   (kMinRequiredFieldsForHeuristics) by AutofillDriverRouter. However,
  //   currently no parsing rule matches FormControlType::kContentEditable, so
  //   the heuristic type is UNKNOWN_TYPE.
  // - Crowdsourcing: The focus-change and form-submission events that trigger
  //   crowdsourcing are not detected for contenteditables. But the
  //   contenteditable may be flattened into a form (see the previous bullet
  //   point) for which these events are triggered. Thus, the contenteditable
  //   could have a non-UNKNOWN_TYPE server type.
  //
  // AutofillContextMenuManager and AutocompleteHistoryManager ignore
  // contenteditables in their own code.
  void RationalizeContentEditables(LogManager* log_manager);

  // Tunes the fields with identical predictions.
  // The `form_signature` is needed for logging.
  void RationalizeRepeatedFields(FormSignature form_signature,
                                 AutofillMetrics::FormInteractionsUkmLogger*,
                                 LogManager* log_manager);

  // A helper function to review the predictions and do appropriate adjustments
  // when it considers necessary.
  void RationalizeFieldTypePredictions(const url::Origin& main_origin,
                                       const GeoIpCountryCode& client_country,
                                       const LanguageCode& language_code,
                                       LogManager* log_manager);

  // Ensures that only a single phone number (which can be split across multiple
  // fields) is autofilled in the `section`. If the section contains multiple
  // phone numbers, `set_only_fill_when_focused(true)` is set for the remaining
  // fields.
  // Contrary to the other rationalization logic of this class, this one happens
  // at filling time.
  void RationalizePhoneNumbersInSection(const Section& section);

 private:
  friend class FormStructureTestApi;

  // This class wraps a vector of vectors of field indices. The indices of a
  // vector belong to the same group.
  class SectionedFieldsIndexes;

  // Fine-tunes the credit cards related predictions. For example: lone credit
  // card fields in an otherwise non-credit-card related form is unlikely to be
  // correct, the function will override that prediction.
  void RationalizeCreditCardFieldPredictions(LogManager* log_manager);

  // Eradicates the type of credit card number and CVC fields on the main
  // frame's origin if there are also fields of this type in a child frame.
  // See crbug.com/1450502 for details.
  void RationalizeMultiOriginCreditCardFields(const url::Origin& main_origin,
                                              LogManager* log_manager);

  // Sets the offsets of adjacent credit card number fields. For example:
  // four adjacent card fields with `FormFieldData::max_length == 4` should
  // likely be filled with the the first, second, third, and fourth,
  // respectively, block of four digits.
  void RationalizeCreditCardNumberOffsets(LogManager* log_manager);

  // Rewrites sequences of (street address, address_line2) into (address_line1,
  // address_line2) as server predictions sometimes introduce wrong street
  // address predictions.
  void RationalizeStreetAddressAndAddressLine(LogManager* log_manager);

  // Rewrites sequences of (home_between_street,
  // home_between_street_1) or (home_between_street, home_between_street_2) to
  // (home_between_street_1, home_between_street_2) as these fields can be
  // wrongly classified by the heuristics.
  void RationalizeBetweenStreetFields(LogManager* log_manager);

  // Depending on the existence of a preceding PHONE_HOME_COUNTRY_CODE field,
  // a phone number's city code and city-and-number representation needs to be
  // prefixed with a trunk prefix. Autofill treats trunk prefixes as separate
  // types and distinguishes e.g. between PHONE_HOME_CITY_CODE and
  // PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX.
  // This function rationalizes types of city code and city-and-number fields
  // accordingly.
  void RationalizePhoneNumberTrunkTypes(LogManager* log_manager);

  // The rationalization is based on the visible fields, but should be applied
  // to the hidden select fields. This is because hidden 'select' fields are
  // also autofilled to take care of the synthetic fields.
  void ApplyRationalizationsToHiddenSelects(
      size_t field_index,
      FieldType new_type,
      FormSignature form_signature,
      AutofillMetrics::FormInteractionsUkmLogger*);

  // Returns true if we can replace server predictions with the heuristics one.
  bool HeuristicsPredictionsAreApplicable(size_t upper_index,
                                          size_t lower_index,
                                          FieldType first_type,
                                          FieldType second_type);

  // Applies upper type to upper field, and lower type to lower field, and
  // applies the rationalization also to hidden select fields if necessary.
  void ApplyRationalizationsToFields(
      size_t upper_index,
      size_t lower_index,
      FieldType upper_type,
      FieldType lower_type,
      FormSignature form_signature,
      AutofillMetrics::FormInteractionsUkmLogger*);

  // Returns true if the fields_[index] server type should be rationalized to
  // ADDRESS_HOME_COUNTRY.
  bool FieldShouldBeRationalizedToCountry(size_t index);

  // Set fields_[|field_index|] to |new_type| and log this change.
  void ApplyRationalizationsToFieldAndLog(
      size_t field_index,
      FieldType new_type,
      FormSignature form_signature,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger);

  // Two or three fields predicted as the whole address should be address lines
  // 1, 2 and 3 instead.
  void RationalizeAddressLineFields(
      SectionedFieldsIndexes* sections_of_address_indexes,
      FormSignature form_signature,
      AutofillMetrics::FormInteractionsUkmLogger*,
      LogManager* log_manager);

  // Rationalize state and country interdependently.
  void RationalizeAddressStateCountry(
      SectionedFieldsIndexes* sections_of_state_indexes,
      SectionedFieldsIndexes* sections_of_country_indexes,
      FormSignature form_signature,
      AutofillMetrics::FormInteractionsUkmLogger*,
      LogManager* log_manager);

  // Filters out fields that don't meet the relationship ruleset for their type
  // defined in |type_relationships_rules_|.
  void RationalizeTypeRelationships(LogManager* log_manager);

  // Executes a set of declarative rationalization rules. See
  // ApplyRationalizationEngineRules in
  // form_structure_rationalization_engine.cc.
  void RationalizeByRationalizationEngine(
      const GeoIpCountryCode& client_country,
      const LanguageCode& language_code,
      LogManager* log_manager);

  // A vector of all the input fields in the form. The reference is const but
  // the fields are mutable by design.
  const raw_ref<const std::vector<std::unique_ptr<AutofillField>>> fields_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_RATIONALIZER_H_
