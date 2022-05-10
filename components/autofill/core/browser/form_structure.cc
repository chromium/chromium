// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"
#include "components/autofill/core/browser/autofill_regexes.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_processing/label_processing_util.h"
#include "components/autofill/core/browser/form_processing/name_processing_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/rationalization_util.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/security_state/core/security_state.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace autofill {

using mojom::SubmissionIndicatorEvent;

namespace {

constexpr char kBillingMode[] = "billing";
constexpr char kShippingMode[] = "shipping";

// Default section name for the fields.
constexpr char kDefaultSection[] = "-default";

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
  for (auto field_type : available_field_types) {
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

  if (autocomplete_attribute_value == "promo-code" ||
      autocomplete_attribute_value == "promo_code" ||
      autocomplete_attribute_value == "promotion-code" ||
      autocomplete_attribute_value == "promotion_code" ||
      autocomplete_attribute_value == "promotional-code" ||
      autocomplete_attribute_value == "promotional_code" ||
      autocomplete_attribute_value == "coupon-code" ||
      autocomplete_attribute_value == "coupon_code" ||
      autocomplete_attribute_value == "gift-code" ||
      autocomplete_attribute_value == "gift_code")
    return HTML_TYPE_MERCHANT_PROMO_CODE;

  return HTML_TYPE_UNRECOGNIZED;
}

std::ostream& operator<<(std::ostream& out,
                         const AutofillQueryResponse& response) {
  for (const auto& form : response.form_suggestions()) {
    out << "\nForm";
    for (const auto& field : form.field_suggestions()) {
      out << "\n Field\n  signature: " << field.field_signature();
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
    for (auto type : field->possible_types()) {
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
    output->set_checksum(StrToHash32Bit(data_value));
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

  for (const auto& [title, title_type] : form.button_titles()) {
    auto* button_title = metadata->add_button_title();
    DCHECK(!title.empty());
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::FORM_BUTTON_TITLES, title,
                          /*include_checksum=*/false,
                          button_title->mutable_title());
    button_title->set_type(static_cast<ButtonTitleType>(title_type));
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

// Defines necessary types for the rationalization logic, meaning that fields of
// `type` are only filled if at least one field of some `GetNecessaryTypesFor()`
// is present.
// TODO(crbug.com/1311937) Cleanup PHONE_HOME_CITY_AND_NUMBER when launched.
ServerFieldTypeSet GetNecessaryTypesFor(ServerFieldType type) {
  switch (type) {
    case PHONE_HOME_COUNTRY_CODE:
      return {PHONE_HOME_NUMBER,
              base::FeatureList::IsEnabled(
                  features::kAutofillEnableSupportForPhoneNumberTrunkTypes)
                  ? PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX
                  : PHONE_HOME_CITY_AND_NUMBER};
    default:
      return {};
  }
}

LogBufferSubmitter LogRationalization(LogManager* log_manager) {
  if (!log_manager)
    return LogManager::DevNull();
  LogBufferSubmitter submitter = log_manager->Log();
  submitter << LoggingScope::kRationalization << LogMessage::kRationalization;
  return submitter;
}

// Creates a unique name for the section that starts with |field|.
//
// The section name is a string of the form "%s_%u_%u", where the first string
// is the field's name and the two integers are the field's frame ID and its
// renderer ID.
//
// For the frame ID, we do not use LocalFrameTokens but instead map them to
// consecutive integers using |frame_token_ids|, which uniquely identify a frame
// within a given FormStructure. Since we do not intend to compare sections from
// different FormStructures, this is sufficient.
//
// We intentionally do not include the LocalFrameToken in the section string
// because frame tokens should not be sent to a renderer.
//
// TODO(crbug.com/1257141): Remove special handling of FrameTokens.
std::u16string GetSectionName(
    const AutofillField& field,
    base::flat_map<LocalFrameToken, size_t>& frame_token_ids) {
  size_t id = frame_token_ids.emplace(field.host_frame, frame_token_ids.size())
                  .first->second;
  return base::StrCat(
      {field.name, u"_", base::NumberToString16(id), u"_",
       base::NumberToString16(field.unique_renderer_id.value())});
}

}  // namespace

class FormStructure::SectionedFieldsIndexes {
 public:
  SectionedFieldsIndexes() = default;
  ~SectionedFieldsIndexes() = default;

  size_t LastFieldIndex() const {
    if (sectioned_indexes_.empty())
      return std::numeric_limits<size_t>::max();  // Shouldn't happen.
    return sectioned_indexes_.back().back();
  }

  void AddFieldIndex(const size_t index, bool is_new_section) {
    if (is_new_section || Empty()) {
      sectioned_indexes_.emplace_back();
    }
    sectioned_indexes_.back().push_back(index);
  }

  void WalkForwardToTheNextSection() { current_section_ptr_++; }

  bool IsFinished() const {
    return current_section_ptr_ >= sectioned_indexes_.size();
  }

  size_t CurrentIndex() const {
    return current_section_ptr_ < sectioned_indexes_.size()
               ? sectioned_indexes_[current_section_ptr_].front()
               : std::numeric_limits<size_t>::max();
  }

  const std::vector<size_t>* CurrentSection() const {
    return current_section_ptr_ < sectioned_indexes_.size()
               ? &sectioned_indexes_[current_section_ptr_]
               : nullptr;
  }

  void Reset() { current_section_ptr_ = 0; }

  bool Empty() const { return sectioned_indexes_.empty(); }

 private:
  // A vector of sections. Each section is a vector of some of the indexes
  // that belong to the same section. The sections and indexes are sorted by
  // their order of appearance on the form.
  std::vector<std::vector<size_t>> sectioned_indexes_;
  // Points to a vector of indexes that belong to the same section.
  size_t current_section_ptr_ = 0;
};

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
      all_fields_are_passwords_(!form.fields.empty()),
      form_parsed_timestamp_(AutofillTickClock::NowTicks()),
      host_frame_(form.host_frame),
      version_(form.version),
      unique_renderer_id_(form.unique_renderer_id) {
  // Copy the form fields.
  for (const FormFieldData& field : form.fields) {
    if (!ShouldSkipField(field))
      ++active_field_count_;

    if (field.form_control_type == "password")
      has_password_field_ = true;
    else
      all_fields_are_passwords_ = false;

    fields_.push_back(std::make_unique<AutofillField>(field));
  }

  form_signature_ = CalculateFormSignature(form);
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

void FormStructure::DetermineHeuristicTypes(
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.DetermineHeuristicTypes");

  ParseFieldTypesFromAutocompleteAttributes();
  ParseFieldTypesWithPatterns(GetActivePatternSource(), log_manager);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillDisableShadowHeuristics)) {
    for (PatternSource shadow_source : GetNonActivePatternSources())
      ParseFieldTypesWithPatterns(shadow_source, log_manager);
  }

  UpdateAutofillCount();
  IdentifySections(has_author_specified_sections_);

  if (base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection))
    RationalizeRepeatedFields(form_interactions_ukm_logger, log_manager);
  RationalizeFieldTypePredictions(log_manager);

  LogDetermineHeuristicTypesMetrics();
}

std::vector<AutofillUploadContents> FormStructure::EncodeUploadRequest(
    const ServerFieldTypeSet& available_field_types,
    bool form_was_autofilled,
    const base::StringPiece& login_form_signature,
    bool observed_submission,
    bool is_raw_metadata_uploading_enabled) const {
  DCHECK(AllTypesCaptured(*this, available_field_types));
  std::string data_present = EncodeFieldTypes(available_field_types);

  AutofillUploadContents upload;
  upload.set_submission(observed_submission);
  upload.set_client_version(
      version_info::GetProductNameAndVersionForUserAgent());
  upload.set_form_signature(form_signature().value());
  upload.set_autofill_used(form_was_autofilled);
  upload.set_data_present(data_present);
  upload.set_passwords_revealed(passwords_were_revealed_);
  upload.set_has_form_tag(is_form_tag_);
  if (!current_page_language_->empty() && randomized_encoder_ != nullptr) {
    upload.set_language(current_page_language_.value());
  }
  if (single_username_data_)
    upload.mutable_single_username_data()->CopyFrom(*single_username_data_);

  auto triggering_event = (submission_event_ != SubmissionIndicatorEvent::NONE)
                              ? submission_event_
                              : ToSubmissionIndicatorEvent(submission_source_);

  DCHECK(mojom::IsKnownEnumValue(triggering_event));
  upload.set_submission_event(
      static_cast<AutofillUploadContents_SubmissionIndicatorEvent>(
          triggering_event));

  if (password_attributes_vote_) {
    EncodePasswordAttributesVote(*password_attributes_vote_,
                                 password_length_vote_, password_symbol_vote_,
                                 &upload);
  }

  if (is_raw_metadata_uploading_enabled) {
    upload.set_action_signature(StrToHash64Bit(target_url_.host_piece()));
    if (!form_name().empty())
      upload.set_form_name(base::UTF16ToUTF8(form_name()));
    for (const ButtonTitleInfo& e : button_titles_) {
      auto* button_title = upload.add_button_title();
      button_title->set_title(base::UTF16ToUTF8(e.first));
      button_title->set_type(static_cast<ButtonTitleType>(e.second));
    }
  }

  if (!login_form_signature.empty()) {
    uint64_t login_sig;
    if (base::StringToUint64(login_form_signature, &login_sig))
      upload.set_login_form_signature(login_sig);
  }

  if (IsMalformed())
    return {};  // Malformed form, skip it.

  if (randomized_encoder_) {
    PopulateRandomizedFormMetadata(*randomized_encoder_, *this,
                                   upload.mutable_randomized_form_metadata());
  }

  EncodeFormFieldsForUpload(is_raw_metadata_uploading_enabled, absl::nullopt,
                            &upload);

  std::vector<AutofillUploadContents> uploads = {std::move(upload)};

  // Build AutofillUploadContents for the renderer forms that have been
  // flattened into `this` (see the function's documentation for details).
  std::vector<std::pair<FormGlobalId, FormSignature>> subforms;
  for (const auto& field : *this) {
    if (field->host_form_signature != form_signature()) {
      subforms.emplace_back(field->renderer_form_id(),
                            field->host_form_signature);
    }
  }
  for (const auto& [subform_id, subform_signature] :
       base::flat_map<FormGlobalId, FormSignature>(std::move(subforms))) {
    uploads.emplace_back();
    uploads.back().set_client_version(
        version_info::GetProductNameAndVersionForUserAgent());
    uploads.back().set_form_signature(subform_signature.value());
    uploads.back().set_autofill_used(form_was_autofilled);
    uploads.back().set_data_present(data_present);
    EncodeFormFieldsForUpload(is_raw_metadata_uploading_enabled, subform_id,
                              &uploads.back());
  }

  return uploads;
}

// static
bool FormStructure::EncodeQueryRequest(
    const std::vector<FormStructure*>& forms,
    AutofillPageQueryRequest* query,
    std::vector<FormSignature>* queried_form_signatures) {
  DCHECK(queried_form_signatures);
  queried_form_signatures->clear();
  queried_form_signatures->reserve(forms.size());

  query->set_client_version(
      version_info::GetProductNameAndVersionForUserAgent());

  // If a page contains repeated forms, detect that and encode only one form as
  // the returned data would be the same for all the repeated forms.
  // TODO(crbug/1064709#c11): the statement is not entirely correct because
  // (1) distinct forms can have identical form signatures because we truncate
  // (large) numbers in the form signature calculation while these are
  // considered for field signatures; (2) for dynamic forms we will hold on to
  // the original form signature.
  std::set<FormSignature> processed_forms;
  for (const auto* form : forms) {
    if (base::Contains(processed_forms, form->form_signature()))
      continue;
    UMA_HISTOGRAM_COUNTS_1000("Autofill.FieldCount", form->field_count());
    if (form->IsMalformed())
      continue;

    form->EncodeFormForQuery(query, queried_form_signatures, &processed_forms);
  }

  return !queried_form_signatures->empty();
}

// static
void FormStructure::ParseApiQueryResponse(
    base::StringPiece payload,
    const std::vector<FormStructure*>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
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
                       form_interactions_ukm_logger, log_manager);
}

// static
void FormStructure::ProcessQueryResponse(
    const AutofillQueryResponse& response,
    const std::vector<FormStructure*>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  using FieldSuggestion =
      AutofillQueryResponse::FormSuggestion::FieldSuggestion;
  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED);
  if (log_manager) {
    log_manager->Log() << LoggingScope::kParsing
                       << LogMessage::kProcessingServerData;
  }

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;

