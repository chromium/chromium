// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_ai/core/browser/suggestion/autofill_ai_suggestions.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/entity_instance.h"
#include "components/autofill/core/browser/data_model/entity_type.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_ai {

namespace {

using autofill::AttributeInstance;
using autofill::AttributeType;
using autofill::AutofillField;
using autofill::FieldGlobalId;

constexpr int kNumberFieldsToShowInSuggestionLabel = 2;

// TODO(crbug.com/364808228): Remove.
constexpr autofill::DenseSet<autofill::FieldFillingSkipReason>
    kIgnorableSkipReasons = {
        autofill::FieldFillingSkipReason::kNotInFilledSection};

// Checks if the cached predictions for a given `form` and Autofill profile have
// at least one matching autofill suggestion for the specified `field_type`.
bool CacheHasMatchingAutofillSuggestion(
    AutofillAiClient& client,
    const AutofillAiModelExecutor::PredictionsByGlobalId& cache,
    const autofill::FormData& form,
    const std::string& autofill_profile_guid,
    autofill::FieldType field_type) {
  autofill::FormStructure* form_structure =
      client.GetCachedFormStructure(form.global_id());
  if (!form_structure) {
    return false;
  }
  for (const std::unique_ptr<AutofillField>& autofill_field :
       form_structure->fields()) {
    // Skip fields that aren't focusable because they wouldn't be filled
    // anyways.
    if (!autofill_field->IsFocusable()) {
      continue;
    }
    if (autofill_field->Type().GetStorableType() == field_type) {
      const std::u16string normalized_autofill_filling_value =
          autofill::NormalizeValue(
              client.GetAutofillNameFillingValue(autofill_profile_guid,
                                                 field_type, *autofill_field),
              /*keep_white_space=*/false);
      if (normalized_autofill_filling_value.empty() ||
          !cache.contains(autofill_field->global_id())) {
        continue;
      }
      const std::u16string normalized_improved_prediction =
          autofill::NormalizeValue(cache.at(autofill_field->global_id()).value,
                                   /*keep_white_space=*/false);
      if (normalized_improved_prediction == normalized_autofill_filling_value) {
        return true;
      }
    }
  }

  return false;
}

// Maps cached field global ids to their predicted field values.
base::flat_map<FieldGlobalId, std::u16string> GetValuesToFill(
    const AutofillAiModelExecutor::PredictionsByGlobalId& cache) {
  std::vector<std::pair<FieldGlobalId, std::u16string>> values_to_fill;
  for (const auto& [field_global_id, prediction] : cache) {
    if (!prediction.is_focusable) {
      continue;
    }
    values_to_fill.emplace_back(field_global_id, prediction.value);
  }
  return values_to_fill;
}

// Creates a full form filling suggestion that will be displayed first in the
// sub popup.
autofill::Suggestion CreateFillAllSuggestion(
    const autofill::Suggestion::AutofillAiPayload& payload) {
  autofill::Suggestion fill_all_suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_ALL_MAIN_TEXT),
      autofill::SuggestionType::kFillAutofillAi);
  fill_all_suggestion.payload = payload;
  return fill_all_suggestion;
}

// Creates a child suggestion for `suggestion` given `prediction` and adds it to
// its list of children in case it didn't exist before. Returns true if a new
// child suggestion was added and false otherwise.
void AddChildFillingSuggestion(
    autofill::Suggestion& suggestion,
    const AutofillAiModelExecutor::Prediction& prediction) {
  const std::u16string& value_to_fill = prediction.select_option_text
                                            ? *prediction.select_option_text
                                            : prediction.value;
  autofill::Suggestion child_suggestion(
      value_to_fill, autofill::SuggestionType::kFillAutofillAi);
  child_suggestion.payload = autofill::Suggestion::ValueToFill(value_to_fill);
  child_suggestion.labels = {{autofill::Suggestion::Text(prediction.label)}};

  // Ensure that a similar child suggestion was not added before, as this would
  // create unnecessary UI noise.
  if (std::ranges::find_if(
          suggestion.children,
          [&child_suggestion](const autofill::Suggestion& previous_child) {
            return previous_child.main_text == child_suggestion.main_text &&
                   previous_child.labels == child_suggestion.labels;
          }) == suggestion.children.end()) {
    suggestion.children.push_back(std::move(child_suggestion));
  }
}

