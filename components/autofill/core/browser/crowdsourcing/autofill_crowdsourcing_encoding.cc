// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/crowdsourcing/randomized_encoder.h"
#include "components/autofill/core/browser/crowdsourcing/server_prediction_overrides.h"
#include "components/autofill/core/browser/data_quality/validation.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/autofill/core/common/signatures.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace autofill {
namespace {

std::ostream& operator<<(std::ostream& out,
                         const AutofillQueryResponse& response) {
  for (const auto& form : response.form_suggestions()) {
    out << "\nForm";
    for (const auto& field : form.field_suggestions()) {
      out << "\n Field\n  signature: " << field.field_signature();
      for (const auto& prediction : field.predictions()) {
        out << "\n  prediction: " << prediction.type();
      }
    }
  }
  return out;
}

// Returns a `FieldPrediction::Source` that is guaranteed to be in the bounds
// of the enum.
FieldPrediction::Source ToSafeFieldPredictionSource(
    FieldPrediction::Source source) {
  FieldPrediction::Source result = FieldPrediction::SOURCE_UNSPECIFIED;
  switch (source) {
    case FieldPrediction::SOURCE_UNSPECIFIED:
    case FieldPrediction::SOURCE_AUTOFILL_DEFAULT:
    case FieldPrediction::SOURCE_PASSWORDS_DEFAULT:
    case FieldPrediction::SOURCE_ALL_APPROVED_EXPERIMENTS:
    case FieldPrediction::SOURCE_FIELD_RANKS:
    case FieldPrediction::SOURCE_OVERRIDE:
    case FieldPrediction::SOURCE_MANUAL_OVERRIDE:
    case FieldPrediction::SOURCE_AUTOFILL_COMBINED_TYPES:
    case FieldPrediction::SOURCE_AUTOFILL_AI:
    case FieldPrediction::SOURCE_AUTOFILL_AI_CROWDSOURCING:
      result = source;
      break;
  }
  return result;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Merges manual and server type predictions.
//
// The logic to merge manual and server overrides (which may differ in length),
// is as follows:
// * Both overrides assume the same field order, so iterate through the manual
//   overrides.
// * If the manual override has at least one field prediction, use the manual
//   override instead of the server override. Pop the server override, but only
//   if there are at least two server overrides left. This is because the last
//   server override may be used for multiple fields (`GetPrediction` inside
//   `ProcessServerPredictionsQueryResponse` will keep returning the last value)
//   and we only wish to override the prediction for the current field.
// * If the manual override has no specified field prediction (i.e. is a "pass
//   through"), then it was not intended to override this specific prediction.
//   In that case, use the server prediction instead. In the special case that
//   the last specified manual override is a pass through, copy all server
//   predictions.
std::deque<FieldSuggestion> MergeManualAndServerOverrides(
    std::deque<FieldSuggestion> manual_overrides,
    std::deque<FieldSuggestion> server_overrides) {
  std::deque<FieldSuggestion> result;
  while (!manual_overrides.empty() && !server_overrides.empty()) {
    // If the manual override has a no type or format string specified, it means
    // that the server prediction should be used.
    result.push_back(!manual_overrides.front().predictions().empty() ||
                             manual_overrides.front().has_format_string()
                         ? manual_overrides.front()
                         : server_overrides.front());

    manual_overrides.pop_front();
    // Generally consume the first element of each override source. However,
    // the last server override can apply to multiple fields with the same
    // signature, so we do not pop it while it is still useful.
    if (server_overrides.size() > 1 || manual_overrides.empty()) {
      server_overrides.pop_front();
    }
  }
  // At most one override source is non-empty - preserve the values.
  std::ranges::move(manual_overrides, std::back_inserter(result));
  std::ranges::move(server_overrides, std::back_inserter(result));

  return result;
}

// Applies manual overrides from `parsed_overrides` to `field_types`.
void InsertParsedOverrides(
    base::expected<ServerPredictionOverrides, std::string> parsed_overrides,
    std::map<std::pair<FormSignature, FieldSignature>,
             std::deque<FieldSuggestion>>& field_types) {
  if (!parsed_overrides.has_value()) {
    LOG(ERROR) << parsed_overrides.error();
    return;
  }
  for (auto& [key, value] : parsed_overrides.value()) {
    field_types.insert_or_assign(
        key,
        MergeManualAndServerOverrides(/*manual_overrides=*/std::move(value),
                                      /*server_overrides=*/field_types[key]));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Helper for `EncodeUploadRequest()` that creates a bit field corresponding to
// `available_field_types` and returns the hex representation as a string.
std::string EncodeFieldTypes(const FieldTypeSet& available_field_types) {
  // There are `MAX_VALID_FIELD_TYPE` different field types and 8 bits per byte,
  // so we need ceil(MAX_VALID_FIELD_TYPE / 8) bytes to encode the bit field.
  constexpr size_t kNumBytes = (MAX_VALID_FIELD_TYPE + 0x7) / 8;

  // Pack the types in `available_field_types` into `bit_field`.
  std::array<uint8_t, kNumBytes> bit_field = {};
  for (const auto field_type : available_field_types) {
    // Set the appropriate bit in the field. The bit we set is the one
    // `field_type` % 8 from the left of the byte.
    const size_t byte = field_type / 8;
    const uint8_t bit = 1 << (7 - field_type % 8);
    bit_field[byte] |= bit;
  }

  // Discard any trailing zeroes.
  // If there are no available types, we return the empty string.
  size_t data_end = bit_field.size();
  while (data_end > 0 && !bit_field[data_end - 1]) {
    --data_end;
  }

  // Print all meaningful bytes into a string.
  std::string data_presence;
  data_presence.reserve(data_end * 2);
  for (size_t i = 0; i < data_end; ++i) {
    absl::StrAppendFormat(&data_presence, "%02x", bit_field[i]);
  }

  return data_presence;
}

// Returns the first form field type that is not contained in `contained_types`
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

// Returns true if the form has no fields, or too many.
bool IsMalformed(const FormStructure& form) {
  // Some badly formatted web sites repeat fields - limit number of fields to
  // 250, which is far larger than any valid form and proto still fits into 10K.
  // Do not send requests for forms with more than this many fields, as they are
  // near certainly not valid/auto-fillable.
  return form.field_count() == 0 || form.field_count() > 250;
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
    DCHECK(data_type == RandomizedEncoder::kFormUrl);
    output->set_checksum(StrToHash32Bit(data_value));
  }
}

void EncodeRandomizedValue(const RandomizedEncoder& encoder,
                           FormSignature form_signature,
                           FieldSignature field_signature,
                           std::string_view data_type,
                           std::u16string_view data_value,
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
                          RandomizedEncoder::kFormId, form.id_attribute(),
                          /*include_checksum=*/false, metadata->mutable_id());
  }
  if (!form.name_attribute().empty()) {
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::kFormName, form.name_attribute(),
                          /*include_checksum=*/false, metadata->mutable_name());
  }

