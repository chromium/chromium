// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/version_info/version_info.h"

namespace autofill {
namespace {

// Helper for |EncodeUploadRequest()| that creates a bit field corresponding to
// |available_field_types| and returns the hex representation as a string.
std::string EncodeFieldTypes(const FieldTypeSet& available_field_types) {
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

  // Print all meaningful bytes into a string.
  std::string data_presence;
  data_presence.reserve(data_end * 2 + 1);
  for (size_t i = 0; i < data_end; ++i) {
    base::StringAppendF(&data_presence, "%02x", bit_field[i]);
  }

  return data_presence;
}

// Returns the first form field type that is not contained in |contained_types|
// or MAX_VALID_FIELD_TYPE if no such type exists.
FieldType FirstNonCapturedType(const FormStructure& form,
                               const FieldTypeSet& contained_types) {
  for (const auto& field : form) {
    for (auto type : field->possible_types()) {
      if (type != UNKNOWN_TYPE && type != EMPTY_TYPE &&
          !contained_types.count(type)) {
        return type;
      }
    }
  }
  return MAX_VALID_FIELD_TYPE;
}

void EncodeRandomizedValue(const RandomizedEncoder& encoder,
                           FormSignature form_signature,
                           FieldSignature field_signature,
                           std::string_view data_type,
                           std::string_view data_value,
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
                           std::string_view data_type,
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
  if (!FormControlTypeToString(field.form_control_type).empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_CONTROL_TYPE,
                          FormControlTypeToString(field.form_control_type),
                          /*include_checksum=*/false, metadata->mutable_type());
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
  if (!field.autocomplete_attribute.empty()) {
    EncodeRandomizedValue(
        encoder, form_signature, field_signature,
        RandomizedEncoder::FIELD_AUTOCOMPLETE, field.autocomplete_attribute,
        /*include_checksum=*/false, metadata->mutable_autocomplete());
  }
}

// Encodes the fields of `upload_fields` in the in-out parameter `upload`.
// Helper function for EncodeUploadRequest().
void EncodeFormFieldsForUpload(const FormStructure& form,
                               base::span<AutofillField*> upload_fields,
                               AutofillUploadContents* upload) {
  DCHECK(!form.IsMalformed());

  for (AutofillField* field : upload_fields) {
    // Don't upload checkable fields.
    if (IsCheckable(field->check_status)) {
      continue;
    }

    // Add the same field elements as the query and a few more below.
    if (form.ShouldSkipField(*field)) {
      continue;
    }

    // Do not upload fields that were filled with a fallback type, as this would
    // introduce unnecessary noise in the field votes.
    if (field->WasAutofilledWithFallback()) {
      continue;
    }

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
        type_validities->add_validity(base::to_underlying(validity));
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

    if (field->initial_value_changed().has_value()) {
      added_field->set_initial_value_changed(
          field->initial_value_changed().value());
    }

    added_field->set_signature(field->GetFieldSignature().value());

    if (field->properties_mask) {
      added_field->set_properties_mask(field->properties_mask);
    }

    if (form.randomized_encoder().has_value()) {
      PopulateRandomizedFieldMetadata(
          *form.randomized_encoder(), form, *field,
          added_field->mutable_randomized_field_metadata());
    }

    if (field->single_username_vote_type()) {
      added_field->set_single_username_vote_type(
          field->single_username_vote_type().value());
    }
    switch (field->is_most_recent_single_username_candidate()) {
      case IsMostRecentSingleUsernameCandidate::kNotPartOfUsernameFirstFlow:
        added_field->clear_is_most_recent_single_username_candidate();
        break;
      case IsMostRecentSingleUsernameCandidate::kHasIntermediateValuesInBetween:
        added_field->set_is_most_recent_single_username_candidate(false);
        break;
      case IsMostRecentSingleUsernameCandidate::kMostRecentCandidate:
        added_field->set_is_most_recent_single_username_candidate(true);
    }
  }
}

}  // namespace

std::vector<AutofillUploadContents> EncodeUploadRequest(
    const FormStructure& form,
    const FieldTypeSet& available_field_types,
    bool form_was_autofilled,
    const std::string_view& login_form_signature,
    bool observed_submission) {
  DCHECK_EQ(FirstNonCapturedType(form, available_field_types),
            MAX_VALID_FIELD_TYPE);

  std::string data_present = EncodeFieldTypes(available_field_types);

  AutofillUploadContents upload;
  upload.set_submission(observed_submission);
  upload.set_client_version(
      std::string(version_info::GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form.form_signature().value());
  upload.set_autofill_used(form_was_autofilled);
  upload.set_data_present(data_present);
  upload.set_has_form_tag(form.is_form_tag());
  if (!form.current_page_language()->empty() &&
      form.randomized_encoder().has_value()) {
    upload.set_language(form.current_page_language().value());
  }
  for (const auto& form_data : form.single_username_data()) {
    AutofillUploadContents::SingleUsernameData* single_username_data =
        upload.add_single_username_data();
    single_username_data->CopyFrom(form_data);
  }

  if (form.form_associations().last_address_form_submitted) {
    upload.set_last_address_form_submitted(
        form.form_associations().last_address_form_submitted->value());
  }
  if (form.form_associations().second_last_address_form_submitted) {
    upload.set_second_last_address_form_submitted(
        form.form_associations().second_last_address_form_submitted->value());
  }
  if (form.form_associations().last_credit_card_form_submitted) {
    upload.set_last_credit_card_form_submitted(
        form.form_associations().last_credit_card_form_submitted->value());
  }

  auto triggering_event =
      (form.submission_event() != mojom::SubmissionIndicatorEvent::NONE)
          ? form.submission_event()
          : ToSubmissionIndicatorEvent(form.submission_source());

  DCHECK(mojom::IsKnownEnumValue(triggering_event));
  upload.set_submission_event(
      static_cast<AutofillUploadContents_SubmissionIndicatorEvent>(
          triggering_event));

  if (!login_form_signature.empty()) {
    uint64_t login_sig;
    if (base::StringToUint64(login_form_signature, &login_sig)) {
      upload.set_login_form_signature(login_sig);
    }
  }

  if (form.IsMalformed()) {
    return {};  // Malformed form, skip it.
  }

  if (form.randomized_encoder().has_value()) {
    PopulateRandomizedFormMetadata(*form.randomized_encoder(), form,
                                   upload.mutable_randomized_form_metadata());
  }

  std::vector<AutofillField*> upload_fields(form.fields().size());
  base::ranges::transform(form.fields(), upload_fields.begin(),
                          &std::unique_ptr<AutofillField>::get);
  EncodeFormFieldsForUpload(form, upload_fields, &upload);
  std::vector<AutofillUploadContents> uploads = {std::move(upload)};

  // Build AutofillUploadContents for the renderer forms that have been
  // flattened into `this` (see the function's documentation for details).
  std::erase_if(upload_fields, [&form](const AutofillField* field) {
    // Autofill on iOS and the Password Manager in general have a null
    // FormFieldData::host_form_signature.
    return !field->host_form_signature ||
           field->host_form_signature == form.form_signature();
  });
  // Partition `upload_fields` with respect to the forms' renderer id.
  base::ranges::stable_sort(upload_fields, /*comp=*/{},
                            &FormFieldData::renderer_form_id);

  for (auto subform_begin = upload_fields.begin();
       subform_begin != upload_fields.end();) {
    AutofillUploadContents& upload_content = uploads.emplace_back();
    upload_content.set_client_version(
        std::string(version_info::GetProductNameAndVersionForUserAgent()));
    upload_content.set_form_signature(
        (*subform_begin)->host_form_signature.value());
    upload_content.set_autofill_used(form_was_autofilled);
    upload_content.set_data_present(data_present);

    auto subform_end =
        std::find_if(subform_begin, upload_fields.end(),
                     [&subform_begin](const AutofillField* field) {
                       return field->renderer_form_id() !=
                              (*subform_begin)->renderer_form_id();
                     });
    EncodeFormFieldsForUpload(form, {subform_begin, subform_end},
                              &uploads.back());
    subform_begin = subform_end;
  }
  return uploads;
}

}  // namespace autofill
