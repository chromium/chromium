// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/rationalization_util.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/security_state/core/security_state.h"
#include "url/origin.h"

namespace autofill {

using mojom::SubmissionIndicatorEvent;

namespace {

// Version of the client sent to the server.
constexpr char kClientVersion[] = "6.1.1715.1442/en (GGLL)";
constexpr char kBillingMode[] = "billing";
constexpr char kShippingMode[] = "shipping";

// Default section name for the fields.
constexpr char kDefaultSection[] = "-default";

// Only removing common name prefixes if we have a minimum number of fields and
// a minimum prefix length. These values are chosen to avoid cases such as two
// fields with "address1" and "address2" and be effective against web frameworks
// which prepend prefixes such as "ctl01$ctl00$MainContentRegion$" on all
// fields.
constexpr int kCommonNamePrefixRemovalFieldThreshold = 3;
constexpr int kMinCommonNamePrefixLength = 16;

// Affix removal configuration. Only remove short affixes if they are common
// to all field names and there is at least the minimum number of fields.
// If no affix common to all field names is found, search for a long
// prefix common to a subset of the fields. This case helps include cases of
// prefixes prepended by web frameworks.
//
// Minimum required number of available fields for trying to remove affixes.
constexpr int kCommonNameAffixRemovalFieldNumberThreshold = 3;
// Minimum required length for affixes common to all field names.
constexpr int kMinCommonNameAffixLength = 3;
// Minimum required length for prefixes common to a subset of the field names.
constexpr int kMinCommonNameLongPrefixLength = 16;
// Regex for checking if |parseable_name| is valid after stripping affixes.
constexpr char kParseableNameValidationRe[] = "\\D";

// Returns true if the scheme given by |url| is one for which autofill is
// allowed to activate. By default this only returns true for HTTP and HTTPS.
bool HasAllowedScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() ||
         base::FeatureList::IsEnabled(
             features::kAutofillAllowNonHttpActivation);
}

// Helper for |EncodeUploadRequest()| that creates a bit field corresponding to
// |available_field_types| and returns the hex representation as a string.
std::string EncodeFieldTypes(const ServerFieldTypeSet& available_field_types) {
  // There are |MAX_VALID_FIELD_TYPE| different field types and 8 bits per byte,
  // so we need ceil(MAX_VALID_FIELD_TYPE / 8) bytes to encode the bit field.
  const size_t kNumBytes = (MAX_VALID_FIELD_TYPE + 0x7) / 8;

  // Pack the types in |available_field_types| into |bit_field|.
  std::vector<uint8_t> bit_field(kNumBytes, 0);
  for (const auto& field_type : available_field_types) {
    // Set the appropriate bit in the field.  The bit we set is the one
    // |field_type| % 8 from the left of the byte.
    const size_t byte = field_type / 8;
    const size_t bit = 0x80 >> (field_type % 8);
    DCHECK(byte < bit_field.size());
    bit_field[byte] |= bit;
  }

  // Discard any trailing zeroes.
  // If there are no available types, we return the empty string.
  size_t data_end = bit_field.size();
  for (; data_end > 0 && !bit_field[data_end - 1]; --data_end) {
  }

  // Print all meaningfull bytes into a string.
  std::string data_presence;
  data_presence.reserve(data_end * 2 + 1);
  for (size_t i = 0; i < data_end; ++i) {
    base::StringAppendF(&data_presence, "%02x", bit_field[i]);
  }

  return data_presence;
}

// Returns |true| iff the |token| is a type hint for a contact field, as
// specified in the implementation section of http://is.gd/whatwg_autocomplete
// Note that "fax" and "pager" are intentionally ignored, as Chrome does not
// support filling either type of information.
bool IsContactTypeHint(const std::string& token) {
  return token == "home" || token == "work" || token == "mobile";
}

// Returns |true| iff the |token| is a type hint appropriate for a field of the
// given |field_type|, as specified in the implementation section of
// http://is.gd/whatwg_autocomplete
bool ContactTypeHintMatchesFieldType(const std::string& token,
                                     HtmlFieldType field_type) {
  // The "home" and "work" type hints are only appropriate for email and phone
  // number field types.
  if (token == "home" || token == "work") {
    return field_type == HTML_TYPE_EMAIL ||
           (field_type >= HTML_TYPE_TEL &&
            field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX);
  }

  // The "mobile" type hint is only appropriate for phone number field types.
  // Note that "fax" and "pager" are intentionally ignored, as Chrome does not
  // support filling either type of information.
  if (token == "mobile") {
    return field_type >= HTML_TYPE_TEL &&
           field_type <= HTML_TYPE_TEL_LOCAL_SUFFIX;
  }

  return false;
}

// Returns the Chrome Autofill-supported field type corresponding to the given
// |autocomplete_attribute_value|, if there is one, in the context of the given
// |field|.  Chrome Autofill supports a subset of the field types listed at
// http://is.gd/whatwg_autocomplete
HtmlFieldType FieldTypeFromAutocompleteAttributeValue(
    const std::string& autocomplete_attribute_value,
    const AutofillField& field) {
  if (autocomplete_attribute_value == "")
    return HTML_TYPE_UNSPECIFIED;

  if (autocomplete_attribute_value == "name")
    return HTML_TYPE_NAME;

  if (autocomplete_attribute_value == "honorific-prefix")
    return HTML_TYPE_HONORIFIC_PREFIX;

  if (autocomplete_attribute_value == "given-name" ||
      autocomplete_attribute_value == "given_name" ||
      autocomplete_attribute_value == "first-name" ||
      autocomplete_attribute_value == "first_name")
    return HTML_TYPE_GIVEN_NAME;

  if (autocomplete_attribute_value == "additional-name" ||
      autocomplete_attribute_value == "additional_name") {
    if (field.max_length == 1)
      return HTML_TYPE_ADDITIONAL_NAME_INITIAL;
    return HTML_TYPE_ADDITIONAL_NAME;
  }

  if (autocomplete_attribute_value == "family-name" ||
      autocomplete_attribute_value == "family_name")
    return HTML_TYPE_FAMILY_NAME;

  if (autocomplete_attribute_value == "organization" ||
      autocomplete_attribute_value == "company")
    return HTML_TYPE_ORGANIZATION;

  if (autocomplete_attribute_value == "street-address" ||
      autocomplete_attribute_value == "street_address" ||
      autocomplete_attribute_value == "address")
    return HTML_TYPE_STREET_ADDRESS;

  if (autocomplete_attribute_value == "address-line1" ||
      autocomplete_attribute_value == "address_line1")
    return HTML_TYPE_ADDRESS_LINE1;

  if (autocomplete_attribute_value == "address-line2" ||
      autocomplete_attribute_value == "address_line2")
    return HTML_TYPE_ADDRESS_LINE2;

  if (autocomplete_attribute_value == "address-line3" ||
      autocomplete_attribute_value == "address_line3")
    return HTML_TYPE_ADDRESS_LINE3;

  // TODO(estade): remove support for "locality" and "region".
  if (autocomplete_attribute_value == "locality")
    return HTML_TYPE_ADDRESS_LEVEL2;

  if (autocomplete_attribute_value == "region")
    return HTML_TYPE_ADDRESS_LEVEL1;

  if (autocomplete_attribute_value == "address-level1" ||
      autocomplete_attribute_value == "address_level1")
    return HTML_TYPE_ADDRESS_LEVEL1;

  if (autocomplete_attribute_value == "address-level2" ||
      autocomplete_attribute_value == "address_level2")
    return HTML_TYPE_ADDRESS_LEVEL2;

  if (autocomplete_attribute_value == "address-level3" ||
      autocomplete_attribute_value == "address_level3")
    return HTML_TYPE_ADDRESS_LEVEL3;

  if (autocomplete_attribute_value == "country")
    return HTML_TYPE_COUNTRY_CODE;

  if (autocomplete_attribute_value == "country-name" ||
      autocomplete_attribute_value == "country_name")
    return HTML_TYPE_COUNTRY_NAME;

  if (autocomplete_attribute_value == "postal-code" ||
      autocomplete_attribute_value == "postal_code")
    return HTML_TYPE_POSTAL_CODE;

  // content_switches.h isn't accessible from here, hence we have
  // to copy the string literal. This should be removed soon anyway.
  if (autocomplete_attribute_value == "address" &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          "enable-experimental-web-platform-features")) {
    return HTML_TYPE_FULL_ADDRESS;
  }

  if (autocomplete_attribute_value == "cc-name" ||
      autocomplete_attribute_value == "cc_name")
    return HTML_TYPE_CREDIT_CARD_NAME_FULL;

  if (autocomplete_attribute_value == "cc-given-name" ||
      autocomplete_attribute_value == "cc_given_name")
    return HTML_TYPE_CREDIT_CARD_NAME_FIRST;

  if (autocomplete_attribute_value == "cc-family-name" ||
      autocomplete_attribute_value == "cc_family_name")
    return HTML_TYPE_CREDIT_CARD_NAME_LAST;

  if (autocomplete_attribute_value == "cc-number" ||
      autocomplete_attribute_value == "cc_number")
    return HTML_TYPE_CREDIT_CARD_NUMBER;

  if (autocomplete_attribute_value == "cc-exp" ||
      autocomplete_attribute_value == "cc_exp") {
    if (field.max_length == 5)
      return HTML_TYPE_CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR;
    if (field.max_length == 7)
      return HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR;
    return HTML_TYPE_CREDIT_CARD_EXP;
  }

  if (autocomplete_attribute_value == "cc-exp-month" ||
      autocomplete_attribute_value == "cc_exp_month")
    return HTML_TYPE_CREDIT_CARD_EXP_MONTH;

  if (autocomplete_attribute_value == "cc-exp-year" ||
      autocomplete_attribute_value == "cc_exp_year") {
    if (field.max_length == 2)
      return HTML_TYPE_CREDIT_CARD_EXP_2_DIGIT_YEAR;
    if (field.max_length == 4)
      return HTML_TYPE_CREDIT_CARD_EXP_4_DIGIT_YEAR;
    return HTML_TYPE_CREDIT_CARD_EXP_YEAR;
  }

  if (autocomplete_attribute_value == "cc-csc" ||
      autocomplete_attribute_value == "cc_csc")
    return HTML_TYPE_CREDIT_CARD_VERIFICATION_CODE;

  if (autocomplete_attribute_value == "cc-type" ||
      autocomplete_attribute_value == "cc_type")
    return HTML_TYPE_CREDIT_CARD_TYPE;

  if (autocomplete_attribute_value == "transaction-amount" ||
      autocomplete_attribute_value == "transaction_amount")
    return HTML_TYPE_TRANSACTION_AMOUNT;

  if (autocomplete_attribute_value == "transaction-currency" ||
      autocomplete_attribute_value == "transaction_currency")
    return HTML_TYPE_TRANSACTION_CURRENCY;

  if (autocomplete_attribute_value == "tel" ||
      autocomplete_attribute_value == "phone")
    return HTML_TYPE_TEL;

  if (autocomplete_attribute_value == "tel-country-code" ||
      autocomplete_attribute_value == "phone-country-code" ||
      autocomplete_attribute_value == "tel_country_code" ||
      autocomplete_attribute_value == "phone_country_code")
    return HTML_TYPE_TEL_COUNTRY_CODE;

  if (autocomplete_attribute_value == "tel-national" ||
      autocomplete_attribute_value == "phone-national" ||
      autocomplete_attribute_value == "tel_national" ||
      autocomplete_attribute_value == "phone_national")
    return HTML_TYPE_TEL_NATIONAL;