  for (const auto& [title, title_type] : form.button_titles()) {
    auto* button_title = metadata->add_button_title();
    DCHECK(!title.empty());
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::kFormButtonTitles, title,
                          /*include_checksum=*/false,
                          button_title->mutable_title());
    button_title->set_type(static_cast<ButtonTitleType>(title_type));
  }
  auto full_source_url = form.full_source_url().spec();
  if (encoder.AnonymousUrlCollectionIsEnabled() && !full_source_url.empty()) {
    EncodeRandomizedValue(encoder, form_signature, kNullFieldSignature,
                          RandomizedEncoder::kFormUrl, full_source_url,
                          /*include_checksum=*/true, metadata->mutable_url());
  }
}

void PopulateRandomizedFieldMetadata(
    const RandomizedEncoder& encoder,
    const FormStructure& form,
    const AutofillField& field,
    AutofillRandomizedFieldMetadata* metadata) {
  // Shorthand for encoding values.
  auto encode_value = [&, form_signature = form.form_signature(),
                       field_signature = field.GetFieldSignature()](
                          std::string_view data_type, auto data_value,
                          AutofillRandomizedValue* output) {
    EncodeRandomizedValue(encoder, form_signature, field_signature, data_type,
                          data_value,
                          /*include_checksum=*/false, output);
  };

  if (!field.id_attribute().empty()) {
    encode_value(RandomizedEncoder::kFieldId, field.id_attribute(),
                 metadata->mutable_id());
  }
  if (!field.name_attribute().empty()) {
    encode_value(RandomizedEncoder::kFieldName, field.name_attribute(),
                 metadata->mutable_name());
  }
  encode_value(RandomizedEncoder::kFieldControlType,
               FormControlTypeToString(field.form_control_type()),
               metadata->mutable_type());
  if (!field.label().empty()) {
    encode_value(RandomizedEncoder::kFieldLabel, field.label(),
                 metadata->mutable_label());
  }
  if (!field.aria_label().empty()) {
    encode_value(RandomizedEncoder::kFieldAriaLabel, field.aria_label(),
                 metadata->mutable_aria_label());
  }
  if (!field.aria_description().empty()) {
    encode_value(RandomizedEncoder::kFieldAriaDescription,
                 field.aria_description(),
                 metadata->mutable_aria_description());
  }
  if (!field.css_classes().empty()) {
    encode_value(RandomizedEncoder::kFieldCssClasses, field.css_classes(),
                 metadata->mutable_css_class());
  }
  if (!field.placeholder().empty()) {
    encode_value(RandomizedEncoder::kFieldPlaceholder, field.placeholder(),
                 metadata->mutable_placeholder());
  }
  if (!field.autocomplete_attribute().empty()) {
    encode_value(RandomizedEncoder::kFieldAutocomplete,
                 field.autocomplete_attribute(),
                 metadata->mutable_autocomplete());
  }
  if (!field.pattern().empty()) {
    encode_value(RandomizedEncoder::kFieldPattern, field.pattern(),

                 metadata->mutable_pattern());
  }
  // 0 is the default value for fields that do not allow free input, while
  // `kDefaultMaxLength` is the default value for fields that allow free input.
  if (field.max_length() != 0 &&
      field.max_length() != FormFieldData::kDefaultMaxLength) {
    encode_value(RandomizedEncoder::kFieldMaxLength,
                 base::NumberToString(field.max_length()),
                 metadata->mutable_max_length());
  }
  if (field.IsSelectElement()) {
    auto add_option = [&](const SelectOption& option) {
      auto* proto_option = metadata->add_select_option();
      if (!option.text.empty()) {
        encode_value(RandomizedEncoder::kFieldSelectOptionText, option.text,
                     proto_option->mutable_text());
      }
      // Only emit `value` if it differs from `text` because both `value` and
      // `text` have the same value if the <option> has neither an explicit
      // value nor an explicit label attribute.
      if (!option.value.empty() && option.value != option.text) {
        encode_value(RandomizedEncoder::kFieldSelectOptionValue, option.value,
                     proto_option->mutable_value());
      }
    };
    if (field.options().size() > 0) {
      add_option(field.options()[0]);
    }
    if (field.options().size() > 1) {
      add_option(field.options()[1]);
    }
    if (field.options().size() > 2) {
      add_option(field.options().back());
    }
  }
}

