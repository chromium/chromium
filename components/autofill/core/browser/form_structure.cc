// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <stdint.h>

#include <algorithm>
#include <deque>
#include <map>
#include <memory>
#include <string>
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
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/buildflags.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/browser/form_processing/label_processing_util.h"
#include "components/autofill/core/browser/form_processing/name_processing_util.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/metrics/precedence_over_autocomplete_metrics.h"
#include "components/autofill/core/browser/metrics/shadow_prediction_metrics.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autocomplete_parsing_util.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/form_field_data_predictions.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/security_state/core/security_state.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

namespace autofill {

using mojom::SubmissionIndicatorEvent;

namespace {

// Returns true if the scheme given by |url| is one for which autofill is
// allowed to activate. By default this only returns true for HTTP and HTTPS.
bool HasAllowedScheme(const GURL& url) {
  return url.SchemeIsHTTPOrHTTPS() ||
         base::FeatureList::IsEnabled(
             features::test::kAutofillAllowNonHttpActivation);
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

  // Print all meaningful bytes into a string.
  std::string data_presence;
  data_presence.reserve(data_end * 2 + 1);
  for (size_t i = 0; i < data_end; ++i) {
    base::StringAppendF(&data_presence, "%02x", bit_field[i]);
  }

  return data_presence;
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
    case PasswordAttribute::kHasLetter:
      upload->set_password_has_letter(password_attributes_vote.second);
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
  if (!field.autocomplete_attribute.empty()) {
    EncodeRandomizedValue(
        encoder, form_signature, field_signature,
        RandomizedEncoder::FIELD_AUTOCOMPLETE, field.autocomplete_attribute,
        /*include_checksum=*/false, metadata->mutable_autocomplete());
  }
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
  SetFieldTypesFromAutocompleteAttribute();
  DetermineFieldRanks();
}

FormStructure::FormStructure(
    FormSignature form_signature,
    const std::vector<FieldSignature>& field_signatures)
    : form_signature_(form_signature) {
  for (const auto& signature : field_signatures)
    fields_.push_back(AutofillField::CreateForPasswordManagerUpload(signature));
  DetermineFieldRanks();
}

FormStructure::~FormStructure() = default;

void FormStructure::DetermineFieldRanks() {
  size_t rank = 0;
  std::map<FormGlobalId, size_t> rank_in_host_form;
  std::map<FieldSignature, size_t> rank_in_signature_group;
  std::map<std::pair<FormGlobalId, FieldSignature>, size_t>
      rank_in_host_form_signature_group;

  for (auto& field : fields_) {
    field->set_rank(rank++);
    field->set_rank_in_host_form(
        rank_in_host_form[field->renderer_form_id()]++);
    field->set_rank_in_signature_group(
        rank_in_signature_group[field->GetFieldSignature()]++);
    field->set_rank_in_host_form_signature_group(
        rank_in_host_form_signature_group[std::make_pair(
            field->renderer_form_id(), field->GetFieldSignature())]++);
  }
}

void FormStructure::DetermineHeuristicTypes(
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
    LogManager* log_manager) {
  SCOPED_UMA_HISTOGRAM_TIMER("Autofill.Timing.DetermineHeuristicTypes");

  ParseFieldTypesWithPatterns(GetActivePatternSource(), log_manager);
  if (!base::FeatureList::IsEnabled(
          features::kAutofillDisableShadowHeuristics)) {
    for (PatternSource shadow_source : GetNonActivePatternSources())
      ParseFieldTypesWithPatterns(shadow_source, log_manager);
  }

  UpdateAutofillCount();
  IdentifySections(/*ignore_autocomplete=*/false);

  FormStructureRationalizer rationalizer(&fields_);
  rationalizer.RationalizeAutocompleteAttributes(log_manager);
  if (base::FeatureList::IsEnabled(features::kAutofillPageLanguageDetection)) {
    rationalizer.RationalizeRepeatedFields(
        form_signature_, form_interactions_ukm_logger, log_manager);
  }
  rationalizer.RationalizeFieldTypePredictions(main_frame_origin_, log_manager);

  // Log the field type predicted by rationalization.
  // The sections are mapped to consecutive natural numbers starting at 1.
  std::map<Section, size_t> section_id_map;
  for (const auto& field : fields_) {
    if (!base::Contains(section_id_map, field->section)) {
      size_t next_section_id = section_id_map.size() + 1;
      section_id_map[field->section] = next_section_id;
    }
    field->AppendLogEventIfNotRepeated(RationalizationFieldLogEvent{
        .field_type = field->Type().GetStorableType(),
        .section_id = section_id_map[field->section],
        .type_changed = field->Type().GetStorableType() !=
                        field->ComputedType().GetStorableType(),
    });
  }

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
      std::string(version_info::GetProductNameAndVersionForUserAgent()));
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