  if (autocomplete_attribute_value == "tel-area-code" ||
      autocomplete_attribute_value == "phone-area-code" ||
      autocomplete_attribute_value == "tel_area_code" ||
      autocomplete_attribute_value == "phone_area_code")
    return HTML_TYPE_TEL_AREA_CODE;

  if (autocomplete_attribute_value == "tel-local" ||
      autocomplete_attribute_value == "phone-local" ||
      autocomplete_attribute_value == "tel_local" ||
      autocomplete_attribute_value == "phone_local")
    return HTML_TYPE_TEL_LOCAL;

  if (autocomplete_attribute_value == "tel-local-prefix" ||
      autocomplete_attribute_value == "phone-local-prefix" ||
      autocomplete_attribute_value == "tel_local_prefix" ||
      autocomplete_attribute_value == "phone_local_prefix")
    return HTML_TYPE_TEL_LOCAL_PREFIX;

  if (autocomplete_attribute_value == "tel-local-suffix" ||
      autocomplete_attribute_value == "phone-local-suffix" ||
      autocomplete_attribute_value == "tel_local_suffix" ||
      autocomplete_attribute_value == "phone_local_suffix")
    return HTML_TYPE_TEL_LOCAL_SUFFIX;

  if (autocomplete_attribute_value == "tel-extension" ||
      autocomplete_attribute_value == "phone-extension" ||
      autocomplete_attribute_value == "phone-ext" ||
      autocomplete_attribute_value == "tel_extension" ||
      autocomplete_attribute_value == "phone_extension" ||
      autocomplete_attribute_value == "phone_ext")
    return HTML_TYPE_TEL_EXTENSION;

  if (autocomplete_attribute_value == "email" ||
      autocomplete_attribute_value == "username")
    return HTML_TYPE_EMAIL;

  if (autocomplete_attribute_value == "upi-vpa" ||
      autocomplete_attribute_value == "upi_vpa" ||
      autocomplete_attribute_value == "upi")
    return HTML_TYPE_UPI_VPA;

  if (autocomplete_attribute_value == "one-time-code")
    return HTML_TYPE_ONE_TIME_CODE;

  return HTML_TYPE_UNRECOGNIZED;
}

std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryResponse& response) {
  for (const auto& form : response.form_suggestions()) {
    out << "\nForm";
    for (const auto& field : form.field_suggestions()) {
      out << "\n Field\n  signature: " << field.field_signature();
      if (field.has_primary_type_prediction())
        out << "\n  primary_type_prediction: "
            << field.primary_type_prediction();
      for (const auto& prediction : field.predictions())
        out << "\n  prediction: " << prediction.type();
    }
  }
  return out;
}

// Returns true iff all form fields autofill types are in |contained_types|.
bool AllTypesCaptured(const FormStructure& form,
                      const ServerFieldTypeSet& contained_types) {
  for (const auto& field : form) {
    for (const auto& type : field->possible_types()) {
      if (type != UNKNOWN_TYPE && type != EMPTY_TYPE &&
          !contained_types.count(type))
        return false;
    }
  }
  return true;
}

// Encode password attributes and length into |upload|.
void EncodePasswordAttributesVote(
    const std::pair<PasswordAttribute, bool>& password_attributes_vote,
    const size_t password_length_vote,
    const int password_symbol_vote,
    AutofillUploadContents* upload) {
  switch (password_attributes_vote.first) {
    case PasswordAttribute::kHasLowercaseLetter:
      upload->set_password_has_lowercase_letter(
          password_attributes_vote.second);
      break;
    case PasswordAttribute::kHasSpecialSymbol:
      upload->set_password_has_special_symbol(password_attributes_vote.second);
      if (password_attributes_vote.second)
        upload->set_password_special_symbol(password_symbol_vote);
      break;
    case PasswordAttribute::kPasswordAttributesCount:
      NOTREACHED();
  }
  upload->set_password_length(password_length_vote);
}

void EncodeRandomizedValue(const RandomizedEncoder& encoder,
                           FormSignature form_signature,
                           FieldSignature field_signature,
                           base::StringPiece data_type,
                           base::StringPiece data_value,
                           bool include_checksum,
                           AutofillRandomizedValue* output) {
  DCHECK(output);
  output->set_encoding_type(encoder.encoding_type());
  output->set_encoded_bits(
      encoder.Encode(form_signature, field_signature, data_type, data_value));
  if (include_checksum) {
    DCHECK(data_type == RandomizedEncoder::FORM_URL);
    output->set_checksum(StrToHash32Bit(data_value.data()));
  }
}

void EncodeRandomizedValue(const RandomizedEncoder& encoder,
                           FormSignature form_signature,
                           FieldSignature field_signature,
                           base::StringPiece data_type,
                           base::StringPiece16 data_value,
                           bool include_checksum,
                           AutofillRandomizedValue* output) {
  EncodeRandomizedValue(encoder, form_signature, field_signature, data_type,
                        base::UTF16ToUTF8(data_value), include_checksum,
                        output);
}

void PopulateRandomizedFormMetadata(const RandomizedEncoder& encoder,
                                    const FormStructure& form,
                                    AutofillRandomizedFormMetadata* metadata) {
  const FormSignature form_signature = form.form_signature();
  constexpr FieldSignature
      kNullFieldSignature;  // Not relevant for form level metadata.
  if (!form.id_attribute().empty()) {
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::FORM_ID, form.id_attribute(),
                          /*include_checksum=*/false, metadata->mutable_id());
  }
  if (!form.name_attribute().empty()) {
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::FORM_NAME, form.name_attribute(),
                          /*include_checksum=*/false, metadata->mutable_name());
  }

  for (const ButtonTitleInfo& e : form.button_titles()) {
    auto* button_title = metadata->add_button_title();
    DCHECK(!e.first.empty());
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::FORM_BUTTON_TITLES, e.first,
                          /*include_checksum=*/false,
                          button_title->mutable_title());
    button_title->set_type(static_cast<ButtonTitleType>(e.second));
  }
  auto full_source_url = form.full_source_url().spec();
  if (encoder.AnonymousUrlCollectionIsEnabled() && !full_source_url.empty()) {
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::FORM_URL, full_source_url,
                          /*include_checksum=*/true, metadata->mutable_url());
  }
}

void PopulateRandomizedFieldMetadata(
    const RandomizedEncoder& encoder,
    const FormStructure& form,
    const AutofillField& field,
    AutofillRandomizedFieldMetadata* metadata) {
  const FormSignature form_signature = form.form_signature();
  const FieldSignature field_signature = field.GetFieldSignature();
  if (!field.id_attribute.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_ID, field.id_attribute,
                          /*include_checksum=*/false, metadata->mutable_id());
  }
  if (!field.name_attribute.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_NAME, field.name_attribute,
                          /*include_checksum=*/false, metadata->mutable_name());
  }
  if (!field.form_control_type.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_CONTROL_TYPE,
                          field.form_control_type, /*include_checksum=*/false,
                          metadata->mutable_type());
  }
  if (!field.label.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_LABEL, field.label,
                          /*include_checksum=*/false,
                          metadata->mutable_label());
  }
  if (!field.aria_label.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_ARIA_LABEL, field.aria_label,
                          /*include_checksum=*/false,
                          metadata->mutable_aria_label());
  }
  if (!field.aria_description.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_ARIA_DESCRIPTION,
                          field.aria_description, /*include_checksum=*/false,
                          metadata->mutable_aria_description());
  }
  if (!field.css_classes.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_CSS_CLASS, field.css_classes,
                          /*include_checksum=*/false,
                          metadata->mutable_css_class());
  }
  if (!field.placeholder.empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_PLACEHOLDER,
                          field.placeholder, /*include_checksum=*/false,
                          metadata->mutable_placeholder());
  }
}

void EncodeFormMetadataForQuery(const FormStructure& form,
                                AutofillRandomizedFormMetadata* metadata) {
  DCHECK(metadata);
  metadata->mutable_id()->set_encoded_bits(
      base::UTF16ToUTF8(form.id_attribute()));
  metadata->mutable_name()->set_encoded_bits(
      base::UTF16ToUTF8(form.name_attribute()));
}

void EncodeFieldMetadataForQuery(const FormFieldData& field,
                                 AutofillRandomizedFieldMetadata* metadata) {
  DCHECK(metadata);
  metadata->mutable_id()->set_encoded_bits(
      base::UTF16ToUTF8(field.id_attribute));
  metadata->mutable_name()->set_encoded_bits(
      base::UTF16ToUTF8(field.name_attribute));
  metadata->mutable_type()->set_encoded_bits(field.form_control_type);
  metadata->mutable_label()->set_encoded_bits(base::UTF16ToUTF8(field.label));
  metadata->mutable_aria_label()->set_encoded_bits(
      base::UTF16ToUTF8(field.aria_label));
  metadata->mutable_aria_description()->set_encoded_bits(
      base::UTF16ToUTF8(field.aria_description));
  metadata->mutable_css_class()->set_encoded_bits(
      base::UTF16ToUTF8(field.css_classes));
  metadata->mutable_placeholder()->set_encoded_bits(
      base::UTF16ToUTF8(field.placeholder));
}

// Creates the type relationship rules map. The keys represent the type that has
// rules, and the value represents the list of required types for the given
// key. In order to respect the rule, only one of the required types is needed.
// For example, for Autofill to support fields of type
// "PHONE_HOME_COUNTRY_CODE", there would need to be at least one other field
// of type "PHONE_HOME_NUMBER" or "PHONE_HOME_CITY_AND_NUMBER".
const std::unordered_map<ServerFieldType, ServerFieldTypeSet>&
GetTypeRelationshipMap() {
  // Initialized and cached on first use.
  static const auto* const rules =
      new std::unordered_map<ServerFieldType, ServerFieldTypeSet>(
          {{PHONE_HOME_COUNTRY_CODE,
            {PHONE_HOME_NUMBER, PHONE_HOME_CITY_AND_NUMBER}}});
  return *rules;
}

}  // namespace

FormStructure::FormStructure(const FormData& form)
    : id_attribute_(form.id_attribute),
      name_attribute_(form.name_attribute),
      form_name_(form.name),
      button_titles_(form.button_titles),
      source_url_(form.url),
      full_source_url_(form.full_url),
      target_url_(form.action),
      main_frame_origin_(form.main_frame_origin),
      is_form_tag_(form.is_form_tag),
      is_formless_checkout_(form.is_formless_checkout),
      all_fields_are_passwords_(!form.fields.empty()),
      form_parsed_timestamp_(AutofillTickClock::NowTicks()),
      passwords_were_revealed_(false),
      password_symbol_vote_(0),
      developer_engagement_metrics_(0),
      unique_renderer_id_(form.unique_renderer_id) {
  // Copy the form fields.
  std::map<base::string16, size_t> unique_names;
  for (const FormFieldData& field : form.fields) {
    if (!ShouldSkipField(field))
      ++active_field_count_;

    if (field.form_control_type == "password")
      has_password_field_ = true;
    else
      all_fields_are_passwords_ = false;

    // Generate a unique name for this field by appending a counter to the name.
    // Make sure to prepend the counter with a non-numeric digit so that we are
    // guaranteed to avoid collisions.
    base::string16 unique_name =
        field.name + base::ASCIIToUTF16("_") +
        base::NumberToString16(++unique_names[field.name]);
    fields_.push_back(std::make_unique<AutofillField>(field, unique_name));
  }

  form_signature_ = autofill::CalculateFormSignature(form);
  // Do further processing on the fields, as needed.
  ProcessExtractedFields();
}

