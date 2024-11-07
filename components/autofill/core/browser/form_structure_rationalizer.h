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

  // A helper function to review the predictions and do appropriate adjustments
  // when it considers necessary.
  void RationalizeFieldTypePredictions(
      const url::Origin& main_origin,
      const GeoIpCountryCode& client_country,
      const LanguageCode& language_code,
      LogManager* log_manager);

  // Ensures that only a single phone number (which can be split across multiple
  // fields) is autofilled per section. If a section contains multiple phone
  // numbers, `only_fill_when_focused` is set for the remaining fields.
  void RationalizePhoneNumbersForFilling();

 private:
  friend class FormStructureTestApi;

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

  // Rewrites two or three (not necessarily consecutive)
  // ADDRESS_HOME_STREET_ADDRESS fields in the same section into address line 1,
  // 2 and 3.
  void RationalizeRepeatedStreetAddressFields(LogManager* log_manager);

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

  // Rationalizes all PHONE_HOME_COUNTRY_CODE fields to UNKNOWN_TYPE if no other
  // phone number fields exist among the `fields_`.
  void RationalizePhoneCountryCode(LogManager* log_manager);

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