// Adds a label to `suggestion` indicating which fields will be filled,
// including the first `kNumberFieldsToShowInSuggestionLabel` field labels and
// appending "& N more field(s)" if there are additional fields.
void AddLabelToFillingSuggestion(autofill::Suggestion& suggestion) {
  std::u16string label =
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_LABEL_TEXT) +
      u" ";
  size_t num_valid_labels = 0;
  for (const autofill::Suggestion& child_suggestion : suggestion.children) {
    if (child_suggestion.type == autofill::SuggestionType::kFillAutofillAi &&
        !child_suggestion.labels.empty() &&
        !child_suggestion.labels.front().empty()) {
      if (num_valid_labels > 0 &&
          num_valid_labels < kNumberFieldsToShowInSuggestionLabel) {
        label += l10n_util::GetStringUTF16(
            IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_LABEL_SEPARATOR);
      }
      if (num_valid_labels < kNumberFieldsToShowInSuggestionLabel) {
        label += child_suggestion.labels.front().front().value;
      }
      ++num_valid_labels;
    }
  }
  if (num_valid_labels > kNumberFieldsToShowInSuggestionLabel) {
    // When more than `kNumberFieldsToShowInSuggestionLabel` are filled, include
    // the "& More".
    size_t number_of_more_fields_to_fill =
        num_valid_labels - kNumberFieldsToShowInSuggestionLabel;
    const std::u16string more_fields_label_substr =
        number_of_more_fields_to_fill > 1
            ? l10n_util::GetStringFUTF16(
                  IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_SUGGESTION_AND_N_MORE_FIELDS,
                  base::NumberToString16(number_of_more_fields_to_fill))
            : l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FILL_SUGGESTION_AND_ONE_MORE_FIELD);
    label = base::StrCat({label, u" ", more_fields_label_substr});
  }
  suggestion.labels = {{autofill::Suggestion::Text(label)}};
}

autofill::Suggestion CreateEditAutofillAiData() {
  autofill::Suggestion edit_suggestion;
  edit_suggestion.type = autofill::SuggestionType::kEditAutofillAiData;
  edit_suggestion.icon = autofill::Suggestion::Icon::kEdit;
  edit_suggestion.main_text = autofill::Suggestion::Text(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_EDIT_INFORMATION_SUGGESTION_MAIN_TEXT),
      autofill::Suggestion::Text::IsPrimary(true));
  return edit_suggestion;
}

autofill::Suggestion CreateFeedbackSuggestion() {
  autofill::Suggestion feedback_suggestion(
      autofill::SuggestionType::kAutofillAiFeedback);
  feedback_suggestion.acceptability =
      autofill::Suggestion::Acceptability::kUnacceptable;
  feedback_suggestion.voice_over = base::JoinString(
      {
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_DETAILS),
          l10n_util::GetStringFUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_TEXT,
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_SUGGESTION_MANAGE_LINK_A11Y_HINT)),
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_FEEDBACK_SUGGESTION_FEEDBACK_BUTTONS_A11Y_HINT),
      },
      u" ");
  feedback_suggestion.highlight_on_select = false;
  return feedback_suggestion;
}

// Creates suggestions shown when retrieving prediction improvements wasn't
// successful or there's nothing to fill (not even by Autofill or Autocomplete).
std::vector<autofill::Suggestion> CreateErrorOrNoInfoSuggestions(
    int message_id) {
  autofill::Suggestion error_suggestion(
      autofill::SuggestionType::kAutofillAiError);
  error_suggestion.main_text = autofill::Suggestion::Text(
      l10n_util::GetStringUTF16(message_id),
      autofill::Suggestion::Text::IsPrimary(true),
      autofill::Suggestion::Text::ShouldTruncate(true));
  error_suggestion.highlight_on_select = false;
  error_suggestion.acceptability =
      autofill::Suggestion::Acceptability::kUnacceptable;
  return {error_suggestion,
          autofill::Suggestion(autofill::SuggestionType::kSeparator),
          CreateFeedbackSuggestion()};
}