  std::map<std::pair<FormSignature, FieldSignature>,
           std::deque<FieldSuggestion>>
      field_types;

  for (int form_idx = 0;
       form_idx < std::min(response.form_suggestions_size(),
                           static_cast<int>(queried_form_signatures.size()));
       ++form_idx) {
    FormSignature form_sig = queried_form_signatures[form_idx];
    for (const auto& field :
         response.form_suggestions(form_idx).field_suggestions()) {
      FieldSignature field_sig(field.field_signature());
      field_types[{form_sig, field_sig}].push_back(field);
    }
  }

  // Retrieves the next prediction for |form| and |field| and pops it. Popping
  // is omitted if no other predictions for |form| and |field| are left, so that
  // any subsequent fields with the same signature will get the same prediction.
  auto GetPrediction =
      [&field_types](FormSignature form,
                     FieldSignature field) -> absl::optional<FieldSuggestion> {
    auto it = field_types.find({form, field});
    if (it == field_types.end())
      return absl::nullopt;
    DCHECK(!it->second.empty());
    auto current_field = it->second.front();
    if (it->second.size() > 1)
      it->second.pop_front();
    return absl::make_optional(std::move(current_field));
  };

  // Copy the field types into the actual form.
  for (FormStructure* form : forms) {
    for (auto& field : form->fields_) {
      // Get the field prediction for |form|'s signature and the |field|'s
      // host_form_signature. The former takes precedence over the latter.
      absl::optional<FieldSuggestion> current_field =
          GetPrediction(form->form_signature(), field->GetFieldSignature());
      if (base::FeatureList::IsEnabled(features::kAutofillAcrossIframes) &&
          field->host_form_signature &&
          field->host_form_signature != form->form_signature()) {
        // Retrieves the alternative prediction even if it is not used so that
        // the alternative predictions are popped.
        absl::optional<FieldSuggestion> alternative_field = GetPrediction(
            field->host_form_signature, field->GetFieldSignature());
        if (alternative_field &&
            (!current_field ||
             base::ranges::all_of(current_field->predictions(),
                                  [](const auto& prediction) {
                                    return prediction.type() == NO_SERVER_DATA;
                                  }))) {
          current_field = *alternative_field;
        }
      }
      if (!current_field)
        continue;

      ServerFieldType heuristic_type = field->heuristic_type();
      if (heuristic_type != UNKNOWN_TYPE)
        heuristics_detected_fillable_field = true;

      field->set_server_predictions({current_field->predictions().begin(),
                                     current_field->predictions().end()});
      field->set_may_use_prefilled_placeholder(
          current_field->may_use_prefilled_placeholder());

      if (heuristic_type != field->Type().GetStorableType())
        query_response_overrode_heuristics = true;

      if (current_field->has_password_requirements())
        field->SetPasswordRequirements(current_field->password_requirements());
    }

    AutofillMetrics::LogServerResponseHasDataForForm(base::ranges::any_of(
        form->fields_, [](ServerFieldType t) { return t != NO_SERVER_DATA; },
        &AutofillField::server_type));

    form->UpdateAutofillCount();
    form->RationalizeRepeatedFields(form_interactions_ukm_logger, log_manager);
    form->RationalizeFieldTypePredictions(log_manager);
    // TODO(crbug.com/1154080): By calling this with false, autocomplete section
    // attributes will be ignored.
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
      annotated_field.host_form_signature =
          base::NumberToString(field->host_form_signature.value());
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
std::vector<FieldGlobalId> FormStructure::FindFieldsEligibleForManualFilling(
    const std::vector<FormStructure*>& forms) {
  std::vector<FieldGlobalId> fields_eligible_for_manual_filling;
  for (const auto* form : forms) {
    for (const auto& field : form->fields_) {
      FieldTypeGroup field_type_group =
          GroupTypeOfServerFieldType(field->server_type());
      // In order to trigger the payments bottom sheet that assists users to
      // manually fill the form, credit card form fields are marked eligible for
      // manual filling. Also, if a field is not classified to a type, we can
      // assume that the prediction failed and thus mark it eligible for manual
      // filling. As more form types support manual filling on form interaction,
      // this list may expand in the future.
      if (field_type_group == FieldTypeGroup::kCreditCard ||
          field_type_group == FieldTypeGroup::kNoGroup) {
        fields_eligible_for_manual_filling.push_back(field->global_id());
      }
    }
  }
  return fields_eligible_for_manual_filling;
}

std::unique_ptr<FormStructure> FormStructure::CreateForPasswordManagerUpload(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures) {
  return base::WrapUnique(new FormStructure(form_signature, field_signatures));
}

std::string FormStructure::FormSignatureAsStr() const {
  return base::NumberToString(form_signature().value());
}

bool FormStructure::IsAutofillable() const {
  size_t min_required_fields =
      std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                kMinRequiredFieldsForUpload});
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
      std::min({kMinRequiredFieldsForHeuristics, kMinRequiredFieldsForQuery,
                kMinRequiredFieldsForUpload});
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
  if (MatchesPattern(base::UTF8ToUTF16(target_url_.path_piece()),
                     kUrlSearchActionRe)) {
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
  return active_field_count() >= kMinRequiredFieldsForHeuristics &&
         HasAllowedScheme(source_url_);
}

bool FormStructure::ShouldRunPromoCodeHeuristics() const {
  return base::FeatureList::IsEnabled(
             features::kAutofillParseMerchantPromoCodeFields) &&
         active_field_count() > 0 && HasAllowedScheme(source_url_);
}

bool FormStructure::ShouldBeQueried() const {
  return (has_password_field_ ||
          active_field_count() >= kMinRequiredFieldsForQuery) &&
         ShouldBeParsed();
}

bool FormStructure::ShouldBeUploaded() const {
  return active_field_count() >= kMinRequiredFieldsForUpload &&
         ShouldBeParsed();
}

void FormStructure::RetrieveFromCache(
    const FormStructure& cached_form,
    const bool should_keep_cached_value,
    const bool only_server_and_autofill_state) {
  std::map<FieldGlobalId, const AutofillField*> cached_fields_by_id;
  for (size_t i = 0; i < cached_form.field_count(); ++i) {
    auto* const field = cached_form.field(i);
    cached_fields_by_id[field->global_id()] = field;
  }
  for (auto& field : *this) {
    const AutofillField* cached_field = nullptr;
    const auto& it = cached_fields_by_id.find(field->global_id());
    if (it != cached_fields_by_id.end())
      cached_field = it->second;

    // If the unique renderer id (or the name) is not stable due to some Java
    // Script magic in the website, use the field signature as a fallback
    // solution to find the field in the cached form.
    if (!cached_field) {
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
        for (int i = 0; i <= static_cast<int>(PatternSource::kMaxValue); ++i) {
          PatternSource s = static_cast<PatternSource>(i);
          field->set_heuristic_type(s, cached_field->heuristic_type(s));
        }
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
        if (should_keep_cached_value) {
          field->value = cached_field->value;
          value_from_dynamic_change_form_ = true;
        } else if (field->value == cached_field->value &&
                   (field->server_type() != ADDRESS_HOME_COUNTRY &&
                    field->server_type() != ADDRESS_HOME_STATE)) {
          // From the perspective of learning user data, text fields containing
          // default values are equivalent to empty fields.
          // Since a website can prefill country and state values basedw on
          // GeoIp, the mechanism is deactivated for state and country fields.
          field->value = std::u16string();
        }
      }
      field->set_server_predictions(cached_field->server_predictions());
      field->set_previously_autofilled(cached_field->previously_autofilled());

      if (cached_field->value_not_autofilled_over_existing_value_hash()) {
        field->set_value_not_autofilled_over_existing_value_hash(
            *cached_field->value_not_autofilled_over_existing_value_hash());
      }

      // Only retrieve an overall prediction from cache if a server prediction
      // is set.
      if (base::FeatureList::IsEnabled(
              features::kAutofillRetrieveOverallPredictionsFromCache) &&
          field->server_type() != NO_SERVER_DATA) {
        field->SetTypeTo(cached_field->Type());
      }
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
    bool observed_submission,
    const FormInteractionCounts& form_interaction_counts) const {
  // Use the same timestamp on UKM Metrics generated within this method's scope.
  AutofillMetrics::UkmTimestampPin timestamp_pin(form_interactions_ukm_logger);

  // Determine the type of the form.
  DenseSet<FormType> form_types = GetFormTypes();
  bool card_form = base::Contains(form_types, FormType::kCreditCardForm);
  bool address_form = base::Contains(form_types, FormType::kAddressForm);

  ServerFieldTypeSet autofilled_field_types;
  size_t num_detected_field_types = 0;
  size_t num_edited_autofilled_fields = 0;
  size_t num_of_accepted_autofilled_fields = 0;
  size_t num_of_corrected_autofilled_fields = 0;

  // Count the number of filled (and corrected) fields which used to not get a
  // type prediction due to autocomplete=unrecognized. Note that credit card
  // related fields are excluded from this since an unrecognized autocomplete
  // attribute has no effect for them even if
  // |kAutofillFillAndImportFromMoreFields| is disabled.
  size_t num_of_accepted_autofilled_fields_with_autocomplete_unrecognized = 0;
  size_t num_of_corrected_autofilled_fields_with_autocomplete_unrecognized = 0;

  bool did_autofill_all_possible_fields = true;
  bool did_autofill_some_possible_fields = false;
  bool is_for_credit_card = IsCompleteCreditCardForm();
  bool has_upi_vpa_field = false;
  bool has_observed_one_time_code_field = false;
  // A perfectly filled form is submitted as it was filled from Autofill without
  // subsequent changes.
  bool perfect_filling = true;
  // Contain the frames across which the fields are distributed.
  base::flat_set<LocalFrameToken> frames_of_detected_fields;
  base::flat_set<LocalFrameToken> frames_of_detected_credit_card_fields;
  base::flat_set<LocalFrameToken> frames_of_autofilled_credit_card_fields;

  // Determine the correct suffix for the metric, depending on whether or
  // not a submission was observed.
  const AutofillMetrics::QualityMetricType metric_type =
      observed_submission ? AutofillMetrics::TYPE_SUBMISSION
                          : AutofillMetrics::TYPE_NO_SUBMISSION;

  for (auto& field : *this) {
    AutofillType type = field->Type();

    if (IsUPIVirtualPaymentAddress(field->value)) {
      has_upi_vpa_field = true;
      AutofillMetrics::LogUserHappinessMetric(
          AutofillMetrics::USER_DID_ENTER_UPI_VPA, type.group(),
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
    autofill::metrics::LogShadowPredictionComparison(*field);
    // We count fields that were autofilled but later modified, regardless of
    // whether the data now in the field is recognized.
    if (field->previously_autofilled())
      num_edited_autofilled_fields++;

    if (type.html_type() == HTML_TYPE_ONE_TIME_CODE)
      has_observed_one_time_code_field = true;

    // The form was not perfectly filled if a non-empty field was not
    // autofilled.
    if (!field->value.empty() && !field->is_autofilled)
      perfect_filling = false;

    const ServerFieldTypeSet& field_types = field->possible_types();
    DCHECK(!field_types.empty());

    if (field_types.count(EMPTY_TYPE) || field_types.count(UNKNOWN_TYPE)) {
      DCHECK_EQ(field_types.size(), 1u);
      continue;
    }

    ++num_detected_field_types;

    // Count the number of autofilled and corrected fields.
    if (field->is_autofilled) {
      ++num_of_accepted_autofilled_fields;
      if (field->ShouldSuppressPromptDueToUnrecognizedAutocompleteAttribute()) {
        ++num_of_accepted_autofilled_fields_with_autocomplete_unrecognized;
      }
    } else if (field->previously_autofilled()) {
      ++num_of_corrected_autofilled_fields;
      if (field->ShouldSuppressPromptDueToUnrecognizedAutocompleteAttribute()) {
        ++num_of_corrected_autofilled_fields_with_autocomplete_unrecognized;
      }
    }

    if (field->is_autofilled)
      did_autofill_some_possible_fields = true;
    else if (!field->only_fill_when_focused())
      did_autofill_all_possible_fields = false;

    if (field->is_autofilled)
      autofilled_field_types.insert(type.GetStorableType());

    // Keep track of the frames of detected and autofilled (credit card) fields.
    frames_of_detected_fields.insert(field->host_frame);
    if (type.group() == FieldTypeGroup::kCreditCard) {
      frames_of_detected_credit_card_fields.insert(field->host_frame);
      if (field->is_autofilled)
        frames_of_autofilled_credit_card_fields.insert(field->host_frame);
    }

    // If the form was submitted, record if field types have been filled and
    // subsequently edited by the user.
    if (observed_submission) {
      if (field->is_autofilled || field->previously_autofilled()) {
        AutofillMetrics::LogEditedAutofilledFieldAtSubmission(
            form_interactions_ukm_logger, *this, *field);

        // If the field was a |ADDRESS_HOME_STATE| field which was autofilled,
        // record the source of the autofilled value between
        // |AlternativeStateNameMap| or the profile value.
        if (field->is_autofilled &&
            type.GetStorableType() == ADDRESS_HOME_STATE) {
          AutofillMetrics::
              LogAutofillingSourceForStateSelectionFieldAtSubmission(
                  field->state_is_a_matching_type()
                      ? AutofillMetrics::
                            AutofilledSourceMetricForStateSelectionField::
                                AUTOFILL_BY_ALTERNATIVE_STATE_NAME_MAP
                      : AutofillMetrics::
                            AutofilledSourceMetricForStateSelectionField::
                                AUTOFILL_BY_VALUE);
        }
      }
    }
  }

  AutofillMetrics::LogNumberOfEditedAutofilledFields(
      num_edited_autofilled_fields, observed_submission);

  // We log "submission" and duration metrics if we are here after observing a
  // submission event.
  if (observed_submission) {
    AutofillMetrics::AutofillFormSubmittedState state;
    if (num_detected_field_types < kMinRequiredFieldsForHeuristics &&
        num_detected_field_types < kMinRequiredFieldsForQuery) {
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

      // Log the number of autofilled fields at submission time.
      AutofillMetrics::LogNumberOfAutofilledFieldsAtSubmission(
          num_of_accepted_autofilled_fields,
          num_of_corrected_autofilled_fields);

      // Log the number of autofilled fields with an unrecognized autocomplete
      // attribute at submission time.
      // Note that credit card fields are not counted since they generally
      // ignore an unrecognized autocompelte attribute.
      AutofillMetrics::
          LogNumberOfAutofilledFieldsWithAutocompleteUnrecognizedAtSubmission(
              num_of_accepted_autofilled_fields_with_autocomplete_unrecognized,
              num_of_corrected_autofilled_fields_with_autocomplete_unrecognized);

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

    if (has_observed_one_time_code_field) {
      if (!load_time.is_null()) {
        DCHECK_GE(submission_time, load_time);
        base::TimeDelta elapsed = submission_time - load_time;
        AutofillMetrics::LogFormFillDurationFromLoadForOneTimeCode(elapsed);
      }
      if (!interaction_time.is_null()) {
        DCHECK(submission_time > interaction_time);
        base::TimeDelta elapsed = submission_time - interaction_time;
        AutofillMetrics::LogFormFillDurationFromInteractionForOneTimeCode(
            elapsed);
      }
    }

    AutofillMetrics::LogAutofillFormSubmittedState(
        state, is_for_credit_card, has_upi_vpa_field, GetFormTypes(),
        form_parsed_timestamp_, form_signature(), form_interactions_ukm_logger,
        form_interaction_counts);

    // The perfect filling metric is only recorded if Autofill was used on at
    // least one field. This conditions this metric on Assistance, Readiness and
    // Acceptance.
    if (did_autofill_some_possible_fields) {
      // Perfect filling is recorded for addresses and credit cards separately.
      // Note that a form can be both an address and a credit card form
      // simultaneously.
      if (address_form) {
        AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/true,
                                                   perfect_filling);
      }
      if (card_form) {
        AutofillMetrics::LogAutofillPerfectFilling(/*is_address=*/false,
                                                   perfect_filling);
      }
    }

    AutofillMetrics::LogNumberOfFramesWithDetectedFields(
        frames_of_detected_fields.size());
    AutofillMetrics::LogNumberOfFramesWithDetectedCreditCardFields(
        frames_of_detected_credit_card_fields.size());
    AutofillMetrics::LogNumberOfFramesWithAutofilledCreditCardFields(
        frames_of_autofilled_credit_card_fields.size());

    if (card_form) {
      AutofillMetrics::LogCreditCardSeamlessnessAtSubmissionTime(
          autofilled_field_types);
    }
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

void FormStructure::LogDetermineHeuristicTypesMetrics() {
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
}

void FormStructure::ParseFieldTypesFromAutocompleteAttributes() {
  if (was_parsed_for_autocomplete_attributes_)
    return;

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

void FormStructure::ParseFieldTypesWithPatterns(PatternSource pattern_source,
                                                LogManager* log_manager) {
  FieldCandidatesMap field_type_map;
  if (ShouldRunHeuristics()) {
    field_type_map =
        FormField::ParseFormFields(fields_, current_page_language_,
                                   is_form_tag_, pattern_source, log_manager);
  } else if (ShouldRunPromoCodeHeuristics()) {
    field_type_map = FormField::ParseFormFieldsForPromoCodes(
        fields_, current_page_language_, is_form_tag_, pattern_source,
        log_manager);
  }
  if (field_type_map.empty())
    return;

  for (const auto& field : fields_) {
    auto iter = field_type_map.find(field->global_id());
    if (iter == field_type_map.end())
      continue;
    const FieldCandidates& candidates = iter->second;
    field->set_heuristic_type(pattern_source, candidates.BestHeuristicType());
  }
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
  data.unique_renderer_id = unique_renderer_id_;
  data.host_frame = host_frame_;
  data.version = version_;

  for (const auto& field : fields_) {
    data.fields.push_back(*field);
  }

  return data;
}

void FormStructure::RationalizeCreditCardFieldPredictions(
    LogManager* log_manager) {
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

  if (!keep_cc_fields && num_cc_fields_found && log_manager) {
    LogRationalization(log_manager)
        << "Credit card rationalization: Did not find credit card number, did "
           "not find >= 3 credit card fields ("
        << num_cc_fields_found << "), and had non-cc fields ("
        << num_other_fields_found << ").";
  }

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
          if (!cc_date_found && log_manager) {
            LogRationalization(log_manager)
                << "Credit card rationalization: Found CC expiration month but "
                   "not a full date.";
          }
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
        } else if (num_months_found > 1) {
          auto it2 = it + 1;
          if (it2 == fields_.end()) {
            field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
            LogRationalization(log_manager)
                << "Credit card rationalization: Found multiple expiration "
                   "months and the last field was an expiration month";
            field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
          } else {
            ServerFieldType next_field_type = (*it2)->Type().GetStorableType();
            if (next_field_type != CREDIT_CARD_EXP_2_DIGIT_YEAR &&
                next_field_type != CREDIT_CARD_EXP_4_DIGIT_YEAR) {
              field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
            }
            LogRationalization(log_manager)
                << "Credit card rationalization: Found multiple expiration "
                   "months and the field following one is not an "
                   "expiration year but "
                << FieldTypeToStringPiece(next_field_type) << ".";
          }
        }
        break;
      case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      case CREDIT_CARD_EXP_4_DIGIT_YEAR:
        if (!keep_cc_fields || !cc_date_found) {
          field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
          if (!cc_date_found && log_manager) {
            LogRationalization(log_manager)
                << "Credit card rationalization: Found expiration year but no "
                   "full expriration date.";
          }
        }
        break;
      default:
        break;
    }
  }
}

void FormStructure::RationalizePhoneNumbersInSection(
    const std::string& section) {
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
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  // The rationalization happens within sections.
  for (sections_of_address_indexes->Reset();
       !sections_of_address_indexes->IsFinished();
       sections_of_address_indexes->WalkForwardToTheNextSection()) {
    auto* current_section = sections_of_address_indexes->CurrentSection();

    // The rationalization only applies to sections that have 2 or 3 visible
    // street address predictions.
    if (!current_section ||
        (current_section->size() != 2 && current_section->size() != 3)) {
      continue;
    }

    int nb_address_rationalized = 0;
    for (auto field_index : *current_section) {
      LogBufferSubmitter log_submitter = LogRationalization(log_manager);
      log_submitter
          << "RationalizeAddressLineFields ADDRESS_HOME_STREET_ADDRESS to ";
      switch (nb_address_rationalized) {
        case 0:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE1,
                                             form_interactions_ukm_logger);
          log_submitter << "ADDRESS_HOME_LINE1";
          break;
        case 1:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE2,
                                             form_interactions_ukm_logger);
          log_submitter << "ADDRESS_HOME_LINE2";
          break;
        case 2:
          ApplyRationalizationsToFieldAndLog(field_index, ADDRESS_HOME_LINE3,
                                             form_interactions_ukm_logger);
          log_submitter << "ADDRESS_HOME_LINE3";
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
            FieldTypeGroup::kAddressHome &&
        fields_[field_index]->section == fields_[upper_index]->section) {
      return false;
    }
  }
  return true;
}

void FormStructure::RationalizeAddressStateCountry(
    SectionedFieldsIndexes* sections_of_state_indexes,
    SectionedFieldsIndexes* sections_of_country_indexes,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
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

    size_t upper_index = 0;
    size_t lower_index = 0;

    auto* current_section_of_state_indexes =
        sections_of_state_indexes->CurrentSection();
    auto* current_section_of_country_indexes =
        sections_of_country_indexes->CurrentSection();
    DCHECK(current_section_of_state_indexes ||
           current_section_of_country_indexes);

    // If country section is before the state ones, it means that that section
    // misses states, and the other way around.
    if (!current_section_of_country_indexes ||
        (current_section_of_state_indexes &&
         *current_section_of_state_indexes <
             *current_section_of_country_indexes)) {
      // We only rationalize when we have exactly two visible fields of a kind.
      if (current_section_of_state_indexes->size() == 2) {
        upper_index = (*current_section_of_state_indexes)[0];
        lower_index = (*current_section_of_state_indexes)[1];
      }
      sections_of_state_indexes->WalkForwardToTheNextSection();
    } else {
      // We only rationalize when we have exactly two visible fields of a kind.
      if (current_section_of_country_indexes->size() == 2) {
        upper_index = (*current_section_of_country_indexes)[0];
        lower_index = (*current_section_of_country_indexes)[1];
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
      LogRationalization(log_manager)
          << "RationalizeAddressStateCountry: Heuristics are applicable";
      continue;
    }

    if (FieldShouldBeRationalizedToCountry(upper_index)) {
      ApplyRationalizationsToFields(upper_index, lower_index,
                                    ADDRESS_HOME_COUNTRY, ADDRESS_HOME_STATE,
                                    form_interactions_ukm_logger);
      LogRationalization(log_manager) << "RationalizeAddressStateCountry: "
                                         "FieldShouldBeRationalizedToCountry";
    } else {
      ApplyRationalizationsToFields(upper_index, lower_index,
                                    ADDRESS_HOME_STATE, ADDRESS_HOME_COUNTRY,
                                    form_interactions_ukm_logger);
      LogRationalization(log_manager) << "RationalizeAddressStateCountry: "
                                         "!FieldShouldBeRationalizedToCountry";
    }
  }
}

void FormStructure::RationalizeRepeatedFields(
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  // The type of every field whose index is in
  // sectioned_field_indexes_by_type[|type|] is predicted by server as |type|.
  // Example: sectioned_field_indexes_by_type[FULL_NAME] is a sectioned fields
  // indexes of fields whose types are predicted as FULL_NAME by the server.
  SectionedFieldsIndexes sectioned_field_indexes_by_type[MAX_VALID_FIELD_TYPE];

  for (size_t i = 0; i < fields_.size(); ++i) {
    const AutofillField& field = *fields_[i];
    // The hidden fields are not considered when rationalizing.
    if (!field.IsVisible())
      continue;
    // The billing and non-billing types are aggregated.
    auto current_type = field.Type().GetStorableType();

    if (current_type != UNKNOWN_TYPE && current_type < MAX_VALID_FIELD_TYPE) {
      // Look at the sectioned field indexes for the current type, if the
      // current field belongs to that section, then the field index should be
      // added to that same section, otherwise, start a new section.
      sectioned_field_indexes_by_type[current_type].AddFieldIndex(
          i,
          /*is_new_section*/ sectioned_field_indexes_by_type[current_type]
                  .Empty() ||
              fields_[sectioned_field_indexes_by_type[current_type]
                          .LastFieldIndex()]
                      ->section != field.section);
    }
  }

  RationalizeAddressLineFields(
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_STREET_ADDRESS]),
      form_interactions_ukm_logger, log_manager);
  // Since the billing types are mapped to the non-billing ones, no need to
  // take care of ADDRESS_BILLING_STATE and .. .
  RationalizeAddressStateCountry(
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_STATE]),
      &(sectioned_field_indexes_by_type[ADDRESS_HOME_COUNTRY]),
      form_interactions_ukm_logger, log_manager);
}