// Populates the three-bit hashes for a given `form`.
void PopulateThreeBitHashedFormMetadata(
    const FormStructure& form,
    ThreeBitHashedFormMetadata* form_metadata) {
  if (!form.id_attribute().empty()) {
    form_metadata->set_id(StrToHash3Bit(form.id_attribute()));
  }
  if (!form.name_attribute().empty()) {
    form_metadata->set_name(StrToHash3Bit(form.name_attribute()));
  }

  if (!form.button_titles().empty()) {
    std::string concatenated_button_titles = base::StrCat(
        base::ToVector(form.button_titles(), [](const ButtonTitleInfo& info) {
          return base::UTF16ToUTF8(info.first);
        }));
    form_metadata->set_button_titles_concatenated(
        StrToHash3Bit(concatenated_button_titles));
  }
}

// Populates the three-bit hashes for a single field.
void PopulateThreeBitHashedFieldMetadata(
    const AutofillField& field,
    ThreeBitHashedFieldMetadata* field_metadata) {
  if (!field.id_attribute().empty()) {
    field_metadata->set_id(StrToHash3Bit(field.id_attribute()));
  }
  if (!field.name_attribute().empty()) {
    field_metadata->set_name(StrToHash3Bit(field.name_attribute()));
  }
  field_metadata->set_type(
      StrToHash3Bit(FormControlTypeToString(field.form_control_type())));
  if (!field.label().empty()) {
    field_metadata->set_label(StrToHash3Bit(field.label()));
  }
  if (!field.aria_label().empty()) {
    field_metadata->set_aria_label(StrToHash3Bit(field.aria_label()));
  }
  if (!field.aria_description().empty()) {
    field_metadata->set_aria_description(
        StrToHash3Bit(field.aria_description()));
  }
  if (!field.placeholder().empty()) {
    field_metadata->set_placeholder(StrToHash3Bit(field.placeholder()));
  }
  if (!field.initial_value().empty()) {
    field_metadata->set_initial_value(StrToHash3Bit(field.initial_value()));
  }
  if (!field.autocomplete_attribute().empty()) {
    field_metadata->set_autocomplete(
        StrToHash3Bit(field.autocomplete_attribute()));
  }
  if (!field.pattern().empty()) {
    field_metadata->set_pattern(StrToHash3Bit(field.pattern()));
  }
}

