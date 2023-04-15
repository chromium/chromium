// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_

#include <string>

#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"

namespace autofill {

class AutofillProfile;
class PersonalDataManager;

// Extended addressinput struct for storing Autofill specific data.
struct ExtendedAddressUiComponent
    : public ::i18n::addressinput::AddressUiComponent {
  bool is_required = false;

  ExtendedAddressUiComponent(const ::i18n::addressinput::AddressUiComponent&&,
                             bool);
  explicit ExtendedAddressUiComponent(
      const ::i18n::addressinput::AddressUiComponent&&);
};

// Creates extended ui components from libaddressinput ones with respect to
// the |country| provided.
std::vector<ExtendedAddressUiComponent> ConvertAddressUiComponents(
    const std::vector<::i18n::addressinput::AddressUiComponent>&
        addressinput_components,
    const AutofillCountry& country);

// Extend `components` using Autofill's address format extensions. These are
// used make fields beyond libaddressinput's format available in Autofill's
// settings UI and import dialogs.
void ExtendAddressComponents(
    std::vector<ExtendedAddressUiComponent>& components,
    const AutofillCountry& country,
    const ::i18n::addressinput::Localization& localization,
    bool include_literals);

// |address_components| is a 2D array for the address components in each line.
// Fills |address_components| with the address UI components that should be used
// to input an address for |country_code| when UI BCP 47 language code is
// |ui_language_code|. If |components_language_code| is not NULL, then sets it
// to the BCP 47 language code that should be used to format the address for
// display. If no components are available for |country_code|, it defaults back
// to the US. |include_literals| controls whether formatting literals such as
// ", " and "-" should be returned.
void GetAddressComponents(
    const std::string& country_code,
    const std::string& ui_language_code,
    bool include_literals,
    std::vector<std::vector<ExtendedAddressUiComponent>>* address_components,
    std::string* components_language_code);

// Returns the address stored in `profile` when UI BCP 47 language code is
// `ui_language_code`. If the format of the country in `profile` isn't known,
// the US address format is used instead. If `ui_language_code` is not valid,
// the default format is returned. If `include_recipient` is true, the recipient
// full name will be included. If `include_country` is true, the country
// will be appended in a separate line at the end.
std::u16string GetEnvelopeStyleAddress(const AutofillProfile& profile,
                                       const std::string& ui_language_code,
                                       bool include_recipient,
                                       bool include_country);

// Returns a one-line `profile` description, listing (at max) 2 significant
// user-visible fields with respect to UI BCP 47 language code in
// `ui_language_code`. If `include_address_and_contacts` is false, only full
// name is included, and the returned string can be empty if the name is not
// present.
std::u16string GetProfileDescription(const AutofillProfile& profile,
                                     const std::string& ui_language_code,
                                     bool include_address_and_contacts);

// Fields in order they should appear in differences for AutofillProfile update.
static constexpr ServerFieldType kVisibleTypesForProfileDifferences[] = {
    NAME_FULL_WITH_HONORIFIC_PREFIX,
    COMPANY_NAME,
    ADDRESS_HOME_STREET_ADDRESS,
    ADDRESS_HOME_DEPENDENT_LOCALITY,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STATE,
    ADDRESS_HOME_ZIP,
    ADDRESS_HOME_COUNTRY,
    EMAIL_ADDRESS,
    PHONE_HOME_WHOLE_NUMBER};

// Gets the difference of two profiles in name, address, email and phone number
// in that order. Differences in name, email and phone number are computed and
// keyed by NAME_FULL_WITH_HONORIFIC_PREFIX, EMAIL_ADDRESS and
// PHONE_HOME_WHOLE_NUMBER respectively. Address differences are computed by
// difference in the envelope style address of both profile, and keyed by
// ADDRESS_HOME_ADDRESS. All computations are done against `app_locale`.
std::vector<ProfileValueDifference> GetProfileDifferenceForUi(
    const AutofillProfile& first_profile,
    const AutofillProfile& second_profile,
    const std::string& app_locale);

// Returns a multi line `profile` description comprising of full name, address,
// email and phone in separate lines if they are non-empty.
std::u16string GetProfileSummaryForMigrationPrompt(
    const AutofillProfile& profile,
    const std::string& app_locale);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_ADDRESS_UTIL_H_