FormStructure::FormStructure(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures)
    : form_signature_(form_signature) {
  for (const auto& signature : field_signatures)
    fields_.push_back(AutofillField::CreateForPasswordManagerUpload(signature));
}

FormStructure::~FormStructure() = default;

void FormStructure::DetermineHeuristicTypes(LogManager* log_manager) {
  const auto determine_heuristic_types_start_time =
      AutofillTickClock::NowTicks();

  // First, try to detect field types based on each field's |autocomplete|
  // attribute value.
  if (!was_parsed_for_autocomplete_attributes_)
    ParseFieldTypesFromAutocompleteAttributes();

  // Then if there are enough active fields, and if we are dealing with either a
  // proper <form> or a <form>-less checkout, run the heuristics and server
  // prediction routines.
  if (ShouldRunHeuristics()) {
    const FieldCandidatesMap field_type_map = FormField::ParseFormFields(
        fields_, page_language_, is_form_tag_, log_manager);
    for (const auto& field : fields_) {
      const auto iter = field_type_map.find(field->unique_name());
      if (iter != field_type_map.end()) {
        field->set_heuristic_type(iter->second.BestHeuristicType());
      }
    }
  }

  UpdateAutofillCount();
  IdentifySections(has_author_specified_sections_);

  developer_engagement_metrics_ = 0;
  if (IsAutofillable()) {
    AutofillMetrics::DeveloperEngagementMetric metric =
        has_author_specified_types_
            ? AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS
            : AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS;
    developer_engagement_metrics_ |= 1 << metric;
    AutofillMetrics::LogDeveloperEngagementMetric(metric);
  }

  if (has_author_specified_upi_vpa_hint_) {
    AutofillMetrics::LogDeveloperEngagementMetric(
        AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT);
    developer_engagement_metrics_ |=
        1 << AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT;
  }

  RationalizeFieldTypePredictions();

  AutofillMetrics::LogDetermineHeuristicTypesTiming(
      AutofillTickClock::NowTicks() - determine_heuristic_types_start_time);
}

bool FormStructure::EncodeUploadRequest(
    const ServerFieldTypeSet& available_field_types,
    bool form_was_autofilled,
    const std::string& login_form_signature,
    bool observed_submission,
    AutofillUploadContents* upload,
    std::vector<FormSignature>* encoded_signatures) const {
  DCHECK(AllTypesCaptured(*this, available_field_types));
  encoded_signatures->clear();

  upload->set_submission(observed_submission);
  upload->set_client_version(kClientVersion);
  upload->set_form_signature(form_signature().value());
  upload->set_autofill_used(form_was_autofilled);
  upload->set_data_present(EncodeFieldTypes(available_field_types));
  upload->set_passwords_revealed(passwords_were_revealed_);
  upload->set_has_form_tag(is_form_tag_);
  if (!page_language_.empty() && randomized_encoder_ != nullptr) {
    upload->set_language(page_language_);
  }

  auto triggering_event = (submission_event_ != SubmissionIndicatorEvent::NONE)
                              ? submission_event_
                              : ToSubmissionIndicatorEvent(submission_source_);

  DCHECK(autofill::mojom::IsKnownEnumValue(triggering_event));
  upload->set_submission_event(
      static_cast<AutofillUploadContents_SubmissionIndicatorEvent>(
          triggering_event));

  if (password_attributes_vote_) {
    EncodePasswordAttributesVote(*password_attributes_vote_,
                                 password_length_vote_, password_symbol_vote_,
                                 upload);
  }

  if (IsAutofillFieldMetadataEnabled()) {
    upload->set_action_signature(StrToHash64Bit(target_url_.host()));
    if (!form_name().empty())
      upload->set_form_name(base::UTF16ToUTF8(form_name()));
    for (const ButtonTitleInfo& e : button_titles_) {
      auto* button_title = upload->add_button_title();
      button_title->set_title(base::UTF16ToUTF8(e.first));
      button_title->set_type(static_cast<ButtonTitleType>(e.second));
    }
  }

  if (!login_form_signature.empty()) {
    uint64_t login_sig;
    if (base::StringToUint64(login_form_signature, &login_sig))
      upload->set_login_form_signature(login_sig);
  }

  if (IsMalformed())
    return false;  // Malformed form, skip it.

  EncodeFormForUpload(upload, encoded_signatures);
  return true;
}

// static
bool FormStructure::EncodeQueryRequest(
    const std::vector<FormStructure*>& forms,
    AutofillPageQueryRequest* query,
    std::vector<FormSignature>* queried_form_signatures) {
  DCHECK(queried_form_signatures);
  queried_form_signatures->clear();
  queried_form_signatures->reserve(forms.size());

  query->set_client_version(kClientVersion);

  // If a page contains repeated forms, detect that and encode only one form as
  // the returned data would be the same for all the repeated forms.
  // TODO(crbug/1064709#c11): the statement is not entirely correct because
  // (1) distinct forms can have identical form signatures because we truncate
  // (large) numbers in the form signature calculation while these are
  // considered for field signatures; (2) for dynamic forms we will hold on to
  // the original form signature.
  std::set<FormSignature> processed_forms;
  for (const auto* form : forms) {
    if (processed_forms.find(form->form_signature()) != processed_forms.end())
      continue;
    processed_forms.insert(form->form_signature());
    UMA_HISTOGRAM_COUNTS_1000("Autofill.FieldCount", form->field_count());
    if (form->IsMalformed())
      continue;

    form->EncodeFormForQuery(query->add_forms(), queried_form_signatures);
  }

  return !queried_form_signatures->empty();
}

// static
void FormStructure::ParseApiQueryResponse(
    base::StringPiece payload,
    const std::vector<FormStructure*>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  AutofillMetrics::LogServerQueryMetric(
      AutofillMetrics::QUERY_RESPONSE_RECEIVED);

  std::string decoded_payload;
  if (!base::Base64Decode(payload, &decoded_payload)) {
    VLOG(1) << "Could not decode payload from base64 to bytes";
    return;
  }

  // Parse the response.
  AutofillQueryResponse response;
  if (!response.ParseFromString(decoded_payload))
    return;

  VLOG(1) << "Autofill query response from API was successfully parsed: "
          << response;

  ProcessQueryResponse(response, forms, queried_form_signatures,
                       form_interactions_ukm_logger);
}

// static
void FormStructure::ProcessQueryResponse(
    const AutofillQueryResponse& response,
    const std::vector<FormStructure*>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED);

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;

  std::map<std::pair<FormSignature, FieldSignature>,
           AutofillQueryResponse::FormSuggestion::FieldSuggestion>
      field_types;
  for (int form_idx = 0;
       form_idx < std::min(response.form_suggestions_size(),
                           static_cast<int>(queried_form_signatures.size()));
       ++form_idx) {
    FormSignature form_sig = queried_form_signatures.at(form_idx);
    for (const auto& field :
         response.form_suggestions(form_idx).field_suggestions()) {
      FieldSignature field_sig(field.field_signature());
      field_types[std::make_pair(form_sig, field_sig)] = field;
    }
  }

  // Copy the field types into the actual form.
  for (FormStructure* form : forms) {
    bool query_response_has_no_server_data = true;
    for (auto& field : form->fields_) {
      auto it = field_types.find(
          std::make_pair(form->form_signature(), field->GetFieldSignature()));
      if (it == field_types.end())
        continue;

      const auto& current_field = it->second;

      ServerFieldType field_type =
          static_cast<ServerFieldType>(current_field.primary_type_prediction());
      query_response_has_no_server_data &= field_type == NO_SERVER_DATA;

      ServerFieldType heuristic_type = field->heuristic_type();
      if (heuristic_type != UNKNOWN_TYPE)
        heuristics_detected_fillable_field = true;

      field->set_server_type(field_type);
      std::vector<AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                      FieldPrediction>
          server_predictions;
      if (current_field.predictions_size() == 0) {
        AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
            field_prediction;
        field_prediction.set_type(field_type);
        server_predictions.push_back(field_prediction);
      } else {
        server_predictions.assign(current_field.predictions().begin(),
                                  current_field.predictions().end());
      }
      field->set_server_predictions(std::move(server_predictions));
      field->set_may_use_prefilled_placeholder(
          current_field.may_use_prefilled_placeholder());

      if (heuristic_type != field->Type().GetStorableType())
        query_response_overrode_heuristics = true;

      if (current_field.has_password_requirements())
        field->SetPasswordRequirements(current_field.password_requirements());
    }

    AutofillMetrics::LogServerResponseHasDataForForm(
        !query_response_has_no_server_data);

    form->UpdateAutofillCount();
    form->RationalizeRepeatedFields(form_interactions_ukm_logger);
    form->RationalizeFieldTypePredictions();
    form->OverrideServerPredictionsWithHeuristics();
    form->IdentifySections(false);
  }

  AutofillMetrics::ServerQueryMetric metric;
  if (query_response_overrode_heuristics) {
    if (heuristics_detected_fillable_field) {
      metric = AutofillMetrics::QUERY_RESPONSE_OVERRODE_LOCAL_HEURISTICS;
    } else {
      metric = AutofillMetrics::QUERY_RESPONSE_WITH_NO_LOCAL_HEURISTICS;
    }
  } else {
    metric = AutofillMetrics::QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS;
  }
  AutofillMetrics::LogServerQueryMetric(metric);
}

// static
std::vector<FormDataPredictions> FormStructure::GetFieldTypePredictions(
    const std::vector<FormStructure*>& form_structures) {
  std::vector<FormDataPredictions> forms;
  forms.reserve(form_structures.size());
  for (const FormStructure* form_structure : form_structures) {
    FormDataPredictions form;
    form.data = form_structure->ToFormData();
    form.signature = form_structure->FormSignatureAsStr();

    for (const auto& field : form_structure->fields_) {
      FormFieldDataPredictions annotated_field;
      annotated_field.signature = field->FieldSignatureAsStr();
      annotated_field.heuristic_type =
          AutofillType(field->heuristic_type()).ToString();
      annotated_field.server_type =
          AutofillType(field->server_type()).ToString();
      annotated_field.overall_type = field->Type().ToString();
      annotated_field.parseable_name =
          base::UTF16ToUTF8(field->parseable_name());
      annotated_field.section = field->section;
      form.fields.push_back(annotated_field);
    }

    forms.push_back(form);
  }
  return forms;
}

// static
bool FormStructure::IsAutofillFieldMetadataEnabled() {
  const std::string group_name =
      base::FieldTrialList::FindFullName("AutofillFieldMetadata");
  return base::StartsWith(group_name, "Enabled", base::CompareCase::SENSITIVE);
}

std::unique_ptr<FormStructure> FormStructure::CreateForPasswordManagerUpload(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures) {
  std::unique_ptr<FormStructure> form;
  form.reset(new FormStructure(form_signature, field_signatures));
  return form;
}

std::string FormStructure::FormSignatureAsStr() const {
  return base::NumberToString(form_signature().value());
}

bool FormStructure::IsAutofillable() const {
  size_t min_required_fields =
      std::min({MinRequiredFieldsForHeuristics(), MinRequiredFieldsForQuery(),
                MinRequiredFieldsForUpload()});
  if (autofill_count() < min_required_fields)
    return false;

  return ShouldBeParsed();
}