void FormStructure::RationalizeFieldTypePredictions(LogManager* log_manager) {
  RationalizeCreditCardFieldPredictions(log_manager);
  for (const auto& field : fields_) {
    field->SetTypeTo(field->Type());
  }
  RationalizeTypeRelationships(log_manager);
}

void FormStructure::EncodeFormForQuery(
    AutofillPageQueryRequest* query,
    std::vector<FormSignature>* queried_form_signatures,
    std::set<FormSignature>* processed_forms) const {
  DCHECK(!IsMalformed());
  // Adds a request to |query| that contains all (|form|, |field|) for every
  // |field| from |fields_| that meets |necessary_condition|. Repeated calls for
  // the same |form| have no effect (early return if |processed_forms| contains
  // |form|).
  auto AddFormIf = [&](FormSignature form, auto necessary_condition) mutable {
    if (!processed_forms->insert(form).second)
      return;

    AutofillPageQueryRequest::Form* query_form = query->add_forms();
    query_form->set_signature(form.value());
    queried_form_signatures->push_back(form);

    for (const auto& field : fields_) {
      if (ShouldSkipField(*field) || !necessary_condition(field))
        continue;

      AutofillPageQueryRequest::Form::Field* added_field =
          query_form->add_fields();
      added_field->set_signature(field->GetFieldSignature().value());
    }
  };

  AddFormIf(form_signature(), [](auto& f) { return true; });

  if (base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    for (const auto& field : fields_) {
      if (field->host_form_signature) {
        AddFormIf(field->host_form_signature, [&](const auto& f) {
          return f->host_form_signature == field->host_form_signature;
        });
      }
    }
  }
}