  if (form_associations_.last_address_form_submitted) {
    upload.set_last_address_form_submitted(
        form_associations_.last_address_form_submitted->value());
  }
  if (form_associations_.second_last_address_form_submitted) {
    upload.set_second_last_address_form_submitted(
        form_associations_.second_last_address_form_submitted->value());
  }
  if (form_associations_.last_credit_card_form_submitted) {
    upload.set_last_credit_card_form_submitted(
        form_associations_.last_credit_card_form_submitted->value());
  }

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
    // Autofill on iOS and the Password Manager in general have a null
    // FormFieldData::host_form_signature.
    if (field->host_form_signature &&
        field->host_form_signature != form_signature()) {
      subforms.emplace_back(field->renderer_form_id(),
                            field->host_form_signature);
    }
  }
  for (const auto& [subform_id, subform_signature] :
       base::flat_map<FormGlobalId, FormSignature>(std::move(subforms))) {
    uploads.emplace_back();
    uploads.back().set_client_version(
        std::string(version_info::GetProductNameAndVersionForUserAgent()));
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
      std::string(version_info::GetProductNameAndVersionForUserAgent()));

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
  LOG_AF(log_manager) << LoggingScope::kParsing
                      << LogMessage::kProcessingServerData;

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
    // Fields can share the same field signature. This map records for each
    // signature how many fields with the same signature have been observed.
    std::map<FieldSignature, size_t> field_rank_map;
    for (auto& field : form->fields_) {
      // Get the field prediction for |form|'s signature and the |field|'s
      // host_form_signature. The precedence rule is the following:
      // 1) Server overrides on main frame first, then iframe.
      // 2) Server crowdsourcing on main frame first, then iframe.
      absl::optional<FieldSuggestion> current_field =
          GetPrediction(form->form_signature(), field->GetFieldSignature());
      auto is_override = [](absl::optional<FieldSuggestion> field_suggestion) {
        return field_suggestion && !field_suggestion->predictions().empty() &&
               field_suggestion->predictions()[0].override();
      };
      if (field->host_form_signature &&
          field->host_form_signature != form->form_signature() &&
          !is_override(current_field)) {
        // Retrieves the alternative prediction even if it is not used so that
        // the alternative predictions are popped.
        absl::optional<FieldSuggestion> alternative_field = GetPrediction(
            field->host_form_signature, field->GetFieldSignature());
        if (alternative_field &&
            (!current_field || is_override(alternative_field) ||
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

      ++field_rank_map[field->GetFieldSignature()];
      // Log the field type predicted from Autofill crowdsourced server.
      field->AppendLogEventIfNotRepeated(ServerPredictionFieldLogEvent{
          .server_type1 = field->server_type(),
          .prediction_source1 = field->server_predictions().empty()
                                    ? FieldPrediction::SOURCE_UNSPECIFIED
                                    : field->server_predictions()[0].source(),
          .server_type2 =
              field->server_predictions().size() >= 2
                  ? ToSafeServerFieldType(field->server_predictions()[1].type(),
                                          NO_SERVER_DATA)
                  : NO_SERVER_DATA,
          .prediction_source2 = field->server_predictions().size() >= 2
                                    ? field->server_predictions()[1].source()
                                    : FieldPrediction::SOURCE_UNSPECIFIED,
          .server_type_prediction_is_override =
              field->server_type_prediction_is_override(),
          .rank_in_field_signature_group =
              field_rank_map[field->GetFieldSignature()],
      });
    }

    AutofillMetrics::LogServerResponseHasDataForForm(base::ranges::any_of(
        form->fields_, [](ServerFieldType t) { return t != NO_SERVER_DATA; },
        &AutofillField::server_type));

    form->UpdateAutofillCount();
    FormStructureRationalizer rationalizer(&form->fields_);
    rationalizer.RationalizeAutocompleteAttributes(log_manager);
    rationalizer.RationalizeRepeatedFields(
        form->form_signature_, form_interactions_ukm_logger, log_manager);
    rationalizer.RationalizeFieldTypePredictions(form->main_frame_origin_,
                                                 log_manager);
    // TODO(crbug.com/1154080): By calling this with true, autocomplete section
    // attributes will be ignored.
    form->IdentifySections(/*ignore_autocomplete=*/true);
    // Metrics are intentionally only emitted after the sever response, not when
    // determining heuristic types. This is done to reduce noise in the metrics,
    // since generally only this sectioning result is used.
    LogSectioningMetrics(form->form_signature(), form->fields_,
                         form_interactions_ukm_logger);

    // Log the field type predicted by rationalization.
    // The sections are mapped to consecutive natural numbers starting at 1.
    std::map<Section, size_t> section_id_map;
    for (const auto& field : form->fields_) {
      if (!base::Contains(section_id_map, field->section)) {
        size_t next_section_id = section_id_map.size() + 1;
        section_id_map[field->section] = next_section_id;
      }
      field->AppendLogEventIfNotRepeated(RationalizationFieldLogEvent{
          .field_type = field->Type().GetStorableType(),
          .section_id = section_id_map[field->section],
          .type_changed = field->Type().GetStorableType() !=
                          field->ComputedType().GetStorableType(),
      });
    }
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
      annotated_field.section = field->section.ToString();
      annotated_field.rank = field->rank();
      annotated_field.rank_in_signature_group =
          field->rank_in_signature_group();
      annotated_field.rank_in_host_form = field->rank_in_host_form();
      annotated_field.rank_in_host_form_signature_group =
          field->rank_in_host_form_signature_group();
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

bool FormStructure::ShouldBeParsed(ShouldBeParsedParams params,
                                   LogManager* log_manager) const {
  // Exclude URLs not on the web via HTTP(S).
  if (!HasAllowedScheme(source_url_)) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotAllowedScheme << *this;
    return false;
  }

  if (active_field_count() < params.min_required_fields &&
      (!all_fields_are_passwords() ||
       active_field_count() <
           params.required_fields_for_forms_with_only_password_fields) &&
      !has_author_specified_types_) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingNotEnoughFields
                        << active_field_count() << *this;
    return false;
  }

  // Rule out search forms.
  if (MatchesRegex<kUrlSearchActionRe>(
          base::UTF8ToUTF16(target_url_.path_piece()))) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingUrlMatchesSearchRegex
                        << *this;
    return false;
  }

  bool has_text_field = base::ranges::any_of(*this, [](const auto& field) {
    return !field->IsSelectOrSelectMenuElement();
  });
  if (!has_text_field) {
    LOG_AF(log_manager) << LoggingScope::kAbortParsing
                        << LogMessage::kAbortParsingFormHasNoTextfield << *this;
  }
  return has_text_field;
}

bool FormStructure::ShouldRunHeuristics() const {
  return active_field_count() >= kMinRequiredFieldsForHeuristics &&
         HasAllowedScheme(source_url_);
}

bool FormStructure::ShouldRunHeuristicsForSingleFieldForms() const {
  return active_field_count() > 0 && HasAllowedScheme(source_url_);
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

void FormStructure::RetrieveFromCache(const FormStructure& cached_form,
                                      RetrieveFromCacheReason reason) {
  // Build a table to lookup AutofillFields by their FieldGlobalId.
  std::map<FieldGlobalId, const AutofillField*> cached_fields_by_id;
  for (const std::unique_ptr<autofill::AutofillField>& field : cached_form)
    cached_fields_by_id[field->global_id()] = field.get();

  // Lookup field by global_id in cached_fields_by_id.
  auto find_field_by_id = [&cached_fields_by_id](FieldGlobalId global_id) {
    const auto& it = cached_fields_by_id.find(global_id);
    return it != cached_fields_by_id.end() ? it->second : nullptr;
  };

  // Lookup field by field signature and return it in case only a single field
  // with the signature exists.
  auto find_field_with_unique_field_signature =
      [&cached_fields_by_id](
          FieldSignature field_signature) -> const AutofillField* {
    const AutofillField* match = nullptr;
    // Iterate over the fields to find the field with the same form signature.
    for (const auto& entry : cached_fields_by_id) {
      if (entry.second->GetFieldSignature() == field_signature) {
        // If there are multiple matches, do not retrieve the field and stop
        // the process.
        if (match)
          return nullptr;
        match = entry.second;
      }
    }
    return match;
  };

  for (auto& field : *this) {
    const AutofillField* cached_field = find_field_by_id(field->global_id());

    // If the unique renderer id (or the name) is not stable due to some Java
    // Script magic in the website, use the field signature as a fallback
    // solution to find the field in the cached form.
    if (!cached_field) {
      cached_field =
          find_field_with_unique_field_signature(field->GetFieldSignature());
    }

    // Skip fields that we could not find.
    if (!cached_field)
      continue;

    switch (reason) {
      case RetrieveFromCacheReason::kFormParsing:
        // During form parsing (as in "assigning field types to fields")
        // the `value` represents the initial value found at page load and needs
        // to be preserved.
        if (!field->IsSelectOrSelectMenuElement()) {
          field->value = cached_field->value;
          value_from_dynamic_change_form_ = true;
        }
        break;
      case RetrieveFromCacheReason::kFormImport:
        // From the perspective of learning user data, text fields containing
        // default values are equivalent to empty fields. So if the value of
        // a submitted form corresponds to the initial value of the field, we
        // clear that value.
        // Since a website can prefill country and state values based on
        // GeoIP, we want to hold on to these values.
        const bool same_value_as_on_page_load =
            field->value == cached_field->value;
        const bool field_is_neither_state_nor_country =
            field->server_type() != ADDRESS_HOME_COUNTRY &&
            field->server_type() != ADDRESS_HOME_STATE;
        if (!field->IsSelectOrSelectMenuElement() &&
            same_value_as_on_page_load && field_is_neither_state_nor_country) {
          field->value = std::u16string();
        }
        break;
    }

    field->set_server_predictions(cached_field->server_predictions());

    // TODO(crbug.com/1373362): The following is the statement which we want
    // to have here once features::kAutofillDontPreserveAutofillState is
    // launched:
    // ---
    // We don't preserve the `is_autofilled` state from the cache, because
    // form parsing and form import both start in the renderer and the renderer
    // shares it's most recent status of whether the fields are currently
    // in autofilled state. Any modifications by JavaScript or the user
    // may take a field out of the autofilled state and get propagated to the
    // AutofillManager via OnTextFieldDidChangeImpl anyways.
    // ---
    // For now we gate this behavioral change by a feature flag to ensure that
    // it does not cause a regression.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillDontPreserveAutofillState)) {
      // Preserve state whether the field was autofilled before.
      if (reason == RetrieveFromCacheReason::kFormParsing)
        field->is_autofilled = cached_field->is_autofilled;
    }

    if (cached_field->autofill_source_profile_guid()) {
      field->set_autofill_source_profile_guid(
          *cached_field->autofill_source_profile_guid());
    }
    field->set_previously_autofilled(cached_field->previously_autofilled());
    field->set_was_context_menu_shown(cached_field->was_context_menu_shown());
    if (cached_field->value_not_autofilled_over_existing_value_hash()) {
      field->set_value_not_autofilled_over_existing_value_hash(
          *cached_field->value_not_autofilled_over_existing_value_hash());
    }

    // During form parsing, we don't care for heuristic field classifications
    // and information derived from the autocomplete attribute as those are
    // either regenerated or copied from the form that the renderer sent.
    // During import, no parsing happens and we want to preserve the last field
    // classification.
    if (reason == RetrieveFromCacheReason::kFormImport) {
      // Transfer attributes of the cached AutofillField to the newly created
      // AutofillField.
      for (int i = 0; i <= static_cast<int>(PatternSource::kMaxValue); ++i) {
        PatternSource s = static_cast<PatternSource>(i);
        field->set_heuristic_type(s, cached_field->heuristic_type(s));
      }
      field->SetHtmlType(cached_field->html_type(), cached_field->html_mode());
      field->section = cached_field->section;
      field->set_only_fill_when_focused(cached_field->only_fill_when_focused());

      // During import, the final field type is used to decide which
      // information to store in an address profile or credit card. As
      // rationalization is an important component of determining the final
      // field type, the output should be preserved.
      field->SetTypeTo(cached_field->Type());
    }
    field->set_field_log_events(cached_field->field_log_events());
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

void FormStructure::SetFieldTypesFromAutocompleteAttribute() {
  has_author_specified_types_ = false;
  has_author_specified_upi_vpa_hint_ = false;
  std::map<FieldSignature, size_t> field_rank_map;
  for (const std::unique_ptr<AutofillField>& field : fields_) {
    if (!field->parsed_autocomplete)
      continue;

    // A parsable autocomplete value was specified. Even an invalid field_type
    // is considered a type hint. This allows a website's author to specify an
    // attribute like autocomplete="other" on a field to disable all Autofill
    // heuristics for the form.
    has_author_specified_types_ = true;
    if (field->parsed_autocomplete->field_type == HtmlFieldType::kUnspecified)
      continue;

    // TODO(crbug.com/702223): Flesh out support for UPI-VPA.
    if (field->parsed_autocomplete->field_type == HtmlFieldType::kUpiVpa) {
      has_author_specified_upi_vpa_hint_ = true;
      field->parsed_autocomplete->field_type = HtmlFieldType::kUnrecognized;
    }

    field->SetHtmlType(field->parsed_autocomplete->field_type,
                       field->parsed_autocomplete->mode);

    // Log the field type predicted from autocomplete attribute.
    ++field_rank_map[field->GetFieldSignature()];
    field->AppendLogEventIfNotRepeated(AutocompleteAttributeFieldLogEvent{
        .html_type = field->parsed_autocomplete->field_type,
        .html_mode = field->parsed_autocomplete->mode,
        .rank_in_field_signature_group =
            field_rank_map[field->GetFieldSignature()],
    });
  }
}

bool FormStructure::SetSectionsFromAutocompleteOrReset() {
  bool has_autocomplete = false;
  for (const auto& field : fields_) {
    if (!field->parsed_autocomplete) {
      field->section = Section();
      continue;
    }

    field->section = Section::FromAutocomplete(
        {.section = field->parsed_autocomplete->section,
         .mode = field->parsed_autocomplete->mode});
    if (field->section)
      has_autocomplete = true;
  }
  return has_autocomplete;
}

void FormStructure::ParseFieldTypesWithPatterns(PatternSource pattern_source,
                                                LogManager* log_manager) {
  FieldCandidatesMap field_type_map;
  if (ShouldRunHeuristics()) {
    FormField::ParseFormFields(fields_, current_page_language_, is_form_tag_,
                               pattern_source, field_type_map, log_manager);
  } else if (ShouldRunHeuristicsForSingleFieldForms()) {
    FormField::ParseSingleFieldForms(fields_, current_page_language_,
                                     is_form_tag_, pattern_source,
                                     field_type_map, log_manager);
  }
  if (field_type_map.empty())
    return;

  // Fields can share the same field signature. This map records for each
  // signature how many fields with the same signature have been observed.
  std::map<FieldSignature, size_t> field_rank_map;
  for (const auto& field : fields_) {
    auto iter = field_type_map.find(field->global_id());
    if (iter == field_type_map.end())
      continue;
    const FieldCandidates& candidates = iter->second;
    field->set_heuristic_type(pattern_source, candidates.BestHeuristicType());

    ++field_rank_map[field->GetFieldSignature()];
    // Log the field type predicted from local heuristics.
    field->AppendLogEventIfNotRepeated(HeuristicPredictionFieldLogEvent{
        .field_type = field->heuristic_type(pattern_source),
        .pattern_source = pattern_source,
        .is_active_pattern_source = GetActivePatternSource() == pattern_source,
        .rank_in_field_signature_group =
            field_rank_map[field->GetFieldSignature()],
    });
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
  return const_cast<AutofillField*>(std::as_const(*this).field(index));
}

size_t FormStructure::field_count() const {
  return fields_.size();
}

const AutofillField* FormStructure::GetFieldById(FieldGlobalId field_id) const {
  auto it = base::ranges::find(
      fields_, field_id, [](const auto& field) { return field->global_id(); });
  return it != fields_.end() ? it->get() : nullptr;
}

AutofillField* FormStructure::GetFieldById(FieldGlobalId field_id) {
  return const_cast<AutofillField*>(
      std::as_const(*this).GetFieldById(field_id));
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

  for (const auto& field : fields_) {
    if (field->host_form_signature) {
      AddFormIf(field->host_form_signature, [&](const auto& f) {
        return f->host_form_signature == field->host_form_signature;
      });
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

  // Use unique local frame tokens of the fields to generate sections.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;

  SetSectionsFromAutocompleteOrReset();

  // Section for non-credit card fields.
  Section current_section;
  Section credit_card_section;

  // Keep track of the types we've seen in this section.
  ServerFieldTypeSet seen_types;
  ServerFieldType previous_type = UNKNOWN_TYPE;

  // Boolean flag that is set to true when a field in the current section
  // has the autocomplete-section attribute defined.
  bool previous_autocomplete_section_present = false;

  bool is_hidden_section = false;
  Section last_visible_section;
  for (const auto& field : fields_) {
    const ServerFieldType current_type = field->Type().GetStorableType();
    // Put credit card fields into one, separate credit card section.
    if (AutofillType(current_type).group() == FieldTypeGroup::kCreditCard) {
      if (!credit_card_section) {
        credit_card_section =
            Section::FromFieldIdentifier(*field, frame_token_ids);
      }
      field->section = credit_card_section;
      continue;
    }

    if (!current_section)
      current_section = Section::FromFieldIdentifier(*field, frame_token_ids);

    bool already_saw_current_type = seen_types.count(current_type) > 0;

    // Forms often ask for multiple phone numbers -- e.g. both a daytime and
    // evening phone number.  Our phone number detection is also generally a
    // little off.  Hence, ignore this field type as a signal here.
    if (AutofillType(current_type).group() == FieldTypeGroup::kPhoneHome)
      already_saw_current_type = false;

    bool ignored_field = !field->IsFocusable();

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

    // Boolean flag that is set to true when the section of the `field` is
    // derived from the autocomplete attribute and its section is different than
    // the previous field's section.
    bool different_autocomplete_section_than_previous_field_section =
        field->section.is_from_autocomplete() &&
        (field_index == 0 ||
         fields_[field_index - 1]->section != field->section);

    // Start a new section if the `current_type` was already seen or the section
    // is derived from the autocomplete attribute which is different than the
    // previous field's section.
    if (current_type != UNKNOWN_TYPE &&
        (already_saw_current_type ||
         different_autocomplete_section_than_previous_field_section)) {
      // Keep track of seen_types if the new section is hidden. The next
      // visible section might be the continuation of the previous visible
      // section.
      if (ignored_field) {
        is_hidden_section = true;
        last_visible_section = current_section;
      }

      if (!is_hidden_section &&
          (!field->section.is_from_autocomplete() ||
           different_autocomplete_section_than_previous_field_section)) {
        seen_types.clear();
      }

      if (field->section.is_from_autocomplete() &&
          !previous_autocomplete_section_present) {
        // If this field is the first field within the section with a defined
        // autocomplete section, then change the section attribute of all the
        // parsed fields in the current section to `field->section`.
        int i = static_cast<int>(field_index - 1);
        while (i >= 0 && fields_[i]->section == current_section) {
          fields_[i]->section = field->section;
          i--;
        }
      }

      // The end of a section, so start a new section.
      current_section = Section::FromFieldIdentifier(*field, frame_token_ids);

      // The section described in the autocomplete section attribute
      // overrides the value determined by the heuristic.
      if (field->section.is_from_autocomplete())
        current_section = field->section;

      previous_autocomplete_section_present =
          field->section.is_from_autocomplete();
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

    field->section = current_section;
  }
}

void FormStructure::IdentifySections(bool ignore_autocomplete) {
  if (fields_.empty())
    return;

  if (base::FeatureList::IsEnabled(features::kAutofillUseNewSectioningMethod)) {
    IdentifySectionsWithNewMethod();
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillUseParameterizedSectioning)) {
    AssignSections(fields_);
    return;
  }

  // Use unique local frame tokens of the fields to generate sections.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;

  bool has_autocomplete = SetSectionsFromAutocompleteOrReset();

  // Put credit card fields into one, separate section.
  Section credit_card_section;
  for (const auto& field : fields_) {
    if (field->Type().group() == FieldTypeGroup::kCreditCard) {
      if (!credit_card_section) {
        credit_card_section =
            Section::FromFieldIdentifier(*field, frame_token_ids);
      }
      field->section = credit_card_section;
    }
  }

  if (ignore_autocomplete || !has_autocomplete) {
    // Section for non-credit card fields.
    Section current_section;

    // Keep track of the types we've seen in this section.
    ServerFieldTypeSet seen_types;
    ServerFieldType previous_type = UNKNOWN_TYPE;

    bool is_hidden_section = false;
    Section last_visible_section;
    for (const auto& field : fields_) {
      const ServerFieldType current_type = field->Type().GetStorableType();
      // Credit card fields are already in one, separate credit card section.
      if (AutofillType(current_type).group() == FieldTypeGroup::kCreditCard)
        continue;

      if (!current_section)
        current_section = Section::FromFieldIdentifier(*field, frame_token_ids);

      bool already_saw_current_type = seen_types.count(current_type) > 0;

      // Forms often ask for multiple phone numbers -- e.g. both a daytime and
      // evening phone number.  Our phone number detection is also generally a
      // little off.  Hence, ignore this field type as a signal here.
      if (AutofillType(current_type).group() == FieldTypeGroup::kPhoneHome)
        already_saw_current_type = false;

      bool ignored_field = !field->IsFocusable();

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

      // Start a new section if the |current_type| was already seen.
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
        current_section = Section::FromFieldIdentifier(*field, frame_token_ids);
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

      field->section = current_section;
    }
  }
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
    if (!field->IsTextInputElement() || !field->IsFocusable()) {
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
    if (!field->IsTextInputElement() || !field->IsFocusable()) {
      // For those fields, set the original label.
      field->set_parseable_label(field->label);
      continue;
    }
    DCHECK(idx < parsable_labels->size());
    field->set_parseable_label(parsable_labels->at(idx++));
  }
}

void FormStructure::ExtractParseableFieldNames() {
  std::vector<base::StringPiece16> names;
  names.reserve(field_count());
  for (const auto& field : *this)
    names.emplace_back(field->name);

  // Determine the parseable names and write them into the corresponding field.
  ComputeParseableNames(names);
  size_t idx = 0;
  for (auto& field : *this)
    field->set_parseable_name(std::u16string(names[idx++]));
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

void FormStructure::RationalizePhoneNumbersInSection(const Section& section) {
  if (base::Contains(phone_rationalized_, section))
    return;
  FormStructureRationalizer rationalizer(&fields_);
  rationalizer.RationalizePhoneNumbersInSection(section);
  phone_rationalized_.insert(section);
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
        field->html_type() != HtmlFieldType::kUnspecified
            ? base::StrCat(
                  {", html: ", FieldTypeToStringPiece(field->html_type())})
            : "";
    if (field->html_type() == HtmlFieldType::kUnrecognized &&
        !field->server_type_prediction_is_override()) {
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
    buffer << Tr{} << "Placeholder:" << field->placeholder;

    auto type = field->Type().ToString();
    auto heuristic_type = AutofillType(field->heuristic_type()).ToString();
    auto server_type = AutofillType(field->server_type()).ToString();
    if (field->server_type_prediction_is_override())
      server_type += " (manual override)";
    auto html_type_description =
        field->html_type() != HtmlFieldType::kUnspecified
            ? base::StrCat(
                  {", html: ", FieldTypeToStringPiece(field->html_type())})
            : "";
    if (field->html_type() == HtmlFieldType::kUnrecognized &&
        !field->server_type_prediction_is_override()) {
      html_type_description += " (disabling autofill)";
    }

    buffer << Tr{} << "Type:"
           << base::StrCat({type, " (heuristic: ", heuristic_type, ", server: ",
                            server_type, html_type_description, ")"});
    buffer << Tr{} << "Section:" << field->section;

    constexpr size_t kMaxLabelSize = 100;
    // TODO(crbug/1165780): Remove once shared labels are launched.
    const std::u16string& label =
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForParsingWithSharedLabels)
            ? field->parseable_label()
            : field->label;
    const std::u16string truncated_label =
        label.substr(0, std::min(label.length(), kMaxLabelSize));
    buffer << Tr{} << "Label:" << truncated_label;

    buffer << Tr{} << "Is empty:" << (field->IsEmpty() ? "Yes" : "No");
    buffer << Tr{} << "Is focusable:"
           << (field->IsFocusable() ? "Yes (focusable)" : "No (unfocusable)");
    buffer << Tr{} << "Is visible:"
           << (field->is_visible ? "Yes (visible)" : "No (invisible)");
    buffer << Tr{} << "Ranks: "
           << base::StringPrintf(
                  "Field rank: %zu, rank in signature group: %zu, "
                  "field rank in host form: %zu, rank in host form signature "
                  "group: %zu",
                  field->rank(), field->rank_in_signature_group(),
                  field->rank_in_host_form(),
                  field->rank_in_host_form_signature_group());
    buffer << CTag{"table"};
    buffer << CTag{"td"};
    buffer << CTag{"tr"};
  }
  buffer << CTag{"table"};
  buffer << CTag{"div"};
  return buffer;
}

}  // namespace autofill