bool FormStructure::IsCompleteCreditCardForm() const {
  bool found_cc_number = false;
  bool found_cc_expiration = false;
  for (const auto& field : fields_) {
    ServerFieldType type = field->Type().GetStorableType();
    if (!found_cc_expiration && data_util::IsCreditCardExpirationType(type)) {
      found_cc_expiration = true;
    } else if (!found_cc_number && type == CREDIT_CARD_NUMBER) {
      found_cc_number = true;
    }
    if (found_cc_expiration && found_cc_number)
      return true;
  }
  return false;
}

void FormStructure::UpdateAutofillCount() {
  autofill_count_ = 0;
  for (const auto& field : *this) {
    if (field && field->IsFieldFillable())
      ++autofill_count_;
  }
}

bool FormStructure::ShouldBeParsed(LogManager* log_manager) const {
  // Exclude URLs not on the web via HTTP(S).
  if (!HasAllowedScheme(source_url_)) {
    if (log_manager) {
      log_manager->Log() << LoggingScope::kAbortParsing
                         << LogMessage::kAbortParsingNotAllowedScheme << *this;
    }
    return false;
  }

  size_t min_required_fields =
      std::min({MinRequiredFieldsForHeuristics(), MinRequiredFieldsForQuery(),
                MinRequiredFieldsForUpload()});
  if (active_field_count() < min_required_fields &&
      (!all_fields_are_passwords() ||
       active_field_count() < kRequiredFieldsForFormsWithOnlyPasswordFields) &&
      !has_author_specified_types_) {
    if (log_manager) {
      log_manager->Log() << LoggingScope::kAbortParsing
                         << LogMessage::kAbortParsingNotEnoughFields
                         << active_field_count() << *this;
    }
    return false;
  }

  // Rule out search forms.
  static const base::string16 kUrlSearchActionPattern =
      base::UTF8ToUTF16(kUrlSearchActionRe);
  if (MatchesPattern(base::UTF8ToUTF16(target_url_.path_piece()),
                     kUrlSearchActionPattern)) {
    if (log_manager) {
      log_manager->Log() << LoggingScope::kAbortParsing
                         << LogMessage::kAbortParsingUrlMatchesSearchRegex
                         << *this;
    }
    return false;
  }

  bool has_text_field = false;
  for (const auto& it : *this) {
    has_text_field |= it->form_control_type != "select-one";
  }

  if (!has_text_field && log_manager) {
    log_manager->Log() << LoggingScope::kAbortParsing
                       << LogMessage::kAbortParsingFormHasNoTextfield << *this;
  }

  return has_text_field;
}

bool FormStructure::ShouldRunHeuristics() const {
  return active_field_count() >= MinRequiredFieldsForHeuristics() &&
         HasAllowedScheme(source_url_) &&
         (is_form_tag_ || is_formless_checkout_ ||
          !base::FeatureList::IsEnabled(
              features::kAutofillRestrictUnownedFieldsToFormlessCheckout));
}

bool FormStructure::ShouldBeQueried() const {
  return (has_password_field_ ||
          active_field_count() >= MinRequiredFieldsForQuery()) &&
         ShouldBeParsed();
}

bool FormStructure::ShouldBeUploaded() const {
  return active_field_count() >= MinRequiredFieldsForUpload() &&
         ShouldBeParsed();
}

void FormStructure::RetrieveFromCache(
    const FormStructure& cached_form,
    const bool should_keep_cached_value,
    const bool only_server_and_autofill_state) {
  // TODO(crbug/1101631) Clean up once the experiment is over.
  const bool kUseRendererIds = base::FeatureList::IsEnabled(
      features::kAutofillRetrieveFromCacheWithRendererIds);
  std::map<base::string16, const AutofillField*> cached_fields_by_name;
  std::map<FieldRendererId, const AutofillField*> cached_fields_by_id;
  for (size_t i = 0; i < cached_form.field_count(); ++i) {
    auto* const field = cached_form.field(i);
    if (kUseRendererIds)
      cached_fields_by_id[field->unique_renderer_id] = field;
    else
      cached_fields_by_name[field->unique_name()] = field;
  }
  for (auto& field : *this) {
    const AutofillField* cached_field = nullptr;
    if (kUseRendererIds) {
      const auto& it = cached_fields_by_id.find(field->unique_renderer_id);
      if (it != cached_fields_by_id.end())
        cached_field = it->second;
    } else {
      const auto& it = cached_fields_by_name.find(field->unique_name());
      if (it != cached_fields_by_name.end())
        cached_field = it->second;
    }

    // If the unique renderer id (or the name) is not stable due to some Java
    // Script magic in the website, use the field signature as a fallback
    // solution to find the field in the cached form.
    // TODO(crbug.com/1125624): Remove feature check once trial ended.
    if (!cached_field &&
        base::FeatureList::IsEnabled(
            features::kAutofillRetrieveFromCacheWithFieldSignatureAsFallback)) {
      // Iterates over the fields to find the field with the same form
      // signature.
      for (size_t i = 0; i < cached_form.field_count(); ++i) {
        auto* const cfield = cached_form.field(i);
        if (field->GetFieldSignature() == cfield->GetFieldSignature()) {
          // If there are multiple matches, do not retrieve the field and stop
          // the process.
          if (cached_field) {
            cached_field = nullptr;
            break;
          } else {
            cached_field = cfield;
          }
        }
      }
    }

    if (cached_field) {
      if (!only_server_and_autofill_state) {
        // Transfer attributes of the cached AutofillField to the newly created
        // AutofillField.
        field->set_heuristic_type(cached_field->heuristic_type());
        field->SetHtmlType(cached_field->html_type(),
                           cached_field->html_mode());
        field->section = cached_field->section;
        field->set_only_fill_when_focused(
            cached_field->only_fill_when_focused());
      }
      if (should_keep_cached_value) {
        field->is_autofilled = cached_field->is_autofilled;
      }
      if (field->form_control_type != "select-one") {
        bool is_credit_card_field =
            AutofillType(cached_field->Type().GetStorableType()).group() ==
            CREDIT_CARD;
        if (should_keep_cached_value &&
            (is_credit_card_field ||
             base::FeatureList::IsEnabled(
                 features::kAutofillKeepInitialFormValuesInCache))) {
          field->value = cached_field->value;
          value_from_dynamic_change_form_ = true;
        } else if (field->value == cached_field->value &&
                   (!base::FeatureList::IsEnabled(
                        features::
                            kAutofillImportPrefilledCountryAndStateValues) ||
                    (field->server_type() != ADDRESS_HOME_COUNTRY &&
                     field->server_type() != ADDRESS_HOME_STATE))) {
          // TODO(crbug.com/1100231): Remove feature check once launched.
          // From the perspective of learning user data, text fields containing
          // default values are equivalent to empty fields.
          field->value = base::string16();
        }
      }
      field->set_server_type(cached_field->server_type());
      field->set_previously_autofilled(cached_field->previously_autofilled());
    }
  }

  UpdateAutofillCount();

  // Update form parsed timestamp
  form_parsed_timestamp_ =
      std::min(form_parsed_timestamp_, cached_form.form_parsed_timestamp_);

  // The form signature should match between query and upload requests to the
  // server. On many websites, form elements are dynamically added, removed, or
  // rearranged via JavaScript between page load and form submission, so we
  // copy over the |form_signature_field_names_| corresponding to the query
  // request.
  form_signature_ = cached_form.form_signature_;
}

void FormStructure::LogQualityMetrics(
    const base::TimeTicks& load_time,
    const base::TimeTicks& interaction_time,
    const base::TimeTicks& submission_time,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    bool did_show_suggestions,
    bool observed_submission) const {
  // Use the same timestamp on UKM Metrics generated within this method's scope.
  AutofillMetrics::UkmTimestampPin timestamp_pin(form_interactions_ukm_logger);

  size_t num_detected_field_types = 0;
  size_t num_edited_autofilled_fields = 0;
  bool did_autofill_all_possible_fields = true;
  bool did_autofill_some_possible_fields = false;
  bool is_for_credit_card = IsCompleteCreditCardForm();
  bool has_upi_vpa_field = false;

  // Determine the correct suffix for the metric, depending on whether or
  // not a submission was observed.
  const AutofillMetrics::QualityMetricType metric_type =
      observed_submission ? AutofillMetrics::TYPE_SUBMISSION
                          : AutofillMetrics::TYPE_NO_SUBMISSION;

  for (size_t i = 0; i < field_count(); ++i) {
    auto* const field = this->field(i);
    if (IsUPIVirtualPaymentAddress(field->value)) {
      has_upi_vpa_field = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::USER_DID_ENTER_UPI_VPA, field->Type().group(),
          security_state::SecurityLevel::SECURITY_LEVEL_COUNT,
          data_util::DetermineGroups(*this));
    }

    form_interactions_ukm_logger->LogFieldFillStatus(*this, *field,
                                                     metric_type);

    AutofillMetrics::LogHeuristicPredictionQualityMetrics(
        form_interactions_ukm_logger, *this, *field, metric_type);
    AutofillMetrics::LogServerPredictionQualityMetrics(
        form_interactions_ukm_logger, *this, *field, metric_type);
    AutofillMetrics::LogOverallPredictionQualityMetrics(
        form_interactions_ukm_logger, *this, *field, metric_type);
    // We count fields that were autofilled but later modified, regardless of
    // whether the data now in the field is recognized.
    if (field->previously_autofilled())
      num_edited_autofilled_fields++;

    const ServerFieldTypeSet& field_types = field->possible_types();
    DCHECK(!field_types.empty());
    if (field_types.count(EMPTY_TYPE) || field_types.count(UNKNOWN_TYPE)) {
      DCHECK_EQ(field_types.size(), 1u);
      continue;
    }

    ++num_detected_field_types;
    if (field->is_autofilled)
      did_autofill_some_possible_fields = true;
    else if (!field->only_fill_when_focused())
      did_autofill_all_possible_fields = false;

    // If the form was submitted, record if field types have been filled and
    // subsequently edited by the user.
    if (observed_submission) {
      if (field->is_autofilled || field->previously_autofilled()) {
        AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
            form_interactions_ukm_logger, *this, *field);
      }
    }
  }

  AutofillMetrics::LogNumberOfEditedAutofilledFields(
      num_edited_autofilled_fields, observed_submission);

  // We log "submission" and duration metrics if we are here after observing a
  // submission event.
  if (observed_submission) {
    AutofillMetrics::AutofillFormSubmittedState state;
    if (num_detected_field_types < MinRequiredFieldsForHeuristics() &&
        num_detected_field_types < MinRequiredFieldsForQuery()) {
      state = AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA;
    } else {
      if (did_autofill_all_possible_fields) {
        state = AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL;
      } else if (did_autofill_some_possible_fields) {
        state = AutofillMetrics::FILLABLE_FORM_AUTOFILLED_SOME;
      } else if (!did_show_suggestions) {
        state = AutofillMetrics::
            FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS;
      } else {
        state =
            AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS;
      }

      // Unlike the other times, the |submission_time| should always be
      // available.
      DCHECK(!submission_time.is_null());

      // The |load_time| might be unset, in the case that the form was
      // dynamically added to the DOM.
      if (!load_time.is_null()) {
        // Submission should always chronologically follow form load.
        DCHECK_GE(submission_time, load_time);
        base::TimeDelta elapsed = submission_time - load_time;
        if (did_autofill_some_possible_fields)
          AutofillMetrics::LogFormFillDurationFromLoadWithAutofill(elapsed);
        else
          AutofillMetrics::LogFormFillDurationFromLoadWithoutAutofill(elapsed);
      }

      // The |interaction_time| might be unset, in the case that the user
      // submitted a blank form.
      if (!interaction_time.is_null()) {
        // Submission should always chronologically follow interaction.
        DCHECK(submission_time > interaction_time);
        base::TimeDelta elapsed = submission_time - interaction_time;
        AutofillMetrics::LogFormFillDurationFromInteraction(
            GetFormTypes(), did_autofill_some_possible_fields, elapsed);
      }
    }

    AutofillMetrics::LogAutofillFormSubmittedState(
        state, is_for_credit_card, has_upi_vpa_field, GetFormTypes(),
        form_parsed_timestamp_, form_signature(), form_interactions_ukm_logger);
  }
}