// static
void FormStructure::EncodeFormFieldsForUpload(
    bool is_raw_metadata_uploading_enabled,
    absl::optional<FormGlobalId> filter_renderer_form_id,
    AutofillUploadContents* upload) const {
  DCHECK(!IsMalformed());

  for (const auto& field : fields_) {
    // Only take those fields that originate from the given renderer form.
    if (filter_renderer_form_id &&
        *filter_renderer_form_id != field->renderer_form_id()) {
      continue;
    }

    // Don't upload checkable fields.
    if (IsCheckable(field->check_status))
      continue;

    // Add the same field elements as the query and a few more below.
    if (ShouldSkipField(*field))
      continue;

    auto* added_field = upload->add_field();

    for (auto field_type : field->possible_types()) {
      added_field->add_autofill_type(field_type);
    }

    field->NormalizePossibleTypesValidities();

    for (const auto& [field_type, validities] :
         field->possible_types_validities()) {
      auto* type_validities = added_field->add_autofill_type_validities();
      type_validities->set_type(field_type);
      for (const auto& validity : validities) {
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

    if (field->single_username_vote_type()) {
      added_field->set_single_username_vote_type(
          field->single_username_vote_type().value());
    }

    if (is_raw_metadata_uploading_enabled) {
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

void FormStructure::IdentifySectionsWithNewMethod() {
  if (fields_.empty())
    return;

  const bool is_enabled_autofill_redundant_name_sectioning =
      base::FeatureList::IsEnabled(
          features::kAutofillSectionUponRedundantNameInfo);

  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  std::u16string current_section =
      GetSectionName(*fields_.front(), frame_token_ids);

  // Keep track of the types we've seen in this section.
  ServerFieldTypeSet seen_types;
  ServerFieldType previous_type = UNKNOWN_TYPE;

  // Boolean flag that is set to true when a field in the current section
  // has the autocomplete-section attribute defined.
  bool previous_autocomplete_section_present = false;

  bool is_hidden_section = false;
  std::u16string last_visible_section;
  for (const auto& field : fields_) {
    const ServerFieldType current_type = field->Type().GetStorableType();
    // All credit card fields belong to the same section that's different
    // from address sections.
    if (AutofillType(current_type).group() == FieldTypeGroup::kCreditCard) {
      field->section = "credit-card";
      continue;
    }

    bool already_saw_current_type = seen_types.count(current_type) > 0;

    // Forms often ask for multiple phone numbers -- e.g. both a daytime and
    // evening phone number.  Our phone number detection is also generally a
    // little off.  Hence, ignore this field type as a signal here.
    if (AutofillType(current_type).group() == FieldTypeGroup::kPhoneHome)
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
    bool autocomplete_section_attribute_present =
        (field->section != kDefaultSection);

    // Boolean flag that is set to true when the |field| has
    // autocomplete-section attribute defined and is different that the
    // previous field.
    bool different_autocomplete_section_than_previous =
        (autocomplete_section_attribute_present &&
         (!field_index || fields_[field_index - 1]->section != field->section));

    // Start a new section if the |current_type| was already seen or the
    // autocomplete-section attribute is defined for the |field| which is
    // different than the previous field.
    if (current_type != UNKNOWN_TYPE &&
        (already_saw_current_type ||
         different_autocomplete_section_than_previous)) {
      // Keep track of seen_types if the new section is hidden. The next
      // visible section might be the continuation of the previous visible
      // section.
      if (ignored_field) {
        is_hidden_section = true;
        last_visible_section = current_section;
      }

      if (!is_hidden_section && (!autocomplete_section_attribute_present ||
                                 different_autocomplete_section_than_previous))
        seen_types.clear();

      if (autocomplete_section_attribute_present &&
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
      current_section = GetSectionName(*field, frame_token_ids);

      // The section described in the autocomplete section attribute
      // overrides the value determined by the heuristic.
      if (autocomplete_section_attribute_present)
        current_section = base::UTF8ToUTF16(field->section);

      previous_autocomplete_section_present =
          autocomplete_section_attribute_present;
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

  // Ensure that credit card and address fields are in separate sections.
  // This simplifies the section-aware logic in autofill_manager.cc.
  for (const auto& field : fields_) {
    FieldTypeGroup field_type_group = field->Type().group();
    if (field_type_group == FieldTypeGroup::kCreditCard)
      field->section = field->section + "-cc";
    else
      field->section = field->section + "-default";
  }

  // Since this function has changed the sections, subsequent calls to
  // ParseFieldTypesFromAutocompleteAttributes(), which modifies the
  // sections, too, should not be no-ops.
  was_parsed_for_autocomplete_attributes_ = false;
}

// TODO(crbug/1153539): Make sectioning less stateful, less std::string-based.
void FormStructure::IdentifySections(bool has_author_specified_sections) {
  if (fields_.empty())
    return;

  if (base::FeatureList::IsEnabled(features::kAutofillUseNewSectioningMethod)) {
    IdentifySectionsWithNewMethod();
    return;
  }

  const bool is_enabled_autofill_redundant_name_sectioning =
      base::FeatureList::IsEnabled(
          features::kAutofillSectionUponRedundantNameInfo);

  if (!has_author_specified_sections) {
    base::flat_map<LocalFrameToken, size_t> frame_token_ids;
    std::u16string current_section =
        GetSectionName(*fields_.front(), frame_token_ids);

    // Keep track of the types we've seen in this section.
    ServerFieldTypeSet seen_types;
    ServerFieldType previous_type = UNKNOWN_TYPE;

    bool is_hidden_section = false;
    std::u16string last_visible_section;
    for (const auto& field : fields_) {
      const ServerFieldType current_type = field->Type().GetStorableType();
      // All credit card fields belong to the same section that's different
      // from address sections.
      if (AutofillType(current_type).group() == FieldTypeGroup::kCreditCard) {
        field->section = "credit-card";
        continue;
      }

      bool already_saw_current_type = seen_types.count(current_type) > 0;

      // Forms often ask for multiple phone numbers -- e.g. both a daytime and
      // evening phone number.  Our phone number detection is also generally a
      // little off.  Hence, ignore this field type as a signal here.
      if (AutofillType(current_type).group() == FieldTypeGroup::kPhoneHome)
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

      // Start a new section if the |current_type| was already seen or the
      // autocomplete-section attribute is defined for the |field| which is
      // different than the previous field.
      if (current_type != UNKNOWN_TYPE && already_saw_current_type) {
        // Keep track of seen_types if the new section is hidden. The next
        // visible section might be the continuation of the previous visible
        // section.
        if (ignored_field) {
          is_hidden_section = true;
          last_visible_section = current_section;
        }

        if (!is_hidden_section)
          seen_types.clear();

        // The end of a section, so start a new section.
        current_section = GetSectionName(*field, frame_token_ids);
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
    if (field_type_group == FieldTypeGroup::kCreditCard)
      field->section = field->section + "-cc";
    else
      field->section = field->section + "-default";
  }

  // Since this function has changed the sections, subsequent calls to
  // ParseFieldTypesFromAutocompleteAttributes(), which modifies the
  // sections, too, should not be no-ops.
  was_parsed_for_autocomplete_attributes_ = false;
}

bool FormStructure::ShouldSkipField(const FormFieldData& field) const {
  return IsCheckable(field.check_status);
}

void FormStructure::ProcessExtractedFields() {
  // Extracts the |parseable_name_| by removing common affixes from the
  // field names.
  ExtractParseableFieldNames();

  // TODO(crbug/1165780): Remove once shared labels are launched.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForParsingWithSharedLabels)) {
    // Extracts the |parsable_label_| for each field.
    ExtractParseableFieldLabels();
  }
}

void FormStructure::ExtractParseableFieldLabels() {
  std::vector<base::StringPiece16> field_labels;
  field_labels.reserve(field_count());
  for (const auto& field : *this) {
    // Skip fields that are not a text input or not visible.
    if (!field->IsTextInputElement() || !field->IsVisible()) {
      continue;
    }
    field_labels.push_back(field->label);
  }

  // Determine the parsable labels and write them back.
  absl::optional<std::vector<std::u16string>> parsable_labels =
      GetParseableLabels(field_labels);
  // If not single label was split, the function can return, because the
  // |parsable_label_| is assigned to |label| by default.
  if (!parsable_labels.has_value()) {
    return;
  }

  size_t idx = 0;
  for (auto& field : *this) {
    if (!field->IsTextInputElement() || !field->IsVisible()) {
      // For those fields, set the original label.
      field->set_parseable_label(field->label);
      continue;
    }
    DCHECK(idx < parsable_labels->size());
    field->set_parseable_label(parsable_labels->at(idx++));
  }
}

void FormStructure::ExtractParseableFieldNames() {
  // Create a vector of string pieces containing the field names.
  std::vector<base::StringPiece16> names;
  names.reserve(field_count());
  for (const auto& field : *this) {
    names.emplace_back(field->name);
  }

  // Determine the parseable names and write them into the corresponding field.
  std::vector<std::u16string> parseable_names = GetParseableNames(names);
  DCHECK_EQ(parseable_names.size(), field_count());
  size_t idx = 0;
  for (auto& field : *this) {
    field->set_parseable_name(parseable_names.at(idx++));
  }
}

DenseSet<FormType> FormStructure::GetFormTypes() const {
  DenseSet<FormType> form_types;
  for (const auto& field : fields_) {
    form_types.insert(FieldTypeGroupToFormType(field->Type().group()));
  }
  return form_types;
}

void FormStructure::set_randomized_encoder(
    std::unique_ptr<RandomizedEncoder> encoder) {
  randomized_encoder_ = std::move(encoder);
}

void FormStructure::RationalizeTypeRelationships(LogManager* log_manager) {
  // Create a local set of all the types for faster lookup.
  ServerFieldTypeSet types;
  for (const auto& field : fields_) {
    types.insert(field->Type().GetStorableType());
  }

  for (const auto& field : fields_) {
    ServerFieldType field_type = field->Type().GetStorableType();
    ServerFieldTypeSet necessary_types = GetNecessaryTypesFor(field_type);
    if (!necessary_types.empty() && !types.contains_any(necessary_types)) {
      // We have relationship rules for this type, but no `neccessary_type` was
      // found. Disabling Autofill for this field.
      field->SetTypeTo(AutofillType(UNKNOWN_TYPE));
      LogRationalization(log_manager)
          << "RationalizeTypeRelationships: Fields of type "
          << FieldTypeToStringPiece(field_type)
          << " can only exist if other fields of specific types exist.";
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
  buffer << "\n Identifiers: "
         << base::StrCat(
                {"renderer id: ",
                 base::NumberToString(form.global_id().renderer_id.value()),
                 ", host frame: ", form.global_id().frame_token.ToString(),
                 " (", url::Origin::Create(form.source_url()).Serialize(),
                 ")"});
  buffer << "\n Target URL:" << form.target_url();
  for (size_t i = 0; i < form.field_count(); ++i) {
    buffer << "\n Field " << i << ": ";
    const AutofillField* field = form.field(i);
    buffer << "\n  Identifiers:"
           << base::StrCat(
                  {"renderer id: ",
                   base::NumberToString(field->unique_renderer_id.value()),
                   ", host frame: ",
                   field->renderer_form_id().frame_token.ToString(), " (",
                   field->origin.Serialize(), "), host form renderer id: ",
                   base::NumberToString(field->host_form_id.value())});
    buffer << "\n  Signature: "
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", host form signature: ",
                   base::NumberToString(field->host_form_signature.value()),
                   " - ",
                   base::NumberToString(
                       HashFormSignature(field->host_form_signature))});
    buffer << "\n  Name: " << field->parseable_name();

    auto type = field->Type().ToString();
    auto heuristic_type = AutofillType(field->heuristic_type()).ToString();
    auto server_type = AutofillType(field->server_type()).ToString();
    if (field->server_type_prediction_is_override())
      server_type += " (manual override)";
    auto html_type_description =
        field->html_type() != HTML_TYPE_UNSPECIFIED
            ? base::StrCat(
                  {", html: ", FieldTypeToStringPiece(field->html_type())})
            : "";
    if (field->html_type() == HTML_TYPE_UNRECOGNIZED &&
        (!base::FeatureList::IsEnabled(
             features::kAutofillServerTypeTakesPrecedence) ||
         !field->server_type_prediction_is_override())) {
      html_type_description += " (disabling autofill)";
    }

    buffer << "\n  Type: "
           << base::StrCat({type, " (heuristic: ", heuristic_type, ", server: ",
                            server_type, html_type_description, ")"});
    buffer << "\n  Section: " << field->section;

    constexpr size_t kMaxLabelSize = 100;
    const std::u16string truncated_label =
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
  buffer << Tr{} << "Identifiers: "
         << base::StrCat(
                {"renderer id: ",
                 base::NumberToString(form.global_id().renderer_id.value()),
                 ", host frame: ", form.global_id().frame_token.ToString(),
                 " (", url::Origin::Create(form.source_url()).Serialize(),
                 ")"});
  buffer << Tr{} << "Target URL:" << form.target_url();
  for (size_t i = 0; i < form.field_count(); ++i) {
    buffer << Tag{"tr"};
    buffer << Tag{"td"} << "Field " << i << ": " << CTag{};
    const AutofillField* field = form.field(i);
    buffer << Tag{"td"};
    buffer << Tag{"table"};
    buffer << Tr{} << "Identifiers:"
           << base::StrCat(
                  {"renderer id: ",
                   base::NumberToString(field->unique_renderer_id.value()),
                   ", host frame: ",
                   field->renderer_form_id().frame_token.ToString(), " (",
                   field->origin.Serialize(), "), host form renderer id: ",
                   base::NumberToString(field->host_form_id.value())});
    buffer << Tr{} << "Signature:"
           << base::StrCat(
                  {base::NumberToString(field->GetFieldSignature().value()),
                   " - ",
                   base::NumberToString(
                       HashFieldSignature(field->GetFieldSignature())),
                   ", host form signature: ",
                   base::NumberToString(field->host_form_signature.value()),
                   " - ",
                   base::NumberToString(
                       HashFormSignature(field->host_form_signature))});
    buffer << Tr{} << "Name:" << field->parseable_name();

    auto type = field->Type().ToString();
    auto heuristic_type = AutofillType(field->heuristic_type()).ToString();
    auto server_type = AutofillType(field->server_type()).ToString();
    if (field->server_type_prediction_is_override())
      server_type += " (manual override)";
    auto html_type_description =
        field->html_type() != HTML_TYPE_UNSPECIFIED
            ? base::StrCat(
                  {", html: ", FieldTypeToStringPiece(field->html_type())})
            : "";
    if (field->html_type() == HTML_TYPE_UNRECOGNIZED &&
        (!base::FeatureList::IsEnabled(
             features::kAutofillServerTypeTakesPrecedence) ||
         !field->server_type_prediction_is_override())) {
      html_type_description += " (disabling autofill)";
    }

    buffer << Tr{} << "Type:"
           << base::StrCat({type, " (heuristic: ", heuristic_type, ", server: ",
                            server_type, html_type_description, ")"});
    buffer << Tr{} << "Section:" << field->section;

    constexpr size_t kMaxLabelSize = 100;
    const std::u16string truncated_label =
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
