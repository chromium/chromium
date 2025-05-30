// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/data_model_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace autofill {

class AutofillProfile;
class CreditCard;
class EntityInstance;
class FormStructure;
class LoyaltyCard;

// Note that the `dates` and `formats` are not aligned (i.e., do not base::zip()
// them!). They may even be of distinct size (see Example 2 of
// ExtractDatesInFields()).
struct DatesAndFormats {
  DatesAndFormats();
  DatesAndFormats(base::flat_set<data_util::Date> dates,
                  base::flat_set<std::u16string> format_strings);
  DatesAndFormats(const DatesAndFormats&) = delete;
  DatesAndFormats& operator=(const DatesAndFormats&) = delete;
  DatesAndFormats(DatesAndFormats&&);
  DatesAndFormats& operator=(DatesAndFormats&&);
  ~DatesAndFormats();

  base::flat_set<data_util::Date> dates;
  base::flat_set<std::u16string> formats;
};

// Looks for date values in `fields`. The returned map contains an entry for
// each field whose value is either a complete date or part of a complete date.
//
// Example 1:
// Input: One field with value "09/03/2025".
// Returns: A map from the field's FieldGlobalId to:
// - {.dates = {{2025,03,09}, {2025,09,03}},
//    .format_strings = {u"DD/MM/YYYY", u"MM/DD/YYYY"}}.
//
// Example 2:
// Input: One field with value "01/01/01".
// Returns: A map from the field's FieldGlobalId to:
// - {.dates = {{2001,01,01}},
//    .format_strings = {u"DD/MM/YY", u"MM/DD/YY", u"YY/MM/DD"}}.
//
// Example 3:
// Input: Three consecutive fields with values "09", "03", "2025", respectively.
// Returns: A map from the three field's FieldGlobalIds to, respectively:
// - {.dates = {{2025,03,09}, {2025,09,03}}, .format_strings = {u"DD", u"MM"}}
// - {.dates = {{2025,03,09}, {2025,09,03}}, .format_strings = {u"DD", u"MM"}}
// - {.dates = {{2025,03,09}, {2025,09,03}}, .format_strings = {u"YYYY"}}
std::map<FieldGlobalId, DatesAndFormats> ExtractDatesInFields(
    base::span<const std::unique_ptr<AutofillField>> fields);

// For each submitted field in the `form_structure`, determines whether
// `ADDRESS_HOME_STATE` is a possible matching type.
// This function is intended to run on the UI thread and its result can be
// passed to DeterminePossibleFieldTypesForUpload().
[[nodiscard]] std::set<FieldGlobalId> PreProcessStateMatchingTypes(
    base::span<const AutofillProfile*> profiles,
    const FormStructure& form_structure,
    const std::string& app_locale);

// Determines the `FieldType`s for which profiles etc. define non-empty
// values. The result is stored in FormStructure::possible_types().
//
// This is potentially expensive -- on the order of 50ms even for a small set of
// `stored_data`. Hence, it should not run on the UI thread -- to avoid
// locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
void DeterminePossibleFieldTypesForUpload(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    const std::set<FieldGlobalId>& fields_that_match_state,
    std::u16string_view last_unlocked_credit_card_cvc,
    const std::map<FieldGlobalId, DatesAndFormats>& dates_and_formats,
    const std::string& app_locale,
    FormStructure& form);

// Returns the set of `FieldType`s for which the given profiles etc. contain
// non-empty values.
FieldTypeSet DetermineAvailableFieldTypes(
    base::span<const AutofillProfile> profiles,
    base::span<const CreditCard> credit_cards,
    base::span<const EntityInstance> entities,
    base::span<const LoyaltyCard> loyalty_cards,
    std::u16string_view last_unlocked_credit_card_cvc,
    const std::string& app_locale);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_DETERMINE_POSSIBLE_FIELD_TYPES_H_