void FormStructure::LogQualityMetricsBasedOnAutocomplete(
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger)
    const {
  const AutofillMetrics::QualityMetricType metric_type =
      AutofillMetrics::TYPE_AUTOCOMPLETE_BASED;
  for (const auto& field : fields_) {
    if (field->html_type() != HTML_TYPE_UNSPECIFIED &&
        field->html_type() != HTML_TYPE_UNRECOGNIZED) {
      AutofillMetrics::LogHeuristicPredictionQualityMetrics(
          form_interactions_ukm_logger, *this, *field, metric_type);
      AutofillMetrics::LogServerPredictionQualityMetrics(
          form_interactions_ukm_logger, *this, *field, metric_type);
    }
  }
}

void FormStructure::ParseFieldTypesFromAutocompleteAttributes() {
  has_author_specified_types_ = false;
  has_author_specified_sections_ = false;
  has_author_specified_upi_vpa_hint_ = false;
  for (const std::unique_ptr<AutofillField>& field : fields_) {
    // To prevent potential section name collisions, add a default suffix for
    // other fields.  Without this, 'autocomplete' attribute values
    // "section--shipping street-address" and "shipping street-address" would be
    // parsed identically, given the section handling code below.  We do this
    // before any validation so that fields with invalid attributes still end up
    // in the default section.  These default section names will be overridden
    // by subsequent heuristic parsing steps if there are no author-specified
    // section names.
    field->section = kDefaultSection;

    std::vector<std::string> tokens =
        LowercaseAndTokenizeAttributeString(field->autocomplete_attribute);

    // The autocomplete attribute is overloaded: it can specify either a field
    // type hint or whether autocomplete should be enabled at all.  Ignore the
    // latter type of attribute value.
    if (tokens.empty() ||
        (tokens.size() == 1 &&
         (tokens[0] == "on" || tokens[0] == "off" || tokens[0] == "false"))) {
      continue;
    }

    // Any other value, even it is invalid, is considered to be a type hint.
    // This allows a website's author to specify an attribute like
    // autocomplete="other" on a field to disable all Autofill heuristics for
    // the form.
    has_author_specified_types_ = true;

    // Per the spec, the tokens are parsed in reverse order. The expected
    // pattern is:
    // [section-*] [shipping|billing] [type_hint] field_type

    // (1) The final token must be the field type. If it is not one of the known
    // types, abort.
    std::string field_type_token = tokens.back();
    tokens.pop_back();
    HtmlFieldType field_type =
        FieldTypeFromAutocompleteAttributeValue(field_type_token, *field);
    if (field_type == HTML_TYPE_UPI_VPA) {
      has_author_specified_upi_vpa_hint_ = true;
      // TODO(crbug.com/702223): Flesh out support for UPI-VPA.
      field_type = HTML_TYPE_UNRECOGNIZED;
    }
    if (field_type == HTML_TYPE_UNSPECIFIED)
      continue;

    // (2) The preceding token, if any, may be a type hint.
    if (!tokens.empty() && IsContactTypeHint(tokens.back())) {
      // If it is, it must match the field type; otherwise, abort.
      // Note that an invalid token invalidates the entire attribute value, even
      // if the other tokens are valid.
      if (!ContactTypeHintMatchesFieldType(tokens.back(), field_type))
        continue;

      // Chrome Autofill ignores these type hints.
      tokens.pop_back();
    }

    DCHECK_EQ(kDefaultSection, field->section);
    std::string section = field->section;
    HtmlFieldMode mode = HTML_MODE_NONE;

    // (3) The preceding token, if any, may be a fixed string that is either
    // "shipping" or "billing".  Chrome Autofill treats these as implicit
    // section name suffixes.
    if (!tokens.empty()) {
      if (tokens.back() == kShippingMode)
        mode = HTML_MODE_SHIPPING;
      else if (tokens.back() == kBillingMode)
        mode = HTML_MODE_BILLING;

      if (mode != HTML_MODE_NONE) {
        section = "-" + tokens.back();
        tokens.pop_back();
      }
    }

    // (4) The preceding token, if any, may be a named section.
    const base::StringPiece kSectionPrefix = "section-";
    if (!tokens.empty() && base::StartsWith(tokens.back(), kSectionPrefix,
                                            base::CompareCase::SENSITIVE)) {
      // Prepend this section name to the suffix set in the preceding block.
      section = tokens.back().substr(kSectionPrefix.size()) + section;
      tokens.pop_back();
    }

    // (5) No other tokens are allowed.  If there are any remaining, abort.
    if (!tokens.empty())
      continue;

    if (section != kDefaultSection) {
      has_author_specified_sections_ = true;
      field->section = section;
    }

    // No errors encountered while parsing!
    // Update the |field|'s type based on what was parsed from the attribute.
    field->SetHtmlType(field_type, mode);
  }

  was_parsed_for_autocomplete_attributes_ = true;
}

std::set<base::string16> FormStructure::PossibleValues(ServerFieldType type) {
  std::set<base::string16> values;
  AutofillType target_type(type);
  for (const auto& field : fields_) {
    if (field->Type().GetStorableType() != target_type.GetStorableType() ||
        field->Type().group() != target_type.group()) {
      continue;
    }

    // No option values; anything goes.
    if (field->option_values.empty()) {
      values.clear();
      break;
    }

    for (const base::string16& val : field->option_values) {
      if (!val.empty())
        values.insert(base::i18n::ToUpper(val));
    }

    for (const base::string16& content : field->option_contents) {
      if (!content.empty())
        values.insert(base::i18n::ToUpper(content));
    }
  }

  return values;
}

const AutofillField* FormStructure::field(size_t index) const {
  if (index >= fields_.size()) {
    NOTREACHED();
    return nullptr;
  }

  return fields_[index].get();
}

AutofillField* FormStructure::field(size_t index) {
  return const_cast<AutofillField*>(
      static_cast<const FormStructure*>(this)->field(index));
}

size_t FormStructure::field_count() const {
  return fields_.size();
}

size_t FormStructure::active_field_count() const {
  return active_field_count_;
}

FormData FormStructure::ToFormData() const {
  FormData data;
  data.id_attribute = id_attribute_;
  data.name_attribute = name_attribute_;
  data.name = form_name_;
  data.button_titles = button_titles_;
  data.url = source_url_;
  data.full_url = full_source_url_;
  data.action = target_url_;
  data.main_frame_origin = main_frame_origin_;
  data.is_form_tag = is_form_tag_;
  data.is_formless_checkout = is_formless_checkout_;
  data.unique_renderer_id = unique_renderer_id_;

  for (size_t i = 0; i < fields_.size(); ++i) {
    data.fields.push_back(FormFieldData(*fields_[i]));
  }

  return data;
}

FormStructure::SectionedFieldsIndexes::SectionedFieldsIndexes() {}

FormStructure::SectionedFieldsIndexes::~SectionedFieldsIndexes() {}

void FormStructure::RationalizeCreditCardFieldPredictions() {
  bool cc_first_name_found = false;
  bool cc_last_name_found = false;
  bool cc_num_found = false;
  bool cc_month_found = false;
  bool cc_year_found = false;
  bool cc_type_found = false;
  bool cc_cvc_found = false;
  size_t num_months_found = 0;
  size_t num_other_fields_found = 0;
  for (const auto& field : fields_) {
    ServerFieldType current_field_type =
        field->ComputedType().GetStorableType();
    switch (current_field_type) {
      case CREDIT_CARD_NAME_FIRST:
        cc_first_name_found = true;
        break;
      case CREDIT_CARD_NAME_LAST:
        cc_last_name_found = true;
        break;
      case CREDIT_CARD_NAME_FULL:
        cc_first_name_found = true;
        cc_last_name_found = true;
        break;
      case CREDIT_CARD_NUMBER:
        cc_num_found = true;
        break;
      case CREDIT_CARD_EXP_MONTH:
        cc_month_found = true;
        ++num_months_found;
        break;
      case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        cc_year_found = true;
        break;
      case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
        cc_month_found = true;
        cc_year_found = true;
        ++num_months_found;
        break;
      case CREDIT_CARD_TYPE:
        cc_type_found = true;
        break;
      case CREDIT_CARD_VERIFICATION_CODE:
        cc_cvc_found = true;
        break;
      case ADDRESS_HOME_ZIP:
      case ADDRESS_BILLING_ZIP:
        // Zip/Postal code often appears as part of a Credit Card form. Do
        // not count it as a non-cc-related field.
        break;
      default:
        ++num_other_fields_found;
    }
  }

  // A partial CC name is unlikely. Prefer to consider these profile names
  // when partial.
  bool cc_name_found = cc_first_name_found && cc_last_name_found;

  // A partial CC expiry date should not be filled. These are often confused
  // with quantity/height fields and/or generic year fields.
  bool cc_date_found = cc_month_found && cc_year_found;

  // Count the credit card related fields in the form.
  size_t num_cc_fields_found =
      static_cast<int>(cc_name_found) + static_cast<int>(cc_num_found) +
      static_cast<int>(cc_date_found) + static_cast<int>(cc_type_found) +
      static_cast<int>(cc_cvc_found);

  // Retain credit card related fields if the form has multiple fields or has
  // no unrelated fields (useful for single cc-field forms). Credit card number
  // is permitted to be alone in an otherwise unrelated form because some
  // dynamic forms reveal the remainder of the fields only after the credit
  // card number is entered and identified as a credit card by the site.
  bool keep_cc_fields =
      cc_num_found || num_cc_fields_found >= 3 || num_other_fields_found == 0;

  // Do an update pass over the fields to rewrite the types if credit card
  // fields are not to be retained. Some special handling is given to expiry
  // dates if the full date is not found or multiple expiry date fields are
  // found. See comments inline below.
  for (auto it = fields_.begin(); it != fields_.end(); ++it) {
    auto& field = *it;
    ServerFieldType current_field_type = field->Type().GetStorableType();
    switch (current_field_type) {
      case CREDIT_CARD_NAME_FIRST:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(NAME_FIRST));
        break;
      case CREDIT_CARD_NAME_LAST:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(NAME_LAST));
        break;
      case CREDIT_CARD_NAME_FULL:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(NAME_FULL));
        break;
      case CREDIT_CARD_NUMBER:
      case CREDIT_CARD_TYPE:
      case CREDIT_CARD_VERIFICATION_CODE:
      case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
        if (!keep_cc_fields)
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
        break;
      case CREDIT_CARD_EXP_MONTH:
        // Do not preserve an expiry month prediction if any of the following
        // are true:
        //   (1) the form is determined to be be non-cc related, so all cc
        //       field predictions are to be discarded
        //   (2) the expiry month was found without a corresponding year
        //   (3) multiple month fields were found in a form having a full
        //       expiry date. This usually means the form is a checkout form
        //       that also has one or more quantity fields. Suppress the expiry
        //       month field(s) not immediately preceding an expiry year field.
        if (!keep_cc_fields || !cc_date_found) {
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
        } else if (num_months_found > 1) {
          auto it2 = it + 1;
          if (it2 == fields_.end()) {
            field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
          } else {
            ServerFieldType next_field_type = (*it2)->Type().GetStorableType();
            if (next_field_type != CREDIT_CARD_EXP_2_DIGIT_YEAR &&
                next_field_type != CREDIT_CARD_EXP_4_DIGIT_YEAR) {
              field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
            }
          }
        }
        break;
      case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        if (!keep_cc_fields || !cc_date_found)
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
        break;
      default:
        break;
    }
  }
}

