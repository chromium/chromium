// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"

#include <algorithm>
#include <deque>
#include <string>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure_rationalizer.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/browser/server_prediction_overrides.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/logging/log_buffer.h"
#include "components/version_info/version_info.h"

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
    // If the manual override has a no type specified, it means that the
    // server prediction should be used.
    result.push_back(manual_overrides.front().predictions().empty()
                         ? server_overrides.front()
                         : manual_overrides.front());

    manual_overrides.pop_front();
    // Generally consume the first element of each override source. However,
    // the last server override can apply to multiple fields with the same
    // signature, so we do not pop it while it is still useful.
    if (server_overrides.size() > 1 || manual_overrides.empty()) {
      server_overrides.pop_front();
    }
  }
  // At most one override source is non-empty - preserve the values.
  base::ranges::move(manual_overrides, std::back_inserter(result));
  base::ranges::move(server_overrides, std::back_inserter(result));

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
  const size_t kNumBytes = (MAX_VALID_FIELD_TYPE + 0x7) / 8;

  // Pack the types in `available_field_types` into `bit_field`.
  std::vector<uint8_t> bit_field(kNumBytes, 0);
  for (const auto field_type : available_field_types) {
    // Set the appropriate bit in the field.  The bit we set is the one
    // `field_type` % 8 from the left of the byte.
    const size_t byte = field_type / 8;
    const size_t bit = 0x80 >> (field_type % 8);
    DCHECK(byte < bit_field.size());
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
  data_presence.reserve(data_end * 2 + 1);
  for (size_t i = 0; i < data_end; ++i) {
    base::StringAppendF(&data_presence, "%02x", bit_field[i]);
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
    DCHECK(data_type == RandomizedEncoder::FORM_URL);
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
  if (!field.id_attribute().empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_ID, field.id_attribute(),
                          /*include_checksum=*/false, metadata->mutable_id());
  }
  if (!field.name_attribute().empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_NAME, field.name_attribute(),
                          /*include_checksum=*/false, metadata->mutable_name());
  }
  EncodeRandomizedValue(encoder, form_signature, field_signature,
                        RandomizedEncoder::FIELD_CONTROL_TYPE,
                        FormControlTypeToString(field.form_control_type()),
                        /*include_checksum=*/false, metadata->mutable_type());
  if (!field.label().empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_LABEL, field.label(),
                          /*include_checksum=*/false,
                          metadata->mutable_label());
  }
  if (!field.aria_label().empty()) {
    EncodeRandomizedValue(
        encoder, form_signature, field_signature,
        RandomizedEncoder::FIELD_ARIA_LABEL, field.aria_label(),
        /*include_checksum=*/false, metadata->mutable_aria_label());
  }
  if (!field.aria_description().empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_ARIA_DESCRIPTION,
                          field.aria_description(), /*include_checksum=*/false,
                          metadata->mutable_aria_description());
  }
  if (!field.css_classes().empty()) {
    EncodeRandomizedValue(
        encoder, form_signature, field_signature,
        RandomizedEncoder::FIELD_CSS_CLASS, field.css_classes(),
        /*include_checksum=*/false, metadata->mutable_css_class());
  }
  if (!field.placeholder().empty()) {
    EncodeRandomizedValue(encoder, form_signature, field_signature,
                          RandomizedEncoder::FIELD_PLACEHOLDER,
                          field.placeholder(), /*include_checksum=*/false,
                          metadata->mutable_placeholder());
  }
  if (!field.autocomplete_attribute().empty()) {
    EncodeRandomizedValue(
        encoder, form_signature, field_signature,
        RandomizedEncoder::FIELD_AUTOCOMPLETE, field.autocomplete_attribute(),
        /*include_checksum=*/false, metadata->mutable_autocomplete());
  }
}