// Returns suggestions whose set of fields and values to be filled are not
// subsets of another.
std::vector<autofill::Suggestion> DedupeFillingSuggestions(
    std::vector<autofill::Suggestion> suggestions) {
  // Returns -1 if the filling payload of `suggestion_a` is a proper subset of
  // the one from `suggestion_b`. Returns 0 if the filling payload of
  // `suggestion_a` is identical to the one from `suggestion_b`. Returns 1
  // otherwise.
  auto check_suggestions_filling_payload_subset_status =
      [](const autofill::Suggestion& suggestion_a,
         const autofill::Suggestion& suggestion_b) {
        const autofill::Suggestion::AutofillAiPayload* payload_a =
            absl::get_if<autofill::Suggestion::AutofillAiPayload>(
                &suggestion_a.payload);
        CHECK(payload_a);
        const autofill::Suggestion::AutofillAiPayload* payload_b =
            absl::get_if<autofill::Suggestion::AutofillAiPayload>(
                &suggestion_b.payload);
        CHECK(payload_b);

        for (auto& [field_global_id, value_to_fill] :
             payload_a->values_to_fill) {
          if (!payload_b->values_to_fill.contains(field_global_id) ||
              value_to_fill != payload_b->values_to_fill.at(field_global_id)) {
            return 1;
          }
        }

        return payload_b->values_to_fill.size() >
                       payload_a->values_to_fill.size()
                   ? -1
                   : 0;
      };

  // Remove those that are subsets of some other suggestion.
  std::vector<autofill::Suggestion> deduped_filling_suggestions;
  std::set<size_t> duplicated_filling_payloads_to_skip;
  for (size_t i = 0; i < suggestions.size(); i++) {
    if (duplicated_filling_payloads_to_skip.contains(i)) {
      continue;
    }
    bool is_proper_subset_of_another_suggestion = false;
    for (size_t j = 0; j < suggestions.size(); j++) {
      if (i == j) {
        continue;
      }

      int subset_status = check_suggestions_filling_payload_subset_status(
          suggestions[i], suggestions[j]);
      if (subset_status == -1) {
        is_proper_subset_of_another_suggestion = true;
      } else if (subset_status == 0) {
        duplicated_filling_payloads_to_skip.insert(j);
      }
    }
    if (!is_proper_subset_of_another_suggestion) {
      deduped_filling_suggestions.push_back(suggestions[i]);
    }
  }

  return deduped_filling_suggestions;
}

}  // namespace

// Returns true if the type of `autofill_suggestion` should not be added to
// prediction improvements or if `autofill_suggestion` likely matches the cached
// prediction improvements.
bool ShouldSkipAutofillSuggestion(
    AutofillAiClient& client,
    const AutofillAiModelExecutor::PredictionsByGlobalId& cache,
    const autofill::FormData& form,
    const autofill::Suggestion& autofill_suggestion) {
  if (autofill_suggestion.type != autofill::SuggestionType::kAddressEntry &&
      autofill_suggestion.type !=
          autofill::SuggestionType::kAddressFieldByFieldFilling) {
    return true;
  }
  const std::string autofill_profile_guid =
      autofill_suggestion
          .GetPayload<autofill::Suggestion::AutofillProfilePayload>()
          .guid.value();
  if (autofill_profile_guid.empty()) {
    return true;
  }

  return CacheHasMatchingAutofillSuggestion(client, cache, form,
                                            autofill_profile_guid,
                                            autofill::FieldType::NAME_FIRST) &&
         CacheHasMatchingAutofillSuggestion(client, cache, form,
                                            autofill_profile_guid,
                                            autofill::FieldType::NAME_LAST);
}

std::vector<autofill::Suggestion> CreateTriggerSuggestions() {
  autofill::Suggestion retrieve_suggestion(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_TRIGGER_SUGGESTION_MAIN_TEXT),
      autofill::SuggestionType::kRetrieveAutofillAi);
  retrieve_suggestion.icon = autofill::Suggestion::Icon::kAutofillAi;
  return {retrieve_suggestion};
}

std::vector<autofill::Suggestion> CreateLoadingSuggestions() {
  autofill::Suggestion loading_suggestion(
      autofill::SuggestionType::kAutofillAiLoadingState);
  loading_suggestion.trailing_icon = autofill::Suggestion::Icon::kAutofillAi;
  loading_suggestion.acceptability =
      autofill::Suggestion::Acceptability::kUnacceptable;
  return {loading_suggestion};
}