void FormStructure::RationalizePhoneNumbersInSection(std::string section) {
  if (phone_rationalized_[section])
    return;
  std::vector<AutofillField*> fields;
  for (size_t i = 0; i < field_count(); ++i) {
    if (field(i)->section != section)
      continue;
    fields.push_back(field(i));
  }
  rationalization_util::RationalizePhoneNumberFields(fields);
  phone_rationalized_[section] = true;
}

void FormStructure::ApplyRationalizationsToFieldAndLog(
    size_t field_index,
    ServerFieldType new_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  if (field_index >= fields_.size())
    return;
  auto old_type = fields_[field_index]->Type().GetStorableType();
  fields_[field_index]->SetTypeTo(AutofillType(new_type));
  if (form_interactions_ukm_logger) {
    form_interactions_ukm_logger->LogRepeatedServerTypePredictionRationalized(
        form_signature_, *fields_[field_index], old_type);
  }
}

void FormStructure::RationalizeAddressLineFields(
    SectionedFieldsIndexes* sections_of_address_indexes,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  // The rationalization happens within sections.
  for (sections_of_address_indexes->Reset();
       !sections_of_address_indexes->IsFinished();
       sections_of_address_indexes->WalkForwardToTheNextSection()) {
    auto current_section = sections_of_address_indexes->CurrentSection();

    // The rationalization only applies to sections that have 2 or 3 visible
    // street address predictions.
    if (current_section.size() != 2 && current_section.size() != 3) {
      continue;
    }

    int nb_address_rationalized = 0;
    for (auto field_index : current_section) {
      switch (nb_address_rationalized) {
        case 0:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE1,
                                             form_interactions_ukm_logger);
          break;
        case 1:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE2,
                                             form_interactions_ukm_logger);
          break;
        case 2:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE3,
                                             form_interactions_ukm_logger);
          break;
        default:
          NOTREACHED();
          break;
      }
      ++nb_address_rationalized;
    }
  }
}

void FormStructure::ApplyRationalizationsToHiddenSelects(
    size_t field_index,
    ServerFieldType new_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  ServerFieldType old_type = fields_[field_index]->Type().GetStorableType();

  // Walk on the hidden select fields right after the field_index which share
  // the same type with the field_index, and apply the rationalization to them
  // as well. These fields, if any, function as one field with the field_index.
  for (auto current_index = field_index + 1; current_index < fields_.size();
       current_index++) {
    if (fields_[current_index]->IsVisible() ||
        fields_[current_index]->form_control_type != "select-one" ||
        fields_[current_index]->Type().GetStorableType() != old_type)
      break;
    ApplyRationalizationsToFieldAndLog(current_index, new_type,
                                       form_interactions_ukm_logger);
  }

  // Same for the fields coming right before the field_index. (No need to check
  // for the fields appearing before the first field!)
  if (field_index == 0)
    return;
  for (auto current_index = field_index - 1;; current_index--) {
    if (fields_[current_index]->IsVisible() ||
        fields_[current_index]->form_control_type != "select-one" ||
        fields_[current_index]->Type().GetStorableType() != old_type)
      break;
    ApplyRationalizationsToFieldAndLog(current_index, new_type,
                                       form_interactions_ukm_logger);
    if (current_index == 0)
      break;
  }
}

bool FormStructure::HeuristicsPredictionsAreApplicable(
    size_t upper_index,
    size_t lower_index,
    ServerFieldType first_type,
    ServerFieldType second_type) {
  // The predictions are applicable if one field has one of the two types, and
  // the other has the other type.
  if (fields_[upper_index]->heuristic_type() ==
      fields_[lower_index]->heuristic_type())
    return false;
  if ((fields_[upper_index]->heuristic_type() == first_type ||
       fields_[upper_index]->heuristic_type() == second_type) &&
      (fields_[lower_index]->heuristic_type() == first_type ||
       fields_[lower_index]->heuristic_type() == second_type))
    return true;
  return false;
}

void FormStructure::ApplyRationalizationsToFields(
    size_t upper_index,
    size_t lower_index,
    ServerFieldType upper_type,
    ServerFieldType lower_type,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  // Hidden fields are ignored during the rationalization, but 'select' hidden
  // fields also get autofilled to support their corresponding visible
  // 'synthetic fields'. So, if a field's type is rationalized, we should make
  // sure that the rationalization is also applied to its corresponding hidden
  // fields, if any.
  ApplyRationalizationsToHiddenSelects(upper_index, upper_type,
                                       form_interactions_ukm_logger);
  ApplyRationalizationsToFieldAndLog(upper_index, upper_type,
                                     form_interactions_ukm_logger);

  ApplyRationalizationsToHiddenSelects(lower_index, lower_type,
                                       form_interactions_ukm_logger);
  ApplyRationalizationsToFieldAndLog(lower_index, lower_type,
                                     form_interactions_ukm_logger);
}

bool FormStructure::FieldShouldBeRationalizedToCountry(size_t upper_index) {
  // Upper field is country if and only if it's the first visible address field
  // in its section. Otherwise, the upper field is a state, and the lower one
  // is a country.
  for (int field_index = upper_index - 1; field_index >= 0; --field_index) {
    if (fields_[field_index]->IsVisible() &&
        AutofillType(fields_[field_index]->Type().GetStorableType()).group() ==
            ADDRESS_HOME &&
        fields_[field_index]->section == fields_[upper_index]->section) {
      return false;
    }
  }
  return true;
}

void FormStructure::RationalizeAddressStateCountry(
    SectionedFieldsIndexes* sections_of_state_indexes,
    SectionedFieldsIndexes* sections_of_country_indexes,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  // Walk on the sections of state and country indexes simultaneously. If they
  // both point to the same section, it means that that section includes both
  // the country and the state type. This means that no that rationalization is
  // needed. So, walk both pointers forward. Otherwise, look at the section that
  // appears earlier on the form. That section doesn't have any field of the
  // other type. Rationalize the fields on the earlier section if needed. Walk
  // the pointer that points to the earlier section forward. Stop when both
  // sections of indexes are processed. (This resembles the merge in the merge
  // sort.)
  sections_of_state_indexes->Reset();
  sections_of_country_indexes->Reset();

  while (!sections_of_state_indexes->IsFinished() ||
         !sections_of_country_indexes->IsFinished()) {
    auto current_section_of_state_indexes =
        sections_of_state_indexes->CurrentSection();
    auto current_section_of_country_indexes =
        sections_of_country_indexes->CurrentSection();
    // If there are still sections left with both country and state type, and
    // state and country current sections are equal, then that section has both
    // state and country. No rationalization needed.
    if (!sections_of_state_indexes->IsFinished() &&
        !sections_of_country_indexes->IsFinished() &&
        fields_[sections_of_state_indexes->CurrentIndex()]->section ==
            fields_[sections_of_country_indexes->CurrentIndex()]->section) {
      sections_of_state_indexes->WalkForwardToTheNextSection();
      sections_of_country_indexes->WalkForwardToTheNextSection();
      continue;
    }

    size_t upper_index = 0, lower_index = 0;

    // If country section is before the state ones, it means that that section
    // misses states, and the other way around.
    if (current_section_of_state_indexes < current_section_of_country_indexes) {
      // We only rationalize when we have exactly two visible fields of a kind.
      if (current_section_of_state_indexes.size() == 2) {
        upper_index = current_section_of_state_indexes[0];
        lower_index = current_section_of_state_indexes[1];
      }
      sections_of_state_indexes->WalkForwardToTheNextSection();
    } else {
      // We only rationalize when we have exactly two visible fields of a kind.
      if (current_section_of_country_indexes.size() == 2) {
        upper_index = current_section_of_country_indexes[0];
        lower_index = current_section_of_country_indexes[1];
      }
      sections_of_country_indexes->WalkForwardToTheNextSection();
    }

    // This is when upper and lower indexes are not changed, meaning that there
    // is no need for rationalization.
    if (upper_index == lower_index) {
      continue;
    }

    if (HeuristicsPredictionsAreApplicable(upper_index, lower_index,
                                           ADDRESS_HOME_STATE,
                                           ADDRESS_HOME_COUNTRY)) {
      ApplyRationalizationsToFields(
          upper_index, lower_index, fields_[upper_index]->heuristic_type(),
          fields_[lower_index]->heuristic_type(), form_interactions_ukm_logger);
      continue;
    }

    if (FieldShouldBeRationalizedToCountry(upper_index)) {
      ApplyRationalizationsToFields(upper_index, lower_index,
                                    ADDRESS_HOME_COUNTRY, ADDRESS_HOME_STATE,
                                    form_interactions_ukm_logger);
    } else {
      ApplyRationalizationsToFields(upper_index, lower_index,
                                    ADDRESS_HOME_STATE, ADDRESS_HOME_COUNTRY,
                                    form_interactions_ukm_logger);
    }
  }
}

void FormStructure::OverrideServerPredictionsWithHeuristics() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInNames) &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForMoreStructureInAddresses)) {
    return;
  }
  for (const auto& field : fields_) {
    switch (field->heuristic_type()) {
      case NAME_LAST_SECOND:
      case NAME_LAST_FIRST:
      case ADDRESS_HOME_STREET_NAME:
      case ADDRESS_HOME_HOUSE_NUMBER:
        field->SetTypeTo(AutofillType(field->heuristic_type()));
        break;
      default: {
      };
    }
  }
}

void FormStructure::RationalizeRepeatedFields(
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger) {
  // The type of every field whose index is in
  // sectioned_field_indexes_by_type[|type|] is predicted by server as |type|.
  // Example: sectioned_field_indexes_by_type[FULL_NAME] is a sectioned fields
  // indexes of fields whose types are predicted as FULL_NAME by the server.
  SectionedFieldsIndexes sectioned_field_indexes_by_type[MAX_VALID_FIELD_TYPE];

  for (const auto& field : fields_) {
    // The hidden fields are not considered when rationalizing.
    if (!field->IsVisible())
      continue;
    // The billing and non-billing types are aggregated.
    auto current_type = field->Type().GetStorableType();

    if (current_type != UNKNOWN_TYPE && current_type < MAX_VALID_FIELD_TYPE) {
      // Look at the sectioned field indexes for the current type, if the
      // current field belongs to that section, then the field index should be
      // added to that same section, otherwise, start a new section.
      sectioned_field_indexes_by_type[current_type].AddFieldIndex(
          &field - &fields_[0],
          /*is_new_section*/ sectioned_field_indexes_by_type[current_type]
                  .Empty() ||
              fields_[sectioned_field_indexes_by_type[current_type]
                          .LastFieldIndex()]
                      ->section != field->section);
    }
  }

  RationalizeAddressLineFields(
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_STREET_ADDRESS]),
      form_interactions_ukm_logger);
  // Since the billing types are mapped to the non-billing ones, no need to
  // take care of ADDRESS_BILLING_STATE and .. .
  RationalizeAddressStateCountry(
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_STATE]),
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_COUNTRY]),
      form_interactions_ukm_logger);
}