// Encodes the fields of `upload_fields` in the in-out parameter `upload`.
// Helper function for EncodeUploadRequest().
void EncodeFormFieldsForUpload(
    const FormStructure& form,
    base::optional_ref<const RandomizedEncoder> encoder,
    const std::map<FieldGlobalId, EncodeUploadRequestOptions::Field>& fields,
    base::span<const AutofillField* const> upload_fields,
    AutofillUploadContents* upload) {
  DCHECK(!IsMalformed(form));

  for (const AutofillField* const field : upload_fields) {
    // Don't upload checkable fields.
    if (IsCheckable(field->check_status())) {
      continue;
    }
    // Do not upload fields that were filled with a fallback type, as this would
    // introduce unnecessary noise in the field votes.
    if (field->WasAutofilledWithFallback() &&
        !base::FeatureList::IsEnabled(
            features::kAutofillUploadManualFallbackFieldsToServer)) {
      continue;
    }

    const EncodeUploadRequestOptions::Field* field_options = nullptr;
    if (auto it = fields.find(field->global_id()); it != fields.end()) {
      field_options = &it->second;
    }

    auto* added_field = upload->add_field_data();
    for (auto field_type : field->possible_types()) {
      added_field->add_autofill_type(field_type);
    }

    if (field_options && field_options->vote_type) {
      added_field->set_vote_type(field_options->vote_type);
    }

    if (field_options && field_options->initial_value_hash) {
      added_field->set_initial_value_hash(
          field_options->initial_value_hash.value());
    }

    // TODO(crbug.com/40286837): Understand and document why the type is
    // relevant.
    if (!field->initial_value().empty() &&
        (!field->Type().GetTypes().contains_any(
             {NO_SERVER_DATA, UNKNOWN_TYPE}) ||
         !field->possible_types().empty())) {
      added_field->set_initial_value_changed(field->initial_value() !=
                                             field->value());
    }

    if (field_options) {
      for (const auto& [type, string] : field_options->format_strings) {
        DCHECK(AutofillFormatString::IsValid(string, type));
        auto* added_format_string = added_field->add_format_string();
        added_format_string->set_type(type);
        added_format_string->set_format_string(base::UTF16ToUTF8(string));
      }
    }

    added_field->set_signature(field->GetFieldSignature().value());

    if (field->properties_mask()) {
      added_field->set_properties_mask(field->properties_mask());
    }

    if (encoder.has_value()) {
      PopulateRandomizedFieldMetadata(
          *encoder, form, *field,
          added_field->mutable_randomized_field_metadata());
    }

    if (base::FeatureList::IsEnabled(features::kAutofillServerUploadMoreData)) {
      PopulateThreeBitHashedFieldMetadata(
          *field, added_field->mutable_three_bit_hashed_field_metadata());
    }

    if (field_options) {
      if (field_options->generation_type) {
        added_field->set_generation_type(field_options->generation_type);
        added_field->set_generated_password_changed(
            field_options->generated_password_changed);
      }

      if (field_options->single_username_vote_type) {
        added_field->set_single_username_vote_type(
            field_options->single_username_vote_type.value());
      }

      switch (field_options->is_most_recent_single_username_candidate) {
        using enum IsMostRecentSingleUsernameCandidate;
        case kNotPartOfUsernameFirstFlow:
          added_field->clear_is_most_recent_single_username_candidate();
          break;
        case kHasIntermediateValuesInBetween:
          added_field->set_is_most_recent_single_username_candidate(false);
          break;
        case kMostRecentCandidate:
          added_field->set_is_most_recent_single_username_candidate(true);
          break;
      }
    }
  }
}

void EncodeFormForQuery(const FormStructure& form,
                        AutofillPageQueryRequest& query,
                        std::vector<FormSignature>& queried_form_signatures,
                        std::set<FormSignature>& processed_forms) {
  DCHECK(!IsMalformed(form));
  // Adds a request to `query` that contains all (`form`, `field`) for every
  // `field` from `fields_` that meets `necessary_condition`. Repeated calls for
  // the same `form` have no effect (early return if `processed_forms` contains
  // `form`).
  auto AddFormIf =
      [&](const std::vector<std::unique_ptr<AutofillField>>& fields,
          FormSignature form_signature, FormSignature alternative_signature,
          auto necessary_condition) mutable {
        if (!processed_forms.insert(form_signature).second) {
          return;
        }

        AutofillPageQueryRequest::Form* query_form = query.add_forms();
        query_form->set_signature(form_signature.value());
        query_form->set_alternative_signature(alternative_signature.value());

        if (base::FeatureList::IsEnabled(
                features::kAutofillServerExperimentalSignatures)) {
          query_form->set_structural_signature(
              form.structural_form_signature().value());
          PopulateThreeBitHashedFormMetadata(
              form, query_form->mutable_three_bit_hashed_form_metadata());
        }

        queried_form_signatures.push_back(form_signature);

        for (const auto& field : fields) {
          if (IsCheckable(field->check_status()) ||
              !necessary_condition(field)) {
            continue;
          }

          AutofillPageQueryRequest::Form::Field* added_field =
              query_form->add_fields();
          added_field->set_signature(field->GetFieldSignature().value());

          if (base::FeatureList::IsEnabled(
                  features::kAutofillServerExperimentalSignatures)) {
            PopulateThreeBitHashedFieldMetadata(
                *field, added_field->mutable_three_bit_hashed_field_metadata());
          }
        }
      };

  AddFormIf(form.fields(), form.form_signature(),
            form.alternative_form_signature(), [](auto& f) { return true; });

  for (const auto& field : form.fields()) {
    if (field->host_form_signature()) {
      AddFormIf(form.fields(), field->host_form_signature(),
                form.alternative_form_signature(), [&](const auto& f) {
                  return f->host_form_signature() ==
                         field->host_form_signature();
                });
    }
  }
}

// Checks if `field_suggestion` contains any password related type prediction.
bool HasPasswordManagerPrediction(const FieldSuggestion& field_suggestion) {
  return std::ranges::any_of(
      field_suggestion.predictions(), [](const auto& prediction) {
        auto group_type = GroupTypeOfFieldType(
            ToSafeFieldType(prediction.type(), NO_SERVER_DATA));
        return group_type == FieldTypeGroup::kPasswordField ||
               group_type == FieldTypeGroup::kUsernameField;
      });
}

// Adds password predictions from `merge_from_predictions` to
// `merge_to_predictions`.
void MergePasswordManagerPredictions(
    const FieldSuggestion& merge_from_predictions,
    FieldSuggestion& merge_to_predictions) {
  CHECK_NE(&merge_to_predictions, &merge_from_predictions);
  for (const auto& prediction : merge_from_predictions.predictions()) {
    FieldTypeGroup group_type = GroupTypeOfFieldType(
        ToSafeFieldType(prediction.type(), NO_SERVER_DATA));
    // Only add predictions relevant for PasswordManager.
    if (group_type == FieldTypeGroup::kPasswordField ||
        group_type == FieldTypeGroup::kUsernameField) {
      auto* new_prediction = merge_to_predictions.add_predictions();
      new_prediction->CopyFrom(prediction);
    }
  }
}