std::vector<autofill::Suggestion> CreateFillingSuggestionsV2(
    const autofill::FormStructure& form,
    FieldGlobalId field_global_id,
    base::span<const autofill::EntityInstance> entities) {
  const autofill::AutofillField* autofill_field =
      form.GetFieldById(field_global_id);
  CHECK(autofill_field);

  std::optional<AttributeType> triggering_field_attribute_type =
      AttributeType::FromFieldType(
          autofill_field->GetAutofillAiServerTypePredictions());
  // The triggering field should be of `FieldTypeGroup::kAutofillAi`
  // type and therefore mapping it to an `AttributeType` should always
  // return a value.
  CHECK(triggering_field_attribute_type);

  std::vector<autofill::Suggestion> suggestions;
  for (const autofill::EntityInstance& entity : entities) {
    //  Only entities that match the triggering field entity should be used to
    //  generate suggestions.
    if (entity.type() != triggering_field_attribute_type->entity_type()) {
      continue;
    }
    base::optional_ref<const AttributeInstance> attribute_for_triggering_field =
        entity.attribute(*triggering_field_attribute_type);
    // Do not create suggestion if the triggering field cannot be filled.
    if (!attribute_for_triggering_field) {
      continue;
    }
    // TODO(crbug.com/389629573): Handle label generation.
    suggestions.emplace_back(
        base::UTF8ToUTF16(attribute_for_triggering_field->value()),
        autofill::SuggestionType::kFillAutofillAi);

    std::vector<std::pair<FieldGlobalId, std::u16string>> values_to_fill;
    for (const std::unique_ptr<AutofillField>& field : form.fields()) {
      // Only fill fields that match the triggering field section.
      if (field->section() != autofill_field->section()) {
        continue;
      }
      autofill::FieldType prediction_for_field_type =
          field->GetAutofillAiServerTypePredictions();

      std::optional<AttributeType> field_attribute_type =
          AttributeType::FromFieldType(prediction_for_field_type);
      // Only fields that match the triggering field entity should be used to
      // generate suggestions.
      if (!field_attribute_type ||
          triggering_field_attribute_type->entity_type() !=
              field_attribute_type->entity_type()) {
        continue;
      }

      base::optional_ref<const AttributeInstance> attribute =
          entity.attribute(*field_attribute_type);
      if (!attribute) {
        continue;
      }

      values_to_fill.emplace_back(field->global_id(),
                                  base::UTF8ToUTF16(attribute->value()));
    }
    auto payload = autofill::Suggestion::AutofillAiPayload(
        values_to_fill, kIgnorableSkipReasons);
    suggestions.back().payload = payload;
  }

  return DedupeFillingSuggestions(std::move(suggestions));
}

std::vector<autofill::Suggestion> CreateFillingSuggestions(
    AutofillAiClient& client,
    const AutofillAiModelExecutor::PredictionsByGlobalId& cache,
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    const std::vector<autofill::Suggestion>& autofill_suggestions) {
  CHECK(cache.contains(field.global_id()));
  const AutofillAiModelExecutor::Prediction& prediction =
      cache.at(field.global_id());
  autofill::Suggestion suggestion(prediction.value,
                                  autofill::SuggestionType::kFillAutofillAi);
  auto payload = autofill::Suggestion::AutofillAiPayload(GetValuesToFill(cache),
                                                         kIgnorableSkipReasons);
  suggestion.payload = payload;
  suggestion.icon = autofill::Suggestion::Icon::kAutofillAi;

  // Add a `kFillAutofillAi` suggestion with a separator to
  // `suggestion.children` before the field-by-field filling entries.
  suggestion.children.emplace_back(CreateFillAllSuggestion(payload));
  suggestion.children.emplace_back(autofill::SuggestionType::kSeparator);

  // Add the child suggestion for the triggering field on top, then for the
  // remaining fields in no particular order.
  AddChildFillingSuggestion(suggestion, prediction);
  for (const auto& [child_field_global_id, child_prediction] : cache) {
    // Only add a child suggestion if the field is not the triggering field,
    // the value to fill is not empty and the field is focusable.
    if (child_field_global_id != field.global_id() &&
        !child_prediction.value.empty() && child_prediction.is_focusable) {
      AddChildFillingSuggestion(suggestion, child_prediction);
    }
  }
  AddLabelToFillingSuggestion(suggestion);

  suggestion.children.emplace_back(autofill::SuggestionType::kSeparator);
  suggestion.children.emplace_back(CreateEditAutofillAiData());

  // TODO(crbug.com/365512352): Figure out how to handle Undo suggestion.
  std::vector<autofill::Suggestion> filling_suggestions = {suggestion};
  for (const autofill::Suggestion& autofill_suggestion : autofill_suggestions) {
    if (ShouldSkipAutofillSuggestion(client, cache, form,
                                     autofill_suggestion)) {
      continue;
    }
    filling_suggestions.push_back(autofill_suggestion);
  }
  filling_suggestions.emplace_back(autofill::SuggestionType::kSeparator);
  filling_suggestions.emplace_back(CreateFeedbackSuggestion());
  return filling_suggestions;
}

std::vector<autofill::Suggestion> CreateErrorSuggestions() {
  return CreateErrorOrNoInfoSuggestions(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_ERROR_POPUP_MAIN_TEXT);
}

std::vector<autofill::Suggestion> CreateNoInfoSuggestions() {
  return CreateErrorOrNoInfoSuggestions(
      IDS_AUTOFILL_PREDICTION_IMPROVEMENTS_NO_INFO_POPUP_MAIN_TEXT);
}

}  // namespace autofill_ai