// Encodes the fields of `upload_fields` in the in-out parameter `upload`.
// Helper function for EncodeUploadRequest().
void EncodeFormFieldsForUpload(const FormStructure& form,
                               base::span<AutofillField*> upload_fields,
                               AutofillUploadContents* upload) {
  DCHECK(!IsMalformed(form));

  for (AutofillField* field : upload_fields) {
    // Don't upload checkable fields.
    if (IsCheckable(field->check_status())) {
      continue;
    }
    // Do not upload fields that were filled with a fallback type, as this would
    // introduce unnecessary noise in the field votes.
    if (field->WasAutofilledWithFallback()) {
      continue;
    }

    auto* added_field = upload->add_field_data();
    for (auto field_type : field->possible_types()) {
      added_field->add_autofill_type(field_type);
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

    if (field->properties_mask()) {
      added_field->set_properties_mask(field->properties_mask());
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

void EncodeFormForQuery(const autofill::FormStructure& form,
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
          FormSignature form, FormSignature alternative_signature,
          auto necessary_condition) mutable {
        if (!processed_forms.insert(form).second) {
          return;
        }

        AutofillPageQueryRequest::Form* query_form = query.add_forms();
        query_form->set_signature(form.value());
        query_form->set_alternative_signature(alternative_signature.value());
        queried_form_signatures.push_back(form);

        for (const auto& field : fields) {
          if (IsCheckable(field->check_status()) ||
              !necessary_condition(field)) {
            continue;
          }

          AutofillPageQueryRequest::Form::Field* added_field =
              query_form->add_fields();
          added_field->set_signature(field->GetFieldSignature().value());
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
        switch (suggestion->predictions().begin()->source()) {
          case FieldPrediction::SOURCE_UNSPECIFIED:
          case FieldPrediction::SOURCE_AUTOFILL_DEFAULT:
          case FieldPrediction::SOURCE_PASSWORDS_DEFAULT:
          case FieldPrediction::SOURCE_ALL_APPROVED_EXPERIMENTS:
          case FieldPrediction::SOURCE_FIELD_RANKS:
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
  base::optional_ref<FieldSuggestion> preferred_field_suggestion =
      base::ranges::max(
          std::vector<base::optional_ref<FieldSuggestion>>{
              main_frame_field_suggestion, iframe_field_suggestion,
              alternative_field_suggestion},
          {}, get_suggestion_priority);

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
  return preferred_field_suggestion.has_value()
             ? std::optional(std::move(*preferred_field_suggestion))
             : std::nullopt;
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
          features::test::kAutofillOverridePredictions)) {
    auto maybe_insert_overrides =
        [&fields_suggestions](const base::FeatureParam<std::string>& param) {
          if (std::string param_value = param.Get(); !param_value.empty()) {
            InsertParsedOverrides(ParseServerPredictionOverrides(param_value),
                                  fields_suggestions);
          }
        };
    maybe_insert_overrides(
        features::test::kAutofillOverridePredictionsSpecification);
    maybe_insert_overrides(
        features::test::
            kAutofillOverridePredictionsForAlternativeFormSignaturesSpecification);
  }
#endif
  return fields_suggestions;
}

}  // namespace

std::vector<AutofillUploadContents> EncodeUploadRequest(
    const FormStructure& form,
    const FieldTypeSet& available_field_types,
    std::string_view login_form_signature,
    bool observed_submission) {
  DCHECK_EQ(FirstNonCapturedType(form, available_field_types),
            MAX_VALID_FIELD_TYPE);

  std::string data_present = EncodeFieldTypes(available_field_types);

  AutofillUploadContents upload;
  upload.set_submission(observed_submission);
  upload.set_client_version(
      std::string(version_info::GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form.form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present(data_present);
  upload.set_has_form_tag(form.is_form_element());
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

  if (IsMalformed(form)) {
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
    return !field->host_form_signature() ||
           field->host_form_signature() == form.form_signature();
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
    EncodeFormFieldsForUpload(
        form, UNSAFE_BUFFERS({subform_begin, subform_end}), &uploads.back());
    subform_begin = subform_end;
  }
  return uploads;
}

std::pair<AutofillPageQueryRequest, std::vector<FormSignature>>
EncodeAutofillPageQueryRequest(
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms) {
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
  for (const autofill::FormStructure* form : forms) {
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
  if (!response.ParseFromString(decoded_payload)) {
    return;
  }

  VLOG(1) << "Autofill query response from API was successfully parsed: "
          << response;

  ProcessServerPredictionsQueryResponse(
      response, forms, queried_form_signatures, form_interactions_ukm_logger,
      log_manager);
}

void ProcessServerPredictionsQueryResponse(
    const AutofillQueryResponse& response,
    const std::vector<raw_ptr<FormStructure, VectorExperimental>>& forms,
    const std::vector<FormSignature>& queried_form_signatures,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
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

  // Copy the field types into the actual form.
  for (FormStructure* form : forms) {
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
      field->set_server_predictions({field_suggestion->predictions().begin(),
                                     field_suggestion->predictions().end()});
      if (field_suggestion->has_may_use_prefilled_placeholder()) {
        field->set_may_use_prefilled_placeholder(
            field_suggestion->may_use_prefilled_placeholder());
      }
      if (heuristic_type != field->Type().GetStorableType()) {
        query_response_overrode_heuristics = true;
      }
      if (field_suggestion->has_password_requirements()) {
        field->SetPasswordRequirements(
            field_suggestion->password_requirements());
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

    form->UpdateAutofillCount();
    form->RationalizeFormStructure(form_interactions_ukm_logger, log_manager);

    AssignSections(form->fields());
    // Metrics are intentionally only emitted after the sever response, not when
    // determining heuristic types. This is done to reduce noise in the metrics,
    // since generally only this sectioning result is used.
    LogSectioningMetrics(form->form_signature(), form->fields(),
                         form_interactions_ukm_logger);

    // Log the field type predicted by rationalization.
    // The sections are mapped to consecutive natural numbers starting at 1.
    std::map<Section, size_t> section_id_map;
    for (const auto& field : form->fields()) {
      if (!base::Contains(section_id_map, field->section())) {
        size_t next_section_id = section_id_map.size() + 1;
        section_id_map[field->section()] = next_section_id;
      }
      field->AppendLogEventIfNotRepeated(RationalizationFieldLogEvent{
          .field_type = field->Type().GetStorableType(),
          .section_id = section_id_map[field->section()],
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

}  // namespace autofill