// Given `form` and `field`, returns the appropriate FieldSuggestion stored
// for that field in `fields_suggestions`.
std::optional<FieldSuggestion> GetFieldSuggestion(
    const FormStructure& form,
    const AutofillField& field,
    std::map<std::pair<FormSignature, FieldSignature>,
             std::deque<FieldSuggestion>>& fields_suggestions) {
  // Retrieves the next prediction for `form` and `field` and pops it. Popping
  // is omitted if no other predictions for `form` and `field` are left, so that
  // any subsequent fields with the same signature will get the same prediction.
  std::set<FormSignature> signatures_seen;
  auto get_suggestion =
      [&fields_suggestions, &signatures_seen](
          FormSignature form_signature,
          FieldSignature field_signature) -> std::optional<FieldSuggestion> {
    auto it = fields_suggestions.find({form_signature, field_signature});
    if (it == fields_suggestions.end() ||
        !signatures_seen.insert(form_signature).second) {
      return std::nullopt;
    }
    CHECK(!it->second.empty());
    FieldSuggestion current_field = it->second.front();
    if (it->second.size() > 1) {
      it->second.pop_front();
    }
    return std::move(current_field);
  };
  // Precedence rule for prediction sources is the following:
  // Manual overrides first, then server overrides, then crowdsourcing of any
  // type. Moreover, Autofill deprioritizes any crowdsourcing that only returned
  // NO_SERVER_DATA. This is not done for overrides because overriding a field
  // as not classifiable could be desirable.
  auto get_suggestion_priority =
      [](base::optional_ref<const FieldSuggestion> suggestion) {
        if (!suggestion.has_value() || suggestion->predictions().empty()) {
          return 0;  // Lowest priority
        }
        switch (ToSafeFieldPredictionSource(
            suggestion->predictions().begin()->source())) {
          case FieldPrediction::SOURCE_AUTOFILL_AI:
          case FieldPrediction::SOURCE_AUTOFILL_AI_CROWDSOURCING:
            return base::FeatureList::IsEnabled(
                       features::kAutofillAiWithDataSchema)
                       ? 2
                       : 0;
          case FieldPrediction::SOURCE_UNSPECIFIED:
          case FieldPrediction::SOURCE_AUTOFILL_DEFAULT:
          case FieldPrediction::SOURCE_PASSWORDS_DEFAULT:
          case FieldPrediction::SOURCE_ALL_APPROVED_EXPERIMENTS:
          case FieldPrediction::SOURCE_FIELD_RANKS:
          case FieldPrediction::SOURCE_AUTOFILL_COMBINED_TYPES:
            return std::ranges::all_of(suggestion->predictions(),
                                       [](const auto& prediction) {
                                         return prediction.type() ==
                                                NO_SERVER_DATA;
                                       })
                       ? 1  // Only better than empty predictions.
                       : 2;
          case FieldPrediction::SOURCE_OVERRIDE:
            return 3;
          case FieldPrediction::SOURCE_MANUAL_OVERRIDE:
            return 4;
        }
        NOTREACHED();
      };
  // Fetch suggestions from form signature, host form signature and alternative
  // form signature.
  std::optional<FieldSuggestion> main_frame_field_suggestion =
      get_suggestion(form.form_signature(), field.GetFieldSignature());
  std::optional<FieldSuggestion> iframe_field_suggestion =
      get_suggestion(field.host_form_signature(), field.GetFieldSignature());
  // NOTE: Suggestions from alternative form signatures are always overrides.
  std::optional<FieldSuggestion> alternative_field_suggestion = get_suggestion(
      form.alternative_form_signature(), field.GetFieldSignature());

  // Precedence rule for form signatures is the following:
  // `form_signature` (main frame) then `host_form_signature_` (iframe) and then
  // alternative_form_signature_.
  // This order is given by the specificity of the form signature. A
  // form_signature is very specific. An iframe can be embedded on multiple
  // sites. An alternative form signature is a fallback and might even match
  // multiple forms on the same site.
  // This precedence rule is less important than the source precedence rule,
  // which means that it is only applicable for suggestions with equal source
  // priority.
  std::vector<base::optional_ref<FieldSuggestion>> suggestions = {
      main_frame_field_suggestion, iframe_field_suggestion,
      alternative_field_suggestion};
  base::optional_ref<FieldSuggestion> preferred_field_suggestion =
      *std::ranges::max_element(suggestions, {}, get_suggestion_priority);
  if (!preferred_field_suggestion) {
    return std::nullopt;
  }

  // Add predictions for PasswordManager from `iframe_field_suggestions` if
  // `field_suggestion` is missing them. This is only relevant for
  // crowdsourcing which is why we do not apply the same logic for
  // `alternative_form_signature` suggestions, which are always overrides.
  if (iframe_field_suggestion &&
      !HasPasswordManagerPrediction(*preferred_field_suggestion) &&
      HasPasswordManagerPrediction(*iframe_field_suggestion)) {
    MergePasswordManagerPredictions(*iframe_field_suggestion,
                                    *preferred_field_suggestion);
  }
  return std::move(*preferred_field_suggestion);
}