void FormStructure::RationalizeFieldTypePredictions() {
  RationalizeCreditCardFieldPredictions();
  for (const auto& field : fields_) {
    if (base::FeatureList::IsEnabled(features::kAutofillOffNoServerData) &&
        !field->should_autocomplete && field->server_type() == NO_SERVER_DATA &&
        field->heuristic_type() != CREDIT_CARD_VERIFICATION_CODE) {
      // When the field has autocomplete off, and the server returned no
      // prediction, then assume Autofill is not useful for the current field.
      // Special case for CVC (crbug.com/968036). We never send votes for CVC
      // fields, but we still fill them when the user inputs them via the CVC
      // prompt. Since Autofill doesn't trigger from a CVC field, we can keep
      // the client-side predictions for this type.
      field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
    } else {
      field->SetTypeTo(field->Type());
    }
  }
  RationalizeTypeRelationships();
}

void FormStructure::EncodeFormForQuery(
    AutofillPageQueryRequest::Form* query_form,
    std::vector<FormSignature>* queried_form_signatures) const {
  DCHECK(!IsMalformed());

  query_form->set_signature(form_signature().value());
  queried_form_signatures->push_back(form_signature());

  if (is_rich_query_enabled_) {
    EncodeFormMetadataForQuery(*this, query_form->mutable_metadata());
  }

  for (const auto& field : fields_) {
    if (ShouldSkipField(*field))
      continue;

    AutofillPageQueryRequest::Form::Field* added_field =
        query_form->add_fields();
    added_field->set_signature(field->GetFieldSignature().value());

    if (is_rich_query_enabled_) {
      EncodeFieldMetadataForQuery(*field, added_field->mutable_metadata());
    }

    if (IsAutofillFieldMetadataEnabled()) {
      added_field->set_control_type(field->form_control_type);

      if (!field->name.empty())
        added_field->set_name(base::UTF16ToUTF8(field->name));
    }
  }
}

void FormStructure::EncodeFormForUpload(
    AutofillUploadContents* upload,
    std::vector<FormSignature>* encoded_signatures) const {
  DCHECK(!IsMalformed());

  encoded_signatures->push_back(form_signature());

  if (randomized_encoder_) {
    PopulateRandomizedFormMetadata(*randomized_encoder_, *this,
                                   upload->mutable_randomized_form_metadata());
  }

  for (const auto& field : fields_) {
    // Don't upload checkable fields.
    if (IsCheckable(field->check_status))
      continue;

    // Add the same field elements as the query and a few more below.
    if (ShouldSkipField(*field))
      continue;

    auto* added_field = upload->add_field();

    for (const auto& field_type : field->possible_types()) {
      added_field->add_autofill_type(field_type);
    }

    field->NormalizePossibleTypesValidities();

    for (const auto& field_type_validities :
         field->possible_types_validities()) {
      auto* type_validities = added_field->add_autofill_type_validities();
      type_validities->set_type(field_type_validities.first);
      for (const auto& validity : field_type_validities.second) {
        type_validities->add_validity(validity);
      }
    }

    if (field->generation_type()) {
      added_field->set_generation_type(field->generation_type());
      added_field->set_generated_password_changed(
          field->generated_password_changed());
    }

    if (field->vote_type()) {
      added_field->set_vote_type(field->vote_type());
    }

    if (field->initial_value_hash()) {
      added_field->set_initial_value_hash(field->initial_value_hash().value());
    }

    added_field->set_signature(field->GetFieldSignature().value());

    if (field->properties_mask)
      added_field->set_properties_mask(field->properties_mask);

    if (randomized_encoder_) {
      PopulateRandomizedFieldMetadata(
          *randomized_encoder_, *this, *field,
          added_field->mutable_randomized_field_metadata());
    }

    if (IsAutofillFieldMetadataEnabled()) {
      added_field->set_type(field->form_control_type);

      if (!field->name.empty())
        added_field->set_name(base::UTF16ToUTF8(field->name));

      if (!field->id_attribute.empty())
        added_field->set_id(base::UTF16ToUTF8(field->id_attribute));

      if (!field->autocomplete_attribute.empty())
        added_field->set_autocomplete(field->autocomplete_attribute);

      if (!field->css_classes.empty())
        added_field->set_css_classes(base::UTF16ToUTF8(field->css_classes));
    }
  }
}

bool FormStructure::IsMalformed() const {
  if (!field_count())  // Nothing to add.
    return true;

  // Some badly formatted web sites repeat fields - limit number of fields to
  // 250, which is far larger than any valid form and proto still fits into 10K.
  // Do not send requests for forms with more than this many fields, as they are
  // near certainly not valid/auto-fillable.
  const size_t kMaxFieldsOnTheForm = 250;
  if (field_count() > kMaxFieldsOnTheForm)
    return true;
  return false;
}

void FormStructure::IdentifySections(bool has_author_specified_sections) {
  if (fields_.empty())
    return;

  const bool is_enabled_autofill_new_sectioning =
      base::FeatureList::IsEnabled(features::kAutofillUseNewSectioningMethod);
  const bool is_enabled_autofill_redundant_name_sectioning =
      base::FeatureList::IsEnabled(
          features::kAutofillSectionUponRedundantNameInfo);

  if (!has_author_specified_sections || is_enabled_autofill_new_sectioning) {
    // Name sections after the first field in the section.
    base::string16 current_section = fields_.front()->unique_name();

    // Keep track of the types we've seen in this section.
    std::set<ServerFieldType> seen_types;
    ServerFieldType previous_type = UNKNOWN_TYPE;

    // Boolean flag that is set to true when a field in the current section
    // has the autocomplete-section attribute defined.
    bool previous_autocomplete_section_present = false;

    bool is_hidden_section = false;
    base::string16 last_visible_section;
    for (const auto& field : fields_) {
      const ServerFieldType current_type = field->Type().GetStorableType();
      // All credit card fields belong to the same section that's different
      // from address sections.
      if (AutofillType(current_type).group() == CREDIT_CARD) {
        field->section = "credit-card";
        continue;
      }

      bool already_saw_current_type = seen_types.count(current_type) > 0;

      // Forms often ask for multiple phone numbers -- e.g. both a daytime and
      // evening phone number.  Our phone number detection is also generally a
      // little off.  Hence, ignore this field type as a signal here.
      if (AutofillType(current_type).group() == PHONE_HOME)
        already_saw_current_type = false;

      if (is_enabled_autofill_redundant_name_sectioning) {
        // Forms sometimes have a different format of inputting names in
        // different sections. If we believe a new name is being entered, assume
        // it is a new section (unless there are two identical inputs in a row).
        if (current_type == NAME_FULL)
          already_saw_current_type |= (seen_types.count(NAME_LAST) > 0);
      }

      bool ignored_field = !field->IsVisible();

      // This is the first visible field after a hidden section. Consider it as
      // the continuation of the last visible section.
      if (!ignored_field && is_hidden_section) {
        current_section = last_visible_section;
      }

      // Start a new section by an ignored field, only if the next field is also
      // already seen.
      size_t field_index = &field - &fields_[0];
      if (ignored_field &&
          (is_hidden_section ||
           !((field_index + 1) < fields_.size() &&
             seen_types.count(
                 fields_[field_index + 1]->Type().GetStorableType()) > 0))) {
        already_saw_current_type = false;
      }

      // Some forms have adjacent fields of the same type.  Two common examples:
      //  * Forms with two email fields, where the second is meant to "confirm"
      //    the first.
      //  * Forms with a <select> menu for states in some countries, and a
      //    freeform <input> field for states in other countries.  (Usually,
      //    only one of these two will be visible for any given choice of
      //    country.)
      // Generally, adjacent fields of the same type belong in the same logical
      // section.
      if (current_type == previous_type)
        already_saw_current_type = false;

      // Boolean flag that is set to true when the |field| has
      // autocomplete-section attribute defined.
      bool autocomplete_section_attribute_present = false;
      if (is_enabled_autofill_new_sectioning)
        autocomplete_section_attribute_present =
            (field->section != kDefaultSection);

      // Boolean flag that is set to true when the |field| has
      // autocomplete-section attribute defined and is different that the
      // previous field.
      bool different_autocomplete_section_than_previous = false;
      if (is_enabled_autofill_new_sectioning) {
        different_autocomplete_section_than_previous =
            (autocomplete_section_attribute_present &&
             (!field_index ||
              fields_[field_index - 1]->section != field->section));
      }

      // Start a new section if the |current_type| was already seen or the
      // autocomplete-section attribute is defined for the |field| which is
      // different than the previous field.
      if (current_type != UNKNOWN_TYPE &&
          (already_saw_current_type ||
           (is_enabled_autofill_new_sectioning &&
            different_autocomplete_section_than_previous))) {
        // Keep track of seen_types if the new section is hidden. The next
        // visible section might be the continuation of the previous visible
        // section.
        if (ignored_field) {
          is_hidden_section = true;
          last_visible_section = current_section;
        }

        if (!is_hidden_section &&
            (!is_enabled_autofill_new_sectioning ||
             !autocomplete_section_attribute_present ||
             different_autocomplete_section_than_previous))
          seen_types.clear();

        if (is_enabled_autofill_new_sectioning &&
            autocomplete_section_attribute_present &&
            !previous_autocomplete_section_present) {
          // If this field is the first field within the section with a defined
          // autocomplete section, then change the section attribute of all the
          // parsed fields in the current section to |field->section|.
          int i = static_cast<int>(field_index - 1);
          while (i >= 0 &&
                 base::UTF8ToUTF16(fields_[i]->section) == current_section) {
            fields_[i]->section = field->section;
            i--;
          }
        }

        // The end of a section, so start a new section.
        current_section = field->unique_name();

        if (is_enabled_autofill_new_sectioning) {
          // The section described in the autocomplete section attribute
          // overrides the value determined by the heuristic.
          if (autocomplete_section_attribute_present)
            current_section = base::UTF8ToUTF16(field->section);

          previous_autocomplete_section_present =
              autocomplete_section_attribute_present;
        }
      }

      // Only consider a type "seen" if it was not ignored. Some forms have
      // sections for different locales, only one of which is enabled at a
      // time. Each section may duplicate some information (e.g. postal code)
      // and we don't want that to cause section splits.
      // Also only set |previous_type| when the field was not ignored. This
      // prevents ignored fields from breaking up fields that are otherwise
      // adjacent.
      if (!ignored_field) {
        seen_types.insert(current_type);
        previous_type = current_type;
        is_hidden_section = false;
      }

      field->section = base::UTF16ToUTF8(current_section);
    }
  }

  // Ensure that credit card and address fields are in separate sections.
  // This simplifies the section-aware logic in autofill_manager.cc.
  for (const auto& field : fields_) {
    FieldTypeGroup field_type_group = field->Type().group();
    if (field_type_group == CREDIT_CARD)
      field->section = field->section + "-cc";
    else
      field->section = field->section + "-default";
  }
}

