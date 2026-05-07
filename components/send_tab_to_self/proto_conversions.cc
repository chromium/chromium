// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/proto_conversions.h"

#include <optional>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"

namespace send_tab_to_self {

std::optional<sync_pb::FormField_AutofillFieldType> AutofillFieldTypeToProto(
    autofill::FieldType type) {
  if (!autofill::IsFillableFieldType(type)) {
    return std::nullopt;
  }
  switch (type) {
    case autofill::NAME_HONORIFIC_PREFIX:
      return sync_pb::FormField_AutofillFieldType_NAME_HONORIFIC_PREFIX;
    case autofill::NAME_FIRST:
      return sync_pb::FormField_AutofillFieldType_NAME_FIRST;
    case autofill::NAME_MIDDLE:
      return sync_pb::FormField_AutofillFieldType_NAME_MIDDLE;
    case autofill::NAME_LAST:
      return sync_pb::FormField_AutofillFieldType_NAME_LAST;
    case autofill::NAME_LAST_FIRST:
      return sync_pb::FormField_AutofillFieldType_NAME_LAST_FIRST;
    case autofill::NAME_LAST_CONJUNCTION:
      return sync_pb::FormField_AutofillFieldType_NAME_LAST_CONJUNCTION;
    case autofill::NAME_LAST_SECOND:
      return sync_pb::FormField_AutofillFieldType_NAME_LAST_SECOND;
    case autofill::NAME_MIDDLE_INITIAL:
      return sync_pb::FormField_AutofillFieldType_NAME_MIDDLE_INITIAL;
    case autofill::NAME_FULL:
      return sync_pb::FormField_AutofillFieldType_NAME_FULL;
    case autofill::NAME_SUFFIX:
      return sync_pb::FormField_AutofillFieldType_NAME_SUFFIX;
    case autofill::ALTERNATIVE_FULL_NAME:
      return sync_pb::FormField_AutofillFieldType_ALTERNATIVE_FULL_NAME;
    case autofill::ALTERNATIVE_FAMILY_NAME:
      return sync_pb::FormField_AutofillFieldType_ALTERNATIVE_FAMILY_NAME;
    case autofill::ALTERNATIVE_GIVEN_NAME:
      return sync_pb::FormField_AutofillFieldType_ALTERNATIVE_GIVEN_NAME;
    case autofill::EMAIL_ADDRESS:
      return sync_pb::FormField_AutofillFieldType_EMAIL_ADDRESS;
    case autofill::USERNAME_AND_EMAIL_ADDRESS:
      return sync_pb::FormField_AutofillFieldType_USERNAME_AND_EMAIL_ADDRESS;
    case autofill::PHONE_HOME_NUMBER:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_NUMBER;
    case autofill::PHONE_HOME_NUMBER_PREFIX:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_NUMBER_PREFIX;
    case autofill::PHONE_HOME_NUMBER_SUFFIX:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_NUMBER_SUFFIX;
    case autofill::PHONE_HOME_CITY_CODE:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_CITY_CODE;
    case autofill::PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX:
      return sync_pb::
          FormField_AutofillFieldType_PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX;
    case autofill::PHONE_HOME_COUNTRY_CODE:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_COUNTRY_CODE;
    case autofill::PHONE_HOME_CITY_AND_NUMBER:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_CITY_AND_NUMBER;
    case autofill::PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX:
      return sync_pb::
          FormField_AutofillFieldType_PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX;
    case autofill::PHONE_HOME_WHOLE_NUMBER:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_WHOLE_NUMBER;
    case autofill::PHONE_HOME_EXTENSION:
      return sync_pb::FormField_AutofillFieldType_PHONE_HOME_EXTENSION;
    case autofill::ADDRESS_HOME_LINE1:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_LINE1;
    case autofill::ADDRESS_HOME_LINE2:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_LINE2;
    case autofill::ADDRESS_HOME_LINE3:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_LINE3;
    case autofill::ADDRESS_HOME_APT:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_APT;
    case autofill::ADDRESS_HOME_APT_NUM:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_APT_NUM;
    case autofill::ADDRESS_HOME_APT_TYPE:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_APT_TYPE;
    case autofill::ADDRESS_HOME_HOUSE_NUMBER_AND_APT:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_HOUSE_NUMBER_AND_APT;
    case autofill::ADDRESS_HOME_CITY:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_CITY;
    case autofill::ADDRESS_HOME_STATE:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_STATE;
    case autofill::ADDRESS_HOME_ZIP:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_ZIP;
    case autofill::ADDRESS_HOME_ZIP_PREFIX:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_ZIP_PREFIX;
    case autofill::ADDRESS_HOME_ZIP_SUFFIX:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_ZIP_SUFFIX;
    case autofill::ADDRESS_HOME_ZIP_AND_CITY:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_ZIP_AND_CITY;
    case autofill::ADDRESS_HOME_COUNTRY:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_COUNTRY;
    case autofill::ADDRESS_HOME_STREET_ADDRESS:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_STREET_ADDRESS;
    case autofill::ADDRESS_HOME_SORTING_CODE:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_SORTING_CODE;
    case autofill::ADDRESS_HOME_DEPENDENT_LOCALITY:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_DEPENDENT_LOCALITY;
    case autofill::ADDRESS_HOME_STREET_NAME:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_STREET_NAME;
    case autofill::ADDRESS_HOME_HOUSE_NUMBER:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_HOUSE_NUMBER;
    case autofill::ADDRESS_HOME_STREET_LOCATION:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_STREET_LOCATION;
    case autofill::ADDRESS_HOME_SUBPREMISE:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_SUBPREMISE;
    case autofill::ADDRESS_HOME_OTHER_SUBUNIT:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_OTHER_SUBUNIT;
    case autofill::ADDRESS_HOME_ADDRESS:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_ADDRESS;
    case autofill::ADDRESS_HOME_ADDRESS_WITH_NAME:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_ADDRESS_WITH_NAME;
    case autofill::ADDRESS_HOME_FLOOR:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_FLOOR;
    case autofill::ADDRESS_HOME_LANDMARK:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_LANDMARK;
    case autofill::ADDRESS_HOME_BETWEEN_STREETS:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_BETWEEN_STREETS;
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_1:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_BETWEEN_STREETS_1;
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_2:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_BETWEEN_STREETS_2;
    case autofill::ADDRESS_HOME_ADMIN_LEVEL2:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_ADMIN_LEVEL2;
    case autofill::ADDRESS_HOME_OVERFLOW:
      return sync_pb::FormField_AutofillFieldType_ADDRESS_HOME_OVERFLOW;
    case autofill::ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK;
    case autofill::ADDRESS_HOME_OVERFLOW_AND_LANDMARK:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_OVERFLOW_AND_LANDMARK;
    case autofill::ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_STREET_LOCATION_AND_LOCALITY;
    case autofill::ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_STREET_LOCATION_AND_LANDMARK;
    case autofill::ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK:
      return sync_pb::
          FormField_AutofillFieldType_ADDRESS_HOME_DEPENDENT_LOCALITY_AND_LANDMARK;
    case autofill::DELIVERY_INSTRUCTIONS:
      return sync_pb::FormField_AutofillFieldType_DELIVERY_INSTRUCTIONS;
    case autofill::LOYALTY_MEMBERSHIP_PROGRAM:
      return sync_pb::FormField_AutofillFieldType_LOYALTY_MEMBERSHIP_PROGRAM;
    case autofill::LOYALTY_MEMBERSHIP_PROVIDER:
      return sync_pb::FormField_AutofillFieldType_LOYALTY_MEMBERSHIP_PROVIDER;
    case autofill::LOYALTY_MEMBERSHIP_ID:
      return sync_pb::FormField_AutofillFieldType_LOYALTY_MEMBERSHIP_ID;
    case autofill::EMAIL_OR_LOYALTY_MEMBERSHIP_ID:
      return sync_pb::
          FormField_AutofillFieldType_EMAIL_OR_LOYALTY_MEMBERSHIP_ID;
    case autofill::CREDIT_CARD_NAME_FULL:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_NAME_FULL;
    case autofill::CREDIT_CARD_NAME_FIRST:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_NAME_FIRST;
    case autofill::CREDIT_CARD_NAME_LAST:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_NAME_LAST;
    case autofill::CREDIT_CARD_NUMBER:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_NUMBER;
    case autofill::CREDIT_CARD_EXP_MONTH:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_EXP_MONTH;
    case autofill::CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_EXP_2_DIGIT_YEAR;
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_EXP_4_DIGIT_YEAR;
    case autofill::CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      return sync_pb::
          FormField_AutofillFieldType_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    case autofill::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      return sync_pb::
          FormField_AutofillFieldType_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
    case autofill::CREDIT_CARD_TYPE:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_TYPE;
    case autofill::CREDIT_CARD_VERIFICATION_CODE:
      return sync_pb::FormField_AutofillFieldType_CREDIT_CARD_VERIFICATION_CODE;
    case autofill::CREDIT_CARD_STANDALONE_VERIFICATION_CODE:
      return sync_pb::
          FormField_AutofillFieldType_CREDIT_CARD_STANDALONE_VERIFICATION_CODE;
    case autofill::IBAN_VALUE:
      return sync_pb::FormField_AutofillFieldType_IBAN_VALUE;
    case autofill::COMPANY_NAME:
      return sync_pb::FormField_AutofillFieldType_COMPANY_NAME;
    case autofill::MERCHANT_PROMO_CODE:
      return sync_pb::FormField_AutofillFieldType_MERCHANT_PROMO_CODE;
    case autofill::USERNAME:
      return sync_pb::FormField_AutofillFieldType_USERNAME;
    case autofill::PASSWORD:
      return sync_pb::FormField_AutofillFieldType_PASSWORD;
    case autofill::ACCOUNT_CREATION_PASSWORD:
      return sync_pb::FormField_AutofillFieldType_ACCOUNT_CREATION_PASSWORD;
    case autofill::CONFIRMATION_PASSWORD:
      return sync_pb::FormField_AutofillFieldType_CONFIRMATION_PASSWORD;
    case autofill::SINGLE_USERNAME:
      return sync_pb::FormField_AutofillFieldType_SINGLE_USERNAME;
    case autofill::SINGLE_USERNAME_FORGOT_PASSWORD:
      return sync_pb::
          FormField_AutofillFieldType_SINGLE_USERNAME_FORGOT_PASSWORD;
    case autofill::SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES:
      return sync_pb::
          FormField_AutofillFieldType_SINGLE_USERNAME_WITH_INTERMEDIATE_VALUES;
    case autofill::ONE_TIME_CODE:
      return sync_pb::FormField_AutofillFieldType_ONE_TIME_CODE;
    case autofill::DRIVERS_LICENSE_EXPIRATION_DATE:
      return sync_pb::
          FormField_AutofillFieldType_DRIVERS_LICENSE_EXPIRATION_DATE;
    case autofill::DRIVERS_LICENSE_ISSUE_DATE:
      return sync_pb::FormField_AutofillFieldType_DRIVERS_LICENSE_ISSUE_DATE;
    case autofill::DRIVERS_LICENSE_NUMBER:
      return sync_pb::FormField_AutofillFieldType_DRIVERS_LICENSE_NUMBER;
    case autofill::DRIVERS_LICENSE_REGION:
      return sync_pb::FormField_AutofillFieldType_DRIVERS_LICENSE_REGION;
    case autofill::PASSPORT_EXPIRATION_DATE:
      return sync_pb::FormField_AutofillFieldType_PASSPORT_EXPIRATION_DATE;
    case autofill::PASSPORT_ISSUE_DATE:
      return sync_pb::FormField_AutofillFieldType_PASSPORT_ISSUE_DATE;
    case autofill::PASSPORT_ISSUING_COUNTRY:
      return sync_pb::FormField_AutofillFieldType_PASSPORT_ISSUING_COUNTRY;
    case autofill::PASSPORT_NUMBER:
      return sync_pb::FormField_AutofillFieldType_PASSPORT_NUMBER;
    case autofill::VEHICLE_LICENSE_PLATE:
      return sync_pb::FormField_AutofillFieldType_VEHICLE_LICENSE_PLATE;
    case autofill::VEHICLE_MAKE:
      return sync_pb::FormField_AutofillFieldType_VEHICLE_MAKE;
    case autofill::VEHICLE_MODEL:
      return sync_pb::FormField_AutofillFieldType_VEHICLE_MODEL;
    case autofill::VEHICLE_PLATE_STATE:
      return sync_pb::FormField_AutofillFieldType_VEHICLE_PLATE_STATE;
    case autofill::VEHICLE_VIN:
      return sync_pb::FormField_AutofillFieldType_VEHICLE_VIN;
    case autofill::VEHICLE_YEAR:
      return sync_pb::FormField_AutofillFieldType_VEHICLE_YEAR;
    case autofill::NATIONAL_ID_CARD_NUMBER:
      return sync_pb::FormField_AutofillFieldType_NATIONAL_ID_CARD_NUMBER;
    case autofill::NATIONAL_ID_CARD_EXPIRATION_DATE:
      return sync_pb::
          FormField_AutofillFieldType_NATIONAL_ID_CARD_EXPIRATION_DATE;
    case autofill::NATIONAL_ID_CARD_ISSUE_DATE:
      return sync_pb::FormField_AutofillFieldType_NATIONAL_ID_CARD_ISSUE_DATE;
    case autofill::NATIONAL_ID_CARD_ISSUING_COUNTRY:
      return sync_pb::
          FormField_AutofillFieldType_NATIONAL_ID_CARD_ISSUING_COUNTRY;
    case autofill::REDRESS_NUMBER:
      return sync_pb::FormField_AutofillFieldType_REDRESS_NUMBER;
    case autofill::KNOWN_TRAVELER_NUMBER:
      return sync_pb::FormField_AutofillFieldType_KNOWN_TRAVELER_NUMBER;
    case autofill::KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE:
      return sync_pb::
          FormField_AutofillFieldType_KNOWN_TRAVELER_NUMBER_EXPIRATION_DATE;
    case autofill::FLIGHT_RESERVATION_FLIGHT_NUMBER:
      return sync_pb::
          FormField_AutofillFieldType_FLIGHT_RESERVATION_FLIGHT_NUMBER;
    case autofill::FLIGHT_RESERVATION_TICKET_NUMBER:
      return sync_pb::
          FormField_AutofillFieldType_FLIGHT_RESERVATION_TICKET_NUMBER;
    case autofill::FLIGHT_RESERVATION_CONFIRMATION_CODE:
      return sync_pb::
          FormField_AutofillFieldType_FLIGHT_RESERVATION_CONFIRMATION_CODE;
    case autofill::ORDER_ID:
      return sync_pb::FormField_AutofillFieldType_ORDER_ID;
    case autofill::ORDER_DATE:
      return sync_pb::FormField_AutofillFieldType_ORDER_DATE;
    case autofill::ORDER_MERCHANT_NAME:
      return sync_pb::FormField_AutofillFieldType_ORDER_MERCHANT_NAME;
    case autofill::SHIPMENT_TRACKING_NUMBER:
      return sync_pb::FormField_AutofillFieldType_SHIPMENT_TRACKING_NUMBER;

    // Non-fillable types handled earlier.
    case autofill::NO_SERVER_DATA:
    case autofill::UNKNOWN_TYPE:
    case autofill::EMPTY_TYPE:
    case autofill::AMBIGUOUS_TYPE:
    case autofill::MERCHANT_EMAIL_SIGNUP:
    case autofill::PRICE:
    case autofill::NUMERIC_QUANTITY:
    case autofill::SEARCH_TERM:
    case autofill::NOT_PASSWORD:
    case autofill::NOT_USERNAME:
    case autofill::NOT_ACCOUNT_CREATION_PASSWORD:
    case autofill::NEW_PASSWORD:
    case autofill::PROBABLY_NEW_PASSWORD:
    case autofill::NOT_NEW_PASSWORD:
    case autofill::FLIGHT_RESERVATION_DEPARTURE_DATE:
    case autofill::MAX_VALID_FIELD_TYPE:
      NOTREACHED();
  }
  NOTREACHED();
}

namespace {
sync_pb::FormField FormFieldToProto(const PageContext::FormField& field) {
  sync_pb::FormField pb_field;
  pb_field.set_id_attribute(base::UTF16ToUTF8(field.id_attribute));
  pb_field.set_name_attribute(base::UTF16ToUTF8(field.name_attribute));
  pb_field.set_form_control_type(field.form_control_type);
  pb_field.set_value(base::UTF16ToUTF8(field.value));
  pb_field.set_form_signature(field.autofill_signature.form_signature.value());
  pb_field.set_field_signature(
      field.autofill_signature.field_signature.value());
  for (sync_pb::FormField_AutofillFieldType type : field.autofill_types) {
    pb_field.add_autofill_types(type);
  }

  return pb_field;
}

PageContext::FormField FormFieldFromProto(const sync_pb::FormField& pb_field) {
  PageContext::FormField field;
  field.id_attribute = base::UTF8ToUTF16(pb_field.id_attribute());
  field.name_attribute = base::UTF8ToUTF16(pb_field.name_attribute());
  field.form_control_type = pb_field.form_control_type();
  field.value = base::UTF8ToUTF16(pb_field.value());
  field.autofill_signature.form_signature =
      autofill::FormSignature(pb_field.form_signature());
  field.autofill_signature.field_signature =
      autofill::FieldSignature(pb_field.field_signature());
  for (int i = 0; i < pb_field.autofill_types_size(); ++i) {
    field.autofill_types.insert(pb_field.autofill_types(i));
  }

  return field;
}

sync_pb::FormFieldInfo FormFieldInfoToProto(
    const PageContext::FormFieldInfo& info) {
  sync_pb::FormFieldInfo pb_info;
  for (const auto& field : info.fields) {
    *pb_info.add_fields() = FormFieldToProto(field);
  }
  return pb_info;
}

PageContext::FormFieldInfo FormFieldInfoFromProto(
    const sync_pb::FormFieldInfo& pb_info) {
  PageContext::FormFieldInfo info;
  for (const auto& pb_field : pb_info.fields()) {
    info.fields.push_back(FormFieldFromProto(pb_field));
  }
  return info;
}

sync_pb::TextFragmentData TextFragmentDataToProto(
    const TextFragmentData& text_fragment) {
  sync_pb::TextFragmentData pb_text_fragment;
  pb_text_fragment.set_text_start(text_fragment.text_start);
  pb_text_fragment.set_text_end(text_fragment.text_end);
  pb_text_fragment.set_prefix(text_fragment.prefix);
  pb_text_fragment.set_suffix(text_fragment.suffix);
  return pb_text_fragment;
}

TextFragmentData TextFragmentDataFromProto(
    const sync_pb::TextFragmentData& pb_text_fragment) {
  return TextFragmentData(pb_text_fragment.text_start(),
                          pb_text_fragment.text_end(),
                          pb_text_fragment.prefix(), pb_text_fragment.suffix());
}

sync_pb::ScrollPosition ScrollPositionToProto(
    const ScrollPosition& scroll_position) {
  sync_pb::ScrollPosition pb_scroll_position;
  if (!scroll_position.text_fragment.IsEmpty()) {
    *pb_scroll_position.mutable_text_fragment() =
        TextFragmentDataToProto(scroll_position.text_fragment);
  }
  return pb_scroll_position;
}

ScrollPosition ScrollPositionFromProto(
    const sync_pb::ScrollPosition& pb_scroll_position) {
  ScrollPosition scroll_position;
  if (pb_scroll_position.has_text_fragment()) {
    scroll_position.text_fragment =
        TextFragmentDataFromProto(pb_scroll_position.text_fragment());
  }
  return scroll_position;
}

}  // namespace

sync_pb::PageContext PageContextToProto(const PageContext& context) {
  sync_pb::PageContext pb_page_context;
  if (!context.form_field_info.fields.empty()) {
    *pb_page_context.mutable_form_field_info() =
        FormFieldInfoToProto(context.form_field_info);
  }
  if (!context.scroll_position.IsEmpty()) {
    *pb_page_context.mutable_scroll_position() =
        ScrollPositionToProto(context.scroll_position);
  }
  return pb_page_context;
}

PageContext PageContextFromProto(const sync_pb::PageContext& pb_page_context) {
  PageContext page_context;
  page_context.form_field_info =
      FormFieldInfoFromProto(pb_page_context.form_field_info());
  if (pb_page_context.has_scroll_position()) {
    page_context.scroll_position =
        ScrollPositionFromProto(pb_page_context.scroll_position());
  }
  return page_context;
}

}  // namespace send_tab_to_self