// Builds a map from a pair of (form_signature, field_signature) to all the
// server FieldSuggestion's retrieved from `response`. Also includes the
// manual overrides provided from the feature `AutofillOverridePredictions`.
std::map<std::pair<FormSignature, FieldSignature>, std::deque<FieldSuggestion>>
GetSuggestionsMapFromResponse(
    const AutofillQueryResponse& response,
    const std::vector<FormSignature>& queried_form_signatures) {
  std::map<std::pair<FormSignature, FieldSignature>,
           std::deque<FieldSuggestion>>
      fields_suggestions;
  const int num_of_forms =
      std::min(response.form_suggestions_size(),
               base::checked_cast<int>(queried_form_signatures.size()));

  for (int form_idx = 0; form_idx < num_of_forms; ++form_idx) {
    FormSignature form_signature = queried_form_signatures[form_idx];
    for (const auto& field :
         response.form_suggestions(form_idx).field_suggestions()) {
      FieldSignature field_signature(field.field_signature());
      fields_suggestions[{form_signature, field_signature}].push_back(field);
    }
  }
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(
          features::debug::kAutofillOverridePredictions)) {
    if (std::string param =
            features::debug::kAutofillOverridePredictionsSpecification.Get();
        !param.empty()) {
      InsertParsedOverrides(
          ParseServerPredictionOverrides(param, OverrideFormat::kSpec),
          fields_suggestions);
    }
    if (std::string param =
            features::debug::kAutofillOverridePredictionsJson.Get();
        !param.empty()) {
      InsertParsedOverrides(
          ParseServerPredictionOverrides(param, OverrideFormat::kJson),
          fields_suggestions);
    }
  }
#endif
  return fields_suggestions;
}

base::flat_set<FormSignature> GetFormsForWhichToRunAiModel(
    const AutofillQueryResponse& response,
    const std::vector<FormSignature>& queried_form_signatures) {
  std::vector<FormSignature> forms;
  const int num_of_forms =
      std::min(response.form_suggestions_size(),
               base::checked_cast<int>(queried_form_signatures.size()));
  for (int i = 0; i < num_of_forms; ++i) {
    if (response.form_suggestions(i).run_autofill_ai_model()) {
      forms.push_back(queried_form_signatures[i]);
    }
  }
  return base::flat_set<FormSignature>(std::move(forms));
}

// Checks the list of server predictions and potentially merges predictions for
// joined types e.g. if the server returned separate predictions for email and
// loyalty card, those are merged into the EMAIL_OR_LOYALTY_MEMBERSHIP_ID type.
void MaybeMergeServerPredictions(
    std::vector<FieldPrediction>& server_predictions) {
  const auto server_types =
      FieldTypeSet(server_predictions, [](const FieldPrediction& pred) {
        return ToSafeFieldType(pred.type(), UNKNOWN_TYPE);
      });

  if (server_types.contains_all({EMAIL_ADDRESS, LOYALTY_MEMBERSHIP_ID}) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableEmailOrLoyaltyCardsFilling)) {
    // Remove email and loyalty card predictions.
    std::erase_if(server_predictions, [](const FieldPrediction& x) {
      return x.type() == EMAIL_ADDRESS || x.type() == LOYALTY_MEMBERSHIP_ID;
    });
    FieldPrediction email_and_loyalty_card_prediction;
    email_and_loyalty_card_prediction.set_type(EMAIL_OR_LOYALTY_MEMBERSHIP_ID);
    email_and_loyalty_card_prediction.set_source(
        FieldPrediction::SOURCE_AUTOFILL_DEFAULT);
    email_and_loyalty_card_prediction.set_override(false);
    server_predictions.insert(server_predictions.begin(),
                              email_and_loyalty_card_prediction);
  }
}

}  // namespace

EncodeUploadRequestOptions::Field::Field() = default;
EncodeUploadRequestOptions::Field::Field(Field&&) = default;
EncodeUploadRequestOptions::Field& EncodeUploadRequestOptions::Field::operator=(
    Field&&) = default;
EncodeUploadRequestOptions::Field::~Field() = default;

EncodeUploadRequestOptions::EncodeUploadRequestOptions() = default;
EncodeUploadRequestOptions::EncodeUploadRequestOptions(
    EncodeUploadRequestOptions&&) = default;
EncodeUploadRequestOptions&
EncodeUploadRequestOptions::EncodeUploadRequestOptions::operator=(
    EncodeUploadRequestOptions&&) = default;
EncodeUploadRequestOptions::~EncodeUploadRequestOptions() = default;