bool FormStructure::ShouldSkipField(const FormFieldData& field) const {
  return IsCheckable(field.check_status);
}

void FormStructure::ProcessExtractedFields() {
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillLabelAffixRemoval)) {
    // Updates the field name parsed by heuristics if several criteria are met.
    // Several fields must be present in the form.
    if (field_count() < kCommonNameAffixRemovalFieldNumberThreshold)
      return;

    std::vector<base::StringPiece16> names;
    names.reserve(field_count());
    for (const auto& field : *this)
      names.push_back(field->name);

    int longest_prefix_length = FindLongestCommonAffixLength(names, false);
    int longest_suffix_length = FindLongestCommonAffixLength(names, true);

    // Don't remove the common affix if it's not long enough.
    if (longest_prefix_length < kMinCommonNameAffixLength)
      longest_prefix_length = 0;

    if (longest_suffix_length < kMinCommonNameAffixLength)
      longest_suffix_length = 0;

    bool success =
        SetStrippedParseableNames(longest_prefix_length, longest_suffix_length);

    // Don't search for inconsistent prefix if valid affixes are found.
    if (success && longest_prefix_length + longest_suffix_length > 0)
      return;

    // Functionality for stripping a prefix only common to a subset
    // of field names.
    // This is needed because an exceptional field may be missing a prefix
    // which is otherwise consistently applied--for instance, a framework
    // may only apply a prefix to those fields which are bound when POSTing.
    names.clear();
    for (const auto& field : *this)
      if (field->name.size() > kMinCommonNameLongPrefixLength)
        names.push_back(field->name);

    if (names.size() < kCommonNamePrefixRemovalFieldThreshold)
      return;

    const int longest_long_prefix_length =
        FindLongestCommonAffixLength(names, false);

    if (longest_long_prefix_length >= kMinCommonNameLongPrefixLength)
      SetStrippedParseableNames(longest_long_prefix_length, 0);

    return;
  }

  // Update the field name parsed by heuristics if several criteria are met.
  // Several fields must be present in the form.
  if (field_count() < kCommonNamePrefixRemovalFieldThreshold)
    return;

  // Find the longest common prefix within all the field names.
  std::vector<base::string16> names;
  names.reserve(field_count());
  for (const auto& field : *this)
    names.push_back(field->name);

  const base::string16 longest_prefix = FindLongestCommonPrefix(names);
  if (longest_prefix.size() < kMinCommonNamePrefixLength)
    return;

  // The name without the prefix will be used for heuristics parsing.
  for (auto& field : *this) {
    if (field->name.size() > longest_prefix.size()) {
      field->set_parseable_name(
          field->name.substr(longest_prefix.size(), field->name.size()));
    }
  }
}

bool FormStructure::SetStrippedParseableNames(size_t offset_left,
                                              size_t offset_right) {
  // Keeps track if all stripped strings are valid according to
  // |IsValidParseableName()|. If at least one string is invalid,
  // all |parseable_name| are reset to |name|.
  bool should_keep = true;
  for (auto& field : *this) {
    // This check allows to only strip affixes from long enough strings.
    if (field->name.size() > offset_right + offset_left) {
      field->set_parseable_name(field->name.substr(
          offset_left, field->name.size() - offset_right - offset_left));
    } else {
      field->set_parseable_name(field->name);
    }

    should_keep &= IsValidParseableName(field->parseable_name());
    if (!should_keep)
      break;
  }

  // Reset if some stripped string was invalid.
  if (!should_keep) {
    for (auto& field : *this)
      field->set_parseable_name(field->name);
  }

  return should_keep;
}

bool FormStructure::IsValidParseableName(
    base::string16 candidateParseableName) {
  static const base::string16 kParseableNameValidationPattern =
      base::UTF8ToUTF16(kParseableNameValidationRe);
  if (MatchesPattern(candidateParseableName, kParseableNameValidationPattern))
    return true;

  return false;
}

// static
size_t FormStructure::FindLongestCommonAffixLength(
    const std::vector<base::StringPiece16>& strings,
    bool findCommonSuffix) {
  if (strings.empty())
    return 0;

  // Go through each character of the first string until there is a mismatch at
  // the same position in any other string. Adapted from http://goo.gl/YGukMM.
  for (size_t affix_len = 0; affix_len < strings[0].size(); affix_len++) {
    size_t base_string_index =
        findCommonSuffix ? strings[0].size() - affix_len - 1 : affix_len;
    for (size_t i = 1; i < strings.size(); i++) {
      size_t compared_string_index =
          findCommonSuffix ? strings[i].size() - affix_len - 1 : affix_len;
      if (affix_len >= strings[i].size() ||
          strings[i][compared_string_index] != strings[0][base_string_index]) {
        // Mismatch found.
        return affix_len;
      }
    }
  }
  return strings[0].size();
}

// static
base::string16 FormStructure::FindLongestCommonPrefix(
    const std::vector<base::string16>& strings) {
  if (strings.empty())
    return base::string16();

  std::vector<base::string16> filtered_strings;

  // Any strings less than kMinCommonNamePrefixLength are neither modified
  // nor considered when processing for a common prefix.
  std::copy_if(
      strings.begin(), strings.end(), std::back_inserter(filtered_strings),
      [](base::string16 s) { return s.size() >= kMinCommonNamePrefixLength; });

  if (filtered_strings.empty())
    return base::string16();

  // Go through each character of the first string until there is a mismatch at
  // the same position in any other string. Adapted from http://goo.gl/YGukMM.
  for (size_t prefix_len = 0; prefix_len < filtered_strings[0].size();
       prefix_len++) {
    for (size_t i = 1; i < filtered_strings.size(); i++) {
      if (prefix_len >= filtered_strings[i].size() ||
          filtered_strings[i].at(prefix_len) !=
              filtered_strings[0].at(prefix_len)) {
        // Mismatch found.
        return filtered_strings[i].substr(0, prefix_len);
      }
    }
  }
  return filtered_strings[0];
}

std::set<FormType> FormStructure::GetFormTypes() const {
  std::set<FormType> form_types;
  for (const auto& field : fields_) {
    form_types.insert(
        FormTypes::FieldTypeGroupToFormType(field->Type().group()));
  }
  return form_types;
}

base::string16 FormStructure::GetIdentifierForRefill() const {
  if (!form_name().empty())
    return form_name();

  if (field_count() && !field(0)->unique_name().empty())
    return field(0)->unique_name();

  return base::string16();
}

void FormStructure::set_randomized_encoder(
    std::unique_ptr<RandomizedEncoder> encoder) {
  randomized_encoder_ = std::move(encoder);
}

void FormStructure::RationalizeTypeRelationships() {
  // Create a local set of all the types for faster lookup.
  std::unordered_set<ServerFieldType> types;
  for (const auto& field : fields_) {
    types.insert(field->Type().GetStorableType());
  }

  const auto& type_relationship_rules = GetTypeRelationshipMap();

  for (const auto& field : fields_) {
    ServerFieldType field_type = field->Type().GetStorableType();
    const auto& ruleset_iterator = type_relationship_rules.find(field_type);
    if (ruleset_iterator != type_relationship_rules.end()) {
      // We have relationship rules for this type. Verify that at least one of
      // the required related type is present.
      bool found = false;
      for (ServerFieldType required_type : ruleset_iterator->second) {
        if (types.find(required_type) != types.end()) {
          // Found a required type, we can break as we only need one required
          // type to respect the rule.
          found = true;
          break;
        }
      }

      if (!found) {
        // No required type was found, the current field failed the relationship
        // requirements for its type. Disabling Autofill for this field.
        field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
      }
    }
  }
}

std::ostream& operator<<(std::ostream& buffer, const FormStructure& form) {
  buffer << "\nForm signature: "
         << base::StrCat({base::NumberToString(form.form_signature().value()),
                          " - ",
                          base::NumberToString(
                              HashFormSignature(form.form_signature()))});
  buffer << "\n Form name: " << form.form_name();
  buffer << "\n Unique renderer Id: " << form.unique_renderer_id().value();
  buffer << "\n Target URL:" << form.target_url();
  for (size_t i = 0; i < form.field_count(); ++i) {
    buffer << "\n Field " << i << ": ";
    const AutofillField* field = form.field(i);
    buffer << "\n  Signature: "
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", unique renderer id: ",
                   base::NumberToString(field->unique_renderer_id.value())});
    buffer << "\n  Name: " << field->parseable_name();

    auto type = field->Type().ToString();
    auto heuristic_type = AutofillType(field->heuristic_type()).ToString();
    auto server_type = AutofillType(field->server_type()).ToString();
    auto html_type_description =
        field->html_type() != HTML_TYPE_UNSPECIFIED
            ? base::StrCat(
                  {", html: ", FieldTypeToStringPiece(field->html_type())})
            : "";

    buffer << "\n  Type: "
           << base::StrCat({type, " (heuristic: ", heuristic_type, ", server: ",
                            server_type, html_type_description, ")"});
    buffer << "\n  Section: " << field->section;

    constexpr size_t kMaxLabelSize = 100;
    const base::string16 truncated_label =
        field->label.substr(0, std::min(field->label.length(), kMaxLabelSize));
    buffer << "\n  Label: " << truncated_label;

    buffer << "\n  Is empty: " << (field->IsEmpty() ? "Yes" : "No");
  }
  return buffer;
}

LogBuffer& operator<<(LogBuffer& buffer, const FormStructure& form) {
  buffer << Tag{"div"} << Attrib{"class", "form"};
  buffer << Tag{"table"};
  buffer << Tr{} << "Form signature:"
         << base::StrCat({base::NumberToString(form.form_signature().value()),
                          " - ",
                          base::NumberToString(
                              HashFormSignature(form.form_signature()))});
  buffer << Tr{} << "Form name:" << form.form_name();
  buffer << Tr{} << "Unique renderer id:" << form.unique_renderer_id().value();
  buffer << Tr{} << "Target URL:" << form.target_url();
  for (size_t i = 0; i < form.field_count(); ++i) {
    buffer << Tag{"tr"};
    buffer << Tag{"td"} << "Field " << i << ": " << CTag{};
    const AutofillField* field = form.field(i);
    buffer << Tag{"td"};
    buffer << Tag{"table"};
    buffer << Tr{} << "Signature:"
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", unique renderer id: ",
                   base::NumberToString(field->unique_renderer_id.value())});
    buffer << Tr{} << "Name:" << field->parseable_name();

    auto type = field->Type().ToString();
    auto heuristic_type = AutofillType(field->heuristic_type()).ToString();
    auto server_type = AutofillType(field->server_type()).ToString();
    auto html_type_description =
        field->html_type() != HTML_TYPE_UNSPECIFIED
            ? base::StrCat(
                  {", html: ", FieldTypeToStringPiece(field->html_type())})
            : "";

    buffer << Tr{} << "Type:"
           << base::StrCat({type, " (heuristic: ", heuristic_type, ", server: ",
                            server_type, html_type_description, ")"});
    buffer << Tr{} << "Section:" << field->section;

    constexpr size_t kMaxLabelSize = 100;
    const base::string16 truncated_label =
        field->label.substr(0, std::min(field->label.length(), kMaxLabelSize));
    buffer << Tr{} << "Label:" << truncated_label;

    buffer << Tr{} << "Is empty:" << (field->IsEmpty() ? "Yes" : "No");
    buffer << CTag{"table"};
    buffer << CTag{"td"};
    buffer << CTag{"tr"};
  }
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  return buffer;
}

}  // namespace autofill
