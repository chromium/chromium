// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"

#include <string_view>

#include "base/containers/to_vector.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_qualifiers.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"

namespace autofill::test {

namespace {

using ::testing::ElementsAre;
using ::testing::Message;

constexpr std::string_view kDefaultTestOrigin = "https://example.test/";

}  // namespace

Message DescribeFormData(const FormData& form_data) {
  Message result;
  result << "Form contains " << form_data.fields().size() << " fields:\n";
  for (const FormFieldData& field : form_data.fields()) {
    result << "type=" << FormControlTypeToString(field.form_control_type())
           << ", name=" << field.name() << ", label=" << field.label() << "\n";
  }
  return result;
}

// Returns the form field relevant to the `role`.
FormFieldData CreateFieldByRole(FieldType role) {
  DCHECK(role != FieldType::MAX_VALID_FIELD_TYPE)
      << "MAX_VALID_FIELD_TYPE is not a valid field type.";
  FormFieldData field;
  // Set the name and label of the field based on the `role`.
  switch (role) {
    case FieldType::USERNAME:
      field.set_name(u"username");
      field.set_label(u"Username");
      break;
    case FieldType::NAME_FULL:
      field.set_name(u"name");
      field.set_label(u"Name");
      break;
    case FieldType::NAME_FIRST:
      field.set_name(u"firstName");
      field.set_label(u"First name");
      break;
    case FieldType::NAME_LAST:
      field.set_name(u"lastName");
      field.set_label(u"Last name");
      break;
    case FieldType::EMAIL_ADDRESS:
      field.set_name(u"email");
      field.set_label(u"E-mail address");
      break;
    case FieldType::ADDRESS_HOME_LINE1:
      field.set_name(u"home_line_one");
      field.set_label(u"Address line 1");
      break;
    case FieldType::ADDRESS_HOME_CITY:
      field.set_name(u"city");
      field.set_label(u"City");
      break;
    case FieldType::ADDRESS_HOME_STATE:
      field.set_name(u"state");
      field.set_label(u"State");
      break;
    case FieldType::ADDRESS_HOME_COUNTRY:
      field.set_name(u"country");
      field.set_label(u"Country");
      break;
    case FieldType::ADDRESS_HOME_ZIP:
      field.set_name(u"zipCode");
      field.set_label(u"ZIP code");
      break;
    case FieldType::PHONE_HOME_NUMBER:
      field.set_name(u"phone");
      field.set_label(u"Phone");
      break;
    case FieldType::COMPANY_NAME:
      field.set_name(u"company");
      field.set_label(u"Company");
      break;
    case FieldType::CREDIT_CARD_NUMBER:
      field.set_name(u"cardNumber");
      field.set_label(u"Credit card number");
      break;
    case FieldType::PASSWORD:
      field.set_name(u"password");
      field.set_label(u"Password");
      break;
    case FieldType::ADDRESS_HOME_ADMIN_LEVEL2:
      field.set_name(u"admin_level2");
      field.set_label(u"Admin level 2");
      break;
    case FieldType::ADDRESS_HOME_APT:
      field.set_name(u"apartment");
      field.set_label(u"Apartment");
      break;
    case FieldType::ADDRESS_HOME_APT_NUM:
      field.set_name(u"apt_num");
      field.set_label(u"Apt/Suite");
      break;
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS:
      field.set_name(u"between_streets");
      field.set_label(u"Between streets");
      break;
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS_1:
      field.set_name(u"between_streets_1");
      field.set_label(u"Between streets 1");
      break;
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS_2:
      field.set_name(u"between_streets_2");
      field.set_label(u"Between streets 2");
      break;
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
      field.set_name(u"between_streets_or_landmark");
      field.set_label(u"Between streets or landmark");
      break;
    case FieldType::ADDRESS_HOME_DEPENDENT_LOCALITY:
      field.set_name(u"dependent_locality");
      field.set_label(u"Dependent locality");
      break;
    case FieldType::ADDRESS_HOME_HOUSE_NUMBER:
      field.set_name(u"house_number");
      field.set_label(u"House number");
      break;
    case FieldType::ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
      field.set_name(u"house_number_and_apt");
      field.set_label(u"House number and apt");
      break;
    case FieldType::ADDRESS_HOME_LANDMARK:
      field.set_name(u"landmark");
      field.set_label(u"Landmark");
      break;
    case FieldType::ADDRESS_HOME_LINE2:
      field.set_name(u"address2");
      field.set_label(u"Address line 2");
      break;
    case FieldType::ADDRESS_HOME_LINE3:
      field.set_name(u"address3");
      field.set_label(u"Address line 3");
      break;
    case FieldType::ADDRESS_HOME_OVERFLOW:
      field.set_name(u"overflow");
      field.set_label(u"Overflow");
      break;
    case FieldType::ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
      field.set_name(u"overflow_and_landmark");
      field.set_label(u"Overflow and landmark");
      break;
    case FieldType::ADDRESS_HOME_STREET_ADDRESS:
      field.set_name(u"street_address");
      field.set_label(u"Street address");
      break;
    case FieldType::ADDRESS_HOME_STREET_LOCATION:
      field.set_name(u"street_location");
      field.set_label(u"Street location");
      break;
    case FieldType::ADDRESS_HOME_STREET_NAME:
      field.set_name(u"street_name");
      field.set_label(u"Street name");
      break;
    case FieldType::ALTERNATIVE_FAMILY_NAME:
      field.set_name(u"alt_family_name");
      field.set_label(u"Alternative family name");
      break;
    case FieldType::ALTERNATIVE_FULL_NAME:
      field.set_name(u"alt_full_name");
      field.set_label(u"Alternative full name");
      break;
    case FieldType::ALTERNATIVE_GIVEN_NAME:
      field.set_name(u"alternative_given_name");
      field.set_label(u"Alternative given name");
      break;
    case FieldType::CREDIT_CARD_EXP_2_DIGIT_YEAR:
      field.set_name(u"exp_year_2_digit");
      field.set_label(u"Credit card expiration year 2-digit");
      break;
    case FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      field.set_name(u"cc_exp_year_4_digit");
      field.set_label(u"Credit card expiration year 4-digit");
      break;
    case FieldType::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      field.set_name(u"cc_exp_date_2_digit_year");
      field.set_label(u"Credit card expiration date 2-digit year");
      break;
    case FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      field.set_name(u"cc_exp_date_4_digit_year");
      field.set_label(u"Credit card expiration date 4-digit year");
      break;
    case FieldType::CREDIT_CARD_EXP_MONTH:
      field.set_name(u"cc_exp_month");
      field.set_label(u"Credit card expiration month");
      break;
    case FieldType::CREDIT_CARD_NAME_FIRST:
      field.set_name(u"cc_name_first");
      field.set_label(u"Credit card first name");
      break;
    case FieldType::CREDIT_CARD_NAME_FULL:
      field.set_name(u"cc_name_full");
      field.set_label(u"Credit card full name");
      break;
    case FieldType::CREDIT_CARD_NAME_LAST:
      field.set_name(u"cc_name_last");
      field.set_label(u"Credit card last name");
      break;
    case FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      field.set_name(u"cc_csc_standalone");
      field.set_label(u"Credit card verification code standalone");
      break;
    case FieldType::CREDIT_CARD_TYPE:
      field.set_name(u"cc_type");
      field.set_label(u"Credit card type");
      break;
    case FieldType::CREDIT_CARD_VERIFICATION_CODE:
      field.set_name(u"cc_csc");
      field.set_label(u"Credit card verification code");
      break;
    case FieldType::IBAN_VALUE:
      field.set_name(u"iban");
      field.set_label(u"IBAN");
      break;
    case FieldType::EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
      field.set_name(u"email_or_loyalty_card_number");
      field.set_label(u"email or loyalty card number");
      break;
    case FieldType::LOYALTY_MEMBERSHIP_ID:
      field.set_name(u"loyalty_card_number");
      field.set_label(u"loyalty card number");
      break;
    case FieldType::MERCHANT_PROMO_CODE:
      field.set_name(u"promo_code");
      field.set_label(u"Promo code");
      break;
    case FieldType::NAME_HONORIFIC_PREFIX:
      field.set_name(u"honorific_prefix");
      field.set_label(u"Honorific prefix");
      break;
    case FieldType::NAME_LAST_FIRST:
      field.set_name(u"lname_first");
      field.set_label(u"Last name first");
      break;
    case FieldType::NAME_LAST_SECOND:
      field.set_name(u"lname_second");
      field.set_label(u"Last name second");
      break;
    case FieldType::NAME_MIDDLE:
      field.set_name(u"mname");
      field.set_label(u"Middle name");
      break;
    case FieldType::NAME_MIDDLE_INITIAL:
      field.set_name(u"minitial");
      field.set_label(u"Middle initial");
      break;
    case FieldType::NUMERIC_QUANTITY:
      field.set_name(u"numeric_quantity");
      field.set_label(u"Numeric quantity");
      break;
    case FieldType::PHONE_HOME_CITY_CODE:
      field.set_name(u"phone_city_code");
      field.set_label(u"Phone city code");
      break;
    case FieldType::PHONE_HOME_COUNTRY_CODE:
      field.set_name(u"phone_country_code");
      field.set_label(u"Phone country code");
      break;
    case FieldType::PHONE_HOME_EXTENSION:
      field.set_name(u"phone_ext");
      field.set_label(u"Phone extension");
      break;
    case FieldType::PHONE_HOME_NUMBER_PREFIX:
      field.set_name(u"phone_prefix");
      field.set_label(u"Phone number prefix");
      break;
    case FieldType::PHONE_HOME_NUMBER_SUFFIX:
      field.set_name(u"phone_suffix");
      field.set_label(u"Phone number suffix");
      break;
    case FieldType::PHONE_HOME_WHOLE_NUMBER:
    case FieldType::PHONE_HOME_CITY_AND_NUMBER:
    case FieldType::PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
      field.set_label(u"Phone Number");
      field.set_name(u"phone_number");
      break;
    case FieldType::PRICE:
      field.set_name(u"price");
      field.set_label(u"Price");
      break;
    case FieldType::SEARCH_TERM:
      field.set_name(u"search");
      field.set_label(u"Search");
      break;
    case FieldType::UNKNOWN_TYPE:
    case FieldType::EMPTY_TYPE:
      break;
    case FieldType::NOT_NEW_PASSWORD:
    case FieldType::NO_SERVER_DATA:
    case FieldType::PASSPORT_NUMBER:
    case FieldType::PASSPORT_ISSUING_COUNTRY:
    case FieldType::PASSPORT_EXPIRATION_DATE:
    case FieldType::PASSPORT_ISSUE_DATE:
    case FieldType::LOYALTY_MEMBERSHIP_PROGRAM:
    case FieldType::LOYALTY_MEMBERSHIP_PROVIDER:
    case FieldType::VEHICLE_LICENSE_PLATE:
    case FieldType::VEHICLE_VIN:
    case FieldType::VEHICLE_MAKE:
    case FieldType::VEHICLE_MODEL:
    case FieldType::DRIVERS_LICENSE_REGION:
    case FieldType::DRIVERS_LICENSE_NUMBER:
    case FieldType::DRIVERS_LICENSE_EXPIRATION_DATE:
    case FieldType::DRIVERS_LICENSE_ISSUE_DATE:
    case FieldType::VEHICLE_YEAR:
    case FieldType::VEHICLE_PLATE_STATE:
    case FieldType::NATIONAL_ID_CARD_NUMBER:
    case FieldType::NATIONAL_ID_CARD_EXPIRATION_DATE:
    case FieldType::NATIONAL_ID_CARD_ISSUE_DATE:
    case FieldType::NATIONAL_ID_CARD_ISSUING_COUNTRY:
    case FieldType::KNOWN_TRAVELER_NUMBER:
    case FieldType::KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
    case FieldType::REDRESS_NUMBER:
    case FieldType::FLIGHT_RESERVATION_FLIGHT_NUMBER:
    case FieldType::FLIGHT_RESERVATION_CONFIRMATION_CODE:
    case FieldType::FLIGHT_RESERVATION_TICKET_NUMBER:
    case FieldType::FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
    case FieldType::FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
    case FieldType::FLIGHT_RESERVATION_DEPARTURE_DATE:
    case FieldType::NAME_SUFFIX:
    case FieldType::MERCHANT_EMAIL_SIGNUP:
    case FieldType::ACCOUNT_CREATION_PASSWORD:
    case FieldType::ADDRESS_HOME_SORTING_CODE:
    case FieldType::NOT_ACCOUNT_CREATION_PASSWORD:
    case FieldType::USERNAME_AND_EMAIL_ADDRESS:
    case FieldType::NEW_PASSWORD:
    case FieldType::PROBABLY_NEW_PASSWORD:
    case FieldType::NOT_PASSWORD:
    case FieldType::CONFIRMATION_PASSWORD:
    case FieldType::AMBIGUOUS_TYPE:
    case FieldType::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
    case FieldType::NAME_LAST_PREFIX:
    case FieldType::NAME_LAST_CORE:
    case FieldType::ADDRESS_HOME_ZIP_PREFIX:
    case FieldType::ADDRESS_HOME_ZIP_SUFFIX:
    case FieldType::ADDRESS_HOME_ZIP_AND_CITY:
    case FieldType::SINGLE_USERNAME:
    case FieldType::NOT_USERNAME:
    case FieldType::ADDRESS_HOME_SUBPREMISE:
    case FieldType::ADDRESS_HOME_OTHER_SUBUNIT:
    case FieldType::NAME_LAST_CONJUNCTION:
    case FieldType::ADDRESS_HOME_ADDRESS:
    case FieldType::ADDRESS_HOME_ADDRESS_WITH_NAME:
    case FieldType::ADDRESS_HOME_FLOOR:
    case FieldType::PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case FieldType::ONE_TIME_CODE:
    case FieldType::DELIVERY_INSTRUCTIONS:
    case FieldType::ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case FieldType::ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case FieldType::ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case FieldType::SINGLE_USERNAME_FORGOT_PASSWORD:
    case FieldType::ADDRESS_HOME_APT_TYPE:
    case FieldType::ORDER_ID:
    case FieldType::ORDER_DATE:
    case FieldType::ORDER_MERCHANT_NAME:
    case FieldType::ORDER_MERCHANT_DOMAIN:
    case FieldType::ORDER_PRODUCT_NAMES:
    case FieldType::ORDER_ACCOUNT:
    case FieldType::ORDER_GRAND_TOTAL:
    case FieldType::MAX_VALID_FIELD_TYPE:
      LOG(ERROR) << "The field created by " << __func__ << "("
                 << FieldTypeToStringView(role)
                 << ") will not get a name and label assigned";
      break;
  }

  // Provide a warning if the role type is not predictable by the local
  // heuristics.
  switch (role) {
    // The following types are predicted by the local heuristics.
    case FieldType::NAME_FULL:
    case FieldType::NAME_FIRST:
    case FieldType::NAME_LAST:
    case FieldType::EMAIL_ADDRESS:
    case FieldType::ADDRESS_HOME_LINE1:
    case FieldType::ADDRESS_HOME_CITY:
    case FieldType::ADDRESS_HOME_STATE:
    case FieldType::ADDRESS_HOME_COUNTRY:
    case FieldType::ADDRESS_HOME_ZIP:
    case FieldType::PHONE_HOME_NUMBER:
    case FieldType::COMPANY_NAME:
    case FieldType::CREDIT_CARD_NUMBER:
    case FieldType::PASSWORD:
    case FieldType::ADDRESS_HOME_ADMIN_LEVEL2:
    case FieldType::ADDRESS_HOME_APT:
    case FieldType::ADDRESS_HOME_APT_NUM:
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS:
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS_1:
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS_2:
    case FieldType::ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
    case FieldType::ADDRESS_HOME_DEPENDENT_LOCALITY:
    case FieldType::ADDRESS_HOME_HOUSE_NUMBER:
    case FieldType::ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
    case FieldType::ADDRESS_HOME_LANDMARK:
    case FieldType::ADDRESS_HOME_LINE2:
    case FieldType::ADDRESS_HOME_LINE3:
    case FieldType::ADDRESS_HOME_OVERFLOW:
    case FieldType::ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
    case FieldType::ADDRESS_HOME_STREET_ADDRESS:
    case FieldType::ADDRESS_HOME_STREET_LOCATION:
    case FieldType::ADDRESS_HOME_STREET_NAME:
    case FieldType::ALTERNATIVE_FAMILY_NAME:
    case FieldType::ALTERNATIVE_FULL_NAME:
    case FieldType::ALTERNATIVE_GIVEN_NAME:
    case FieldType::CREDIT_CARD_EXP_2_DIGIT_YEAR:
    case FieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR:
    case FieldType::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
    case FieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
    case FieldType::CREDIT_CARD_EXP_MONTH:
    case FieldType::CREDIT_CARD_NAME_FIRST:
    case FieldType::CREDIT_CARD_NAME_FULL:
    case FieldType::CREDIT_CARD_NAME_LAST:
    case FieldType::CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
    case FieldType::CREDIT_CARD_TYPE:
    case FieldType::CREDIT_CARD_VERIFICATION_CODE:
    case FieldType::EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
    case FieldType::IBAN_VALUE:
    case FieldType::LOYALTY_MEMBERSHIP_ID:
    case FieldType::MERCHANT_PROMO_CODE:
    case FieldType::NAME_HONORIFIC_PREFIX:
    case FieldType::NAME_LAST_FIRST:
    case FieldType::NAME_LAST_SECOND:
    case FieldType::NAME_MIDDLE:
    case FieldType::NAME_MIDDLE_INITIAL:
    case FieldType::NUMERIC_QUANTITY:
    case FieldType::PHONE_HOME_CITY_AND_NUMBER:
    case FieldType::PHONE_HOME_CITY_CODE:
    case FieldType::PHONE_HOME_COUNTRY_CODE:
    case FieldType::PHONE_HOME_EXTENSION:
    case FieldType::PHONE_HOME_NUMBER_PREFIX:
    case FieldType::PHONE_HOME_NUMBER_SUFFIX:
    case FieldType::PHONE_HOME_WHOLE_NUMBER:
    case FieldType::PRICE:
    case FieldType::SEARCH_TERM:
    case FieldType::UNKNOWN_TYPE:
    case FieldType::EMPTY_TYPE:
      break;

    // The following types cannot be predicted by the local heuristics.
    case FieldType::USERNAME:
    case FieldType::NOT_NEW_PASSWORD:
    case FieldType::SINGLE_USERNAME:
    case FieldType::NO_SERVER_DATA:
    case FieldType::PASSPORT_NUMBER:
    case FieldType::PASSPORT_ISSUING_COUNTRY:
    case FieldType::PASSPORT_EXPIRATION_DATE:
    case FieldType::PASSPORT_ISSUE_DATE:
    case FieldType::LOYALTY_MEMBERSHIP_PROGRAM:
    case FieldType::LOYALTY_MEMBERSHIP_PROVIDER:
    case FieldType::VEHICLE_LICENSE_PLATE:
    case FieldType::VEHICLE_VIN:
    case FieldType::VEHICLE_MAKE:
    case FieldType::VEHICLE_MODEL:
    case FieldType::DRIVERS_LICENSE_REGION:
    case FieldType::DRIVERS_LICENSE_NUMBER:
    case FieldType::DRIVERS_LICENSE_EXPIRATION_DATE:
    case FieldType::DRIVERS_LICENSE_ISSUE_DATE:
    case FieldType::VEHICLE_YEAR:
    case FieldType::VEHICLE_PLATE_STATE:
    case FieldType::NATIONAL_ID_CARD_NUMBER:
    case FieldType::NATIONAL_ID_CARD_EXPIRATION_DATE:
    case FieldType::NATIONAL_ID_CARD_ISSUE_DATE:
    case FieldType::NATIONAL_ID_CARD_ISSUING_COUNTRY:
    case FieldType::KNOWN_TRAVELER_NUMBER:
    case FieldType::KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
    case FieldType::REDRESS_NUMBER:
    case FieldType::FLIGHT_RESERVATION_FLIGHT_NUMBER:
    case FieldType::FLIGHT_RESERVATION_CONFIRMATION_CODE:
    case FieldType::FLIGHT_RESERVATION_TICKET_NUMBER:
    case FieldType::FLIGHT_RESERVATION_DEPARTURE_AIRPORT:
    case FieldType::FLIGHT_RESERVATION_ARRIVAL_AIRPORT:
    case FieldType::FLIGHT_RESERVATION_DEPARTURE_DATE:
    case FieldType::PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
    case FieldType::NAME_SUFFIX:
    case FieldType::MERCHANT_EMAIL_SIGNUP:
    case FieldType::ACCOUNT_CREATION_PASSWORD:
    case FieldType::ADDRESS_HOME_SORTING_CODE:
    case FieldType::NOT_ACCOUNT_CREATION_PASSWORD:
    case FieldType::USERNAME_AND_EMAIL_ADDRESS:
    case FieldType::NEW_PASSWORD:
    case FieldType::PROBABLY_NEW_PASSWORD:
    case FieldType::NOT_PASSWORD:
    case FieldType::CONFIRMATION_PASSWORD:
    case FieldType::AMBIGUOUS_TYPE:
    case FieldType::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
    case FieldType::NAME_LAST_PREFIX:
    case FieldType::NAME_LAST_CORE:
    case FieldType::ADDRESS_HOME_ZIP_PREFIX:
    case FieldType::ADDRESS_HOME_ZIP_SUFFIX:
    case FieldType::ADDRESS_HOME_ZIP_AND_CITY:
    case FieldType::NOT_USERNAME:
    case FieldType::ADDRESS_HOME_SUBPREMISE:
    case FieldType::ADDRESS_HOME_OTHER_SUBUNIT:
    case FieldType::NAME_LAST_CONJUNCTION:
    case FieldType::ADDRESS_HOME_ADDRESS:
    case FieldType::ADDRESS_HOME_ADDRESS_WITH_NAME:
    case FieldType::ADDRESS_HOME_FLOOR:
    case FieldType::PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
    case FieldType::ONE_TIME_CODE:
    case FieldType::DELIVERY_INSTRUCTIONS:
    case FieldType::ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
    case FieldType::ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
    case FieldType::ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
    case FieldType::SINGLE_USERNAME_FORGOT_PASSWORD:
    case FieldType::ADDRESS_HOME_APT_TYPE:
    case FieldType::ORDER_ID:
    case FieldType::ORDER_DATE:
    case FieldType::ORDER_MERCHANT_NAME:
    case FieldType::ORDER_MERCHANT_DOMAIN:
    case FieldType::ORDER_PRODUCT_NAMES:
    case FieldType::ORDER_ACCOUNT:
    case FieldType::ORDER_GRAND_TOTAL:
    case FieldType::MAX_VALID_FIELD_TYPE:
      LOG(ERROR) << "The field created by " << __func__ << "("
                 << FieldTypeToStringView(role)
                 << ") will not be assigned to the expected "
                    "FieldType by the local heuristics!";
      break;
  }
  return field;
}

FormFieldData GetFormFieldData(const FieldDescription& fd) {
  FormFieldData ff = CreateFieldByRole(fd.role);
  ff.set_form_control_type(fd.form_control_type);
  if (ff.form_control_type() == FormControlType::kSelectOne &&
      !fd.select_options.empty()) {
    ff.set_options(fd.select_options);
  }
  if (!fd.datalist_options.empty()) {
    ff.set_datalist_options(fd.datalist_options);
  }
  ff.set_renderer_id(fd.renderer_id.value_or(MakeFieldRendererId()));
  ff.set_host_form_id(MakeFormRendererId());
  ff.set_is_focusable(fd.is_focusable);
  ff.set_is_visible(fd.is_visible);
  if (!fd.autocomplete_attribute.empty()) {
    ff.set_autocomplete_attribute(fd.autocomplete_attribute);
    ff.set_parsed_autocomplete(
        ParseAutocompleteAttribute(fd.autocomplete_attribute));
  }
  if (fd.host_frame) {
    ff.set_host_frame(*fd.host_frame);
  }
  if (fd.host_form_signature) {
    ff.set_host_form_signature(*fd.host_form_signature);
  }
  if (fd.label) {
    ff.set_label(*fd.label);
  }
  if (fd.name) {
    ff.set_name(*fd.name);
  }
  if (fd.name_attribute) {
    ff.set_name_attribute(*fd.name_attribute);
  }
  if (fd.id_attribute) {
    ff.set_id_attribute(*fd.id_attribute);
  }
  if (fd.nonce) {
    ff.set_nonce(*fd.nonce);
  }
  if (fd.value) {
    ff.set_value(*fd.value);
  }
  if (fd.placeholder) {
    ff.set_placeholder(*fd.placeholder);
  }
  if (fd.aria_label) {
    ff.set_aria_label(*fd.aria_label);
  }
  if (fd.aria_description) {
    ff.set_aria_description(*fd.aria_description);
  }
  if (fd.max_length) {
    ff.set_max_length(*fd.max_length);
  }
  if (fd.origin) {
    ff.set_origin(*fd.origin);
  }
  ff.set_is_autofilled_according_to_renderer(
      fd.is_autofilled_according_to_renderer.value_or(false));
  ff.set_should_autocomplete(fd.should_autocomplete);
  ff.set_properties_mask(fd.properties_mask);
  if (ff.form_control_type() == FormControlType::kInputCheckbox ||
      ff.form_control_type() == FormControlType::kInputRadio) {
    ff.set_check_status(
        fd.checked ? FormFieldData::CheckStatus::kChecked
                   : FormFieldData::CheckStatus::kCheckableButUnchecked);
  }
  if (fd.form_control_ax_id) {
    ff.set_form_control_ax_id(*fd.form_control_ax_id);
  }
  CHECK(!fd.checked ||
        ff.form_control_type() == FormControlType::kInputCheckbox ||
        ff.form_control_type() == FormControlType::kInputRadio)
      << "Only <input type=checkbox> and <input type=radio> are checkable";
  return ff;
}

FormData GetFormData(const FormDescription& d) {
  FormData f;
  f.set_url(GURL(d.url));
  f.set_action(GURL(d.action));
  f.set_name(d.name);
  f.set_host_frame(d.host_frame.value_or(MakeLocalFrameToken()));
  f.set_renderer_id(d.renderer_id.value_or(MakeFormRendererId()));
  if (d.main_frame_origin) {
    f.set_main_frame_origin(*d.main_frame_origin);
  } else {
    f.set_main_frame_origin(url::Origin::Create(GURL(kDefaultTestOrigin)));
  }
  f.set_fields(base::ToVector(d.fields, [&f](const FieldDescription& dd) {
    FormFieldData ff = GetFormFieldData(dd);
    ff.set_host_frame(dd.host_frame.value_or(f.host_frame()));
    ff.set_origin(dd.origin.value_or(f.main_frame_origin()));
    ff.set_host_form_id(f.renderer_id());
    return ff;
  }));
  return f;
}

FormData GetFormData(const std::vector<FieldType>& field_types) {
  FormDescription form_description;
  form_description.fields.reserve(field_types.size());
  for (FieldType type : field_types) {
    form_description.fields.emplace_back(type);
  }
  return GetFormData(form_description);
}

std::vector<FieldType> GetHeuristicTypes(
    const FormDescription& form_description) {
  return base::ToVector(form_description.fields, [](const auto& field) {
    return field.heuristic_type.value_or(field.role);
  });
}

std::vector<FieldType> GetServerTypes(const FormDescription& form_description) {
  return base::ToVector(form_description.fields, [](const auto& field) {
    return field.server_type.value_or(field.role);
  });
}

// static
void FormStructureTest::CheckFormStructureTestData(
    const std::vector<FormStructureTestCase>& test_cases) {
  for (const FormStructureTestCase& test_case : test_cases) {
    const FormData form = GetFormData(test_case.form_attributes);
    SCOPED_TRACE(Message("Test description: ")
                 << test_case.form_attributes.description_for_logging);

    auto form_structure = std::make_unique<FormStructure>(form);

    if (test_case.form_flags.determine_heuristic_type) {
      const RegexPredictions regex_predictions = DetermineRegexTypes(
          GeoIpCountryCode(""), LanguageCode(""), form_structure->ToFormData(),
          nullptr, /*ignore_small_forms=*/true);
      regex_predictions.ApplyTo(form_structure->fields());
      form_structure->RationalizeAndAssignSections(GeoIpCountryCode(""),
                                                   LanguageCode(""), nullptr);
    }

    if (test_case.form_flags.is_autofillable) {
      EXPECT_TRUE(IsAutofillable(*form_structure));
    }
    if (test_case.form_flags.should_be_parsed) {
      EXPECT_TRUE(ShouldBeParsed(*form_structure, /*log_manager=*/nullptr));
    }
    if (test_case.form_flags.should_be_queried) {
      EXPECT_TRUE(ShouldBeQueried(*form_structure));
    }
    if (test_case.form_flags.should_be_uploaded) {
      EXPECT_TRUE(ShouldBeUploaded(*form_structure));
    }
    if (test_case.form_flags.has_author_specified_types) {
      EXPECT_TRUE(
          std::ranges::any_of(form_structure->fields(), [](const auto& field) {
            return field->parsed_autocomplete().has_value();
          }));
    }

    if (test_case.form_flags.is_complete_credit_card_form.has_value()) {
      auto [completeness, expected_result] =
          *test_case.form_flags.is_complete_credit_card_form;
      EXPECT_EQ(form_structure->IsCompleteCreditCardForm(completeness),
                expected_result);
    }
    if (test_case.form_flags.field_count) {
      ASSERT_EQ(*test_case.form_flags.field_count,
                static_cast<int>(form_structure->field_count()));
    }
    if (test_case.form_flags.autofill_count) {
      ASSERT_EQ(*test_case.form_flags.autofill_count,
                static_cast<int>(std::ranges::count_if(
                    form_structure->fields(), [](const auto& field) {
                      return field->IsFieldFillable();
                    })));
    }
    if (test_case.form_flags.section_count) {
      std::set<Section> section_names;
      for (const auto& field : *form_structure) {
        section_names.insert(field->section());
      }
      EXPECT_EQ(*test_case.form_flags.section_count,
                static_cast<int>(section_names.size()));
    }

    for (size_t i = 0;
         i < test_case.expected_field_types.expected_html_type.size(); ++i) {
      EXPECT_EQ(test_case.expected_field_types.expected_html_type[i],
                form_structure->field(i)->html_type());
    }
    for (size_t i = 0;
         i < test_case.expected_field_types.expected_heuristic_type.size();
         ++i) {
      EXPECT_EQ(test_case.expected_field_types.expected_heuristic_type[i],
                form_structure->field(i)->heuristic_type());
    }
    for (size_t i = 0;
         i < test_case.expected_field_types.expected_overall_type.size(); ++i) {
      EXPECT_THAT(
          form_structure->field(i)->Type().GetTypes(),
          ElementsAre(test_case.expected_field_types.expected_overall_type[i]));
    }
  }
}

}  // namespace autofill::test