std::vector<AutofillUploadContents> EncodeUploadRequest(
    const FormStructure& form,
    const EncodeUploadRequestOptions& options) {
  DCHECK_EQ(FirstNonCapturedType(form, options.available_field_types),
            MAX_VALID_FIELD_TYPE);

  std::string data_present = EncodeFieldTypes(options.available_field_types);

  AutofillUploadContents upload;
  upload.set_submission(options.observed_submission);
  upload.set_client_version(
      std::string(version_info::GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form.form_signature().value());
  if (base::FeatureList::IsEnabled(
          features::kAutofillUseStructuralSignatureInsteadOfSecondary)) {
    upload.set_structural_form_signature(
        form.structural_form_signature().value());
  } else {
    upload.set_secondary_form_signature(
        form.alternative_form_signature().value());
  }
  upload.set_autofill_used(false);
  upload.set_data_present(data_present);
  upload.set_has_form_tag(form.is_form_element());
  if (!options.current_page_language->empty() && options.encoder) {
    upload.set_language(options.current_page_language.value());
  }

  if (options.form_associations.last_address_form_submitted) {
    upload.set_last_address_form_submitted(
        options.form_associations.last_address_form_submitted->value());
  }
  if (options.form_associations.second_last_address_form_submitted) {
    upload.set_second_last_address_form_submitted(
        options.form_associations.second_last_address_form_submitted->value());
  }
  if (options.form_associations.last_credit_card_form_submitted) {
    upload.set_last_credit_card_form_submitted(
        options.form_associations.last_credit_card_form_submitted->value());
  }

  auto triggering_event =
      (options.submission_event != mojom::SubmissionIndicatorEvent::NONE)
          ? options.submission_event
          : ToSubmissionIndicatorEvent(form.submission_source());

  DCHECK(mojom::IsKnownEnumValue(triggering_event));
  upload.set_submission_event(
      static_cast<AutofillUploadContents_SubmissionIndicatorEvent>(
          triggering_event));

  if (options.login_form_signature.has_value()) {
    upload.set_login_form_signature(options.login_form_signature->value());
  }

  if (IsMalformed(form)) {
    return {};  // Malformed form, skip it.
  }

  if (options.encoder) {
    PopulateRandomizedFormMetadata(*options.encoder, form,
                                   upload.mutable_randomized_form_metadata());
  }

  if (base::FeatureList::IsEnabled(features::kAutofillServerUploadMoreData)) {
    PopulateThreeBitHashedFormMetadata(
        form, upload.mutable_three_bit_hashed_form_metadata());
  }

  std::vector<AutofillField*> upload_fields(form.fields().size());
  std::ranges::transform(form.fields(), upload_fields.begin(),
                         &std::unique_ptr<AutofillField>::get);
  EncodeFormFieldsForUpload(form, options.encoder, options.fields,
                            upload_fields, &upload);
  std::vector<AutofillUploadContents> uploads = {std::move(upload)};

  // Build AutofillUploadContents for the renderer forms that have been
  // flattened into `this` (see the function's documentation for details).
  std::erase_if(upload_fields, [&form](const AutofillField* field) {
    // Autofill on iOS and the Password Manager in general have a null
    // FormFieldData::host_form_signature.
    return !field->host_form_signature() ||
           field->host_form_signature() == form.form_signature();
  });
  // Partition `upload_fields` with respect to the forms' renderer id.
  std::ranges::stable_sort(upload_fields, /*comp=*/{},
                           &FormFieldData::renderer_form_id);

  for (auto subform_begin = upload_fields.begin();
       subform_begin != upload_fields.end();) {
    AutofillUploadContents& upload_content = uploads.emplace_back();
    upload_content.set_client_version(
        std::string(version_info::GetProductNameAndVersionForUserAgent()));
    upload_content.set_form_signature(
        (*subform_begin)->host_form_signature().value());
    upload_content.set_autofill_used(false);
    upload_content.set_data_present(data_present);

    auto subform_end =
        std::find_if(subform_begin, upload_fields.end(),
                     [&subform_begin](const AutofillField* field) {
                       return field->renderer_form_id() !=
                              (*subform_begin)->renderer_form_id();
                     });
    // SAFETY: The iterators are from the same container.
    EncodeFormFieldsForUpload(form, options.encoder, options.fields,
                              UNSAFE_BUFFERS({subform_begin, subform_end}),
                              &uploads.back());
    subform_begin = subform_end;
  }
  return uploads;
}

std::pair<AutofillPageQueryRequest, std::vector<FormSignature>>
EncodeAutofillPageQueryRequest(
    const std::vector<raw_ptr<const FormStructure, VectorExperimental>>&
        forms) {
  AutofillPageQueryRequest query;
  std::vector<FormSignature> queried_form_signatures;
  queried_form_signatures.reserve(forms.size());
  query.set_client_version(
      std::string(version_info::GetProductNameAndVersionForUserAgent()));

  // If a page contains repeated forms, detect that and encode only one form as
  // the returned data would be the same for all the repeated forms.
  // TODO(crbug/1064709#c11): the statement is not entirely correct because
  // (1) distinct forms can have identical form signatures because we truncate
  // (large) numbers in the form signature calculation while these are
  // considered for field signatures; (2) for dynamic forms we will hold on to
  // the original form signature.
  std::set<FormSignature> processed_forms;
  for (const FormStructure* form : forms) {
    if (base::Contains(processed_forms, form->form_signature())) {
      continue;
    }
    UMA_HISTOGRAM_COUNTS_1000("Autofill.FieldCount", form->field_count());
    if (IsMalformed(*form)) {
      continue;
    }

    EncodeFormForQuery(*form, query, queried_form_signatures, processed_forms);
  }

  return std::make_pair(std::move(query), std::move(queried_form_signatures));
}

void ParseServerPredictionsQueryResponse(
    std::string_view payload,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    LogManager* log_manager) {
  AutofillMetrics::LogServerQueryMetric(
      AutofillMetrics::QUERY_RESPONSE_RECEIVED);

  std::string decoded_payload;
  if (!base::Base64Decode(payload, &decoded_payload)) {
    DVLOG(1) << "Could not decode payload from base64 to bytes";
    return;
  }

  // Parse the response.
  AutofillQueryResponse response;
  if (!response.ParseFromString(decoded_payload)) {
    return;
  }

  DVLOG(1) << "Autofill query response from API was successfully parsed: "
           << response;

  ProcessServerPredictionsQueryResponse(response, forms,
                                        queried_form_signatures, log_manager);
}

void ProcessServerPredictionsQueryResponse(
    const AutofillQueryResponse& response,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    LogManager* log_manager) {
  AutofillMetrics::LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED);
  LOG_AF(log_manager) << LoggingScope::kParsing
                      << LogMessage::kProcessingServerData;

  bool heuristics_detected_fillable_field = false;
  bool query_response_overrode_heuristics = false;
  std::map<std::pair<FormSignature, FieldSignature>,
           std::deque<FieldSuggestion>>
      fields_suggestions =
          GetSuggestionsMapFromResponse(response, queried_form_signatures);

  const base::flat_set<FormSignature> forms_for_which_to_run_ai_model =
      GetFormsForWhichToRunAiModel(response, queried_form_signatures);

  // Copy the field types into the actual form.
  for (FormStructure* form : forms) {
    form->set_may_run_autofill_ai_model(
        forms_for_which_to_run_ai_model.contains(form->form_signature()));

    // Fields can share the same field signature. This map records for each
    // signature how many fields with the same signature have been observed.
    std::map<FieldSignature, size_t> field_rank_map;
    for (auto& field : form->fields()) {
      std::optional<FieldSuggestion> field_suggestion =
          GetFieldSuggestion(*form, *field, fields_suggestions);
      if (!field_suggestion) {
        continue;
      }
      FieldType heuristic_type = field->heuristic_type();
      if (heuristic_type != UNKNOWN_TYPE) {
        heuristics_detected_fillable_field = true;
      }
      std::vector<FieldPrediction> server_predictions = {
          field_suggestion->predictions().begin(),
          field_suggestion->predictions().end()};
      MaybeMergeServerPredictions(server_predictions);
      field->set_server_predictions(std::move(server_predictions));
      if (!field->Type().GetTypes().contains(heuristic_type)) {
        query_response_overrode_heuristics = true;
      }
      if (field_suggestion->has_password_requirements()) {
        field->SetPasswordRequirements(
            field_suggestion->password_requirements());
      }
      if (field_suggestion->has_format_string()) {
        std::u16string format_string_value = base::UTF8ToUTF16(
            field_suggestion->format_string().format_string());
        if (AutofillFormatString::IsValid(
                format_string_value,
                field_suggestion->format_string().type())) {
          field->set_format_string_unless_overruled(
              AutofillFormatString(format_string_value,
                                   field_suggestion->format_string().type()),
              AutofillFormatStringSource::kServer);
        }
      }
      ++field_rank_map[field->GetFieldSignature()];

      // Log the field type predicted from Autofill crowdsourced server.
      field->AppendLogEventIfNotRepeated(ServerPredictionFieldLogEvent{
          // If the server prediction is empty, the server type should be
          // SERVER_RESPONSE_PENDING (161), which means that Autofill may not
          // have received server predictions. NO_SERVER_DATA means that the
          // server has no classification for the field.
          .server_type1 = !field->server_predictions().empty()
                              ? std::optional<FieldType>(field->server_type())
                              : std::nullopt,
          .prediction_source1 = !field->server_predictions().empty()
                                    ? field->server_predictions()[0].source()
                                    : FieldPrediction::SOURCE_UNSPECIFIED,
          .server_type2 =
              field->server_predictions().size() >= 2
                  ? std::optional<FieldType>(ToSafeFieldType(
                        field->server_predictions()[1].type(), NO_SERVER_DATA))
                  : std::nullopt,
          .prediction_source2 = field->server_predictions().size() >= 2
                                    ? field->server_predictions()[1].source()
                                    : FieldPrediction::SOURCE_UNSPECIFIED,
          .server_type_prediction_is_override =
              field->server_type_prediction_is_override(),
          .rank_in_field_signature_group =
              field_rank_map[field->GetFieldSignature()],
      });
    }

    AutofillMetrics::LogServerResponseHasDataForForm(std::ranges::any_of(
        form->fields(), [](FieldType t) { return t != NO_SERVER_DATA; },
        &AutofillField::server_type));
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

}  // namespace autofill
